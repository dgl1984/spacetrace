#!/usr/bin/env python3
"""Regression for FABIAN correction provenance and fixed level trim.

This is intentionally a package-level measurement, not a renderer rewrite. It
uses exact elevation-zero HRIRs and the shipped correction WAV magnitude to
make sure the fixed trim remains a separate level-matching control.
"""
from pathlib import Path
import json, struct, hashlib
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
HEADS = ROOT / "Heads"
FABIAN_TTM = ROOT / "Docs" / "Correction_Models" / "FABIAN_HATO0_ToneTrace.ttm"
FULL2DEG_TTM = ROOT / "Docs" / "Correction_Models" / "FULL2DEG_KU100_ToneTrace.ttm"


def read_u32(f): return struct.unpack('<I', f.read(4))[0]
def read_string(f): return f.read(read_u32(f)).decode('utf-8', 'replace')


def read_sthrtf(path: Path):
    with path.open('rb') as f:
        if f.read(8) != b'STHRTF1\0': raise AssertionError(f'{path}: bad magic')
        version, endian, flags = struct.unpack('<III', f.read(12))
        if endian != 0x01020304: raise AssertionError(f'{path}: bad endian marker')
        sr = struct.unpack('<d', f.read(8))[0]
        count, ir_len = struct.unpack('<II', f.read(8))
        f.read(12)  # compensation sample rate + length
        for _ in range(9): read_string(f)
        if version >= 2: read_string(f)
        read_string(f); read_string(f)
        rows = []
        for _ in range(count):
            az, el, dist, ld, rd = struct.unpack('<fffff', f.read(20))
            left = np.frombuffer(f.read(ir_len * 4), dtype='<f4').astype(np.float64)
            right = np.frombuffer(f.read(ir_len * 4), dtype='<f4').astype(np.float64)
            rows.append((az, el, left, right))
    return sr, rows


def read_float_wav(path: Path):
    data = path.read_bytes()
    if data[:4] != b'RIFF' or data[8:12] != b'WAVE':
        raise AssertionError(f'{path}: not RIFF/WAVE')
    pos, fmt, payload = 12, None, None
    while pos + 8 <= len(data):
        cid = data[pos:pos+4]; size = struct.unpack_from('<I', data, pos+4)[0]; pos += 8
        chunk = data[pos:pos+size]
        if cid == b'fmt ':
            tag, channels, sr, byte_rate, align, bits = struct.unpack_from('<HHIIHH', chunk, 0)
            fmt = (tag, channels, sr, bits)
        elif cid == b'data': payload = chunk
        pos += size + (size & 1)
    if fmt is None or payload is None: raise AssertionError(f'{path}: missing fmt/data')
    tag, channels, sr, bits = fmt
    if tag != 3 or bits != 32: raise AssertionError(f'{path}: expected 32-bit IEEE float')
    x = np.frombuffer(payload, dtype='<f4').astype(np.float64)
    if channels > 1: x = x.reshape(-1, channels)[:, 0]
    return float(sr), x


def nearest_el0(rows, az):
    def score(row):
        da = abs(((row[0] - az + 180.0) % 360.0) - 180.0)
        return da + abs(row[1]) * 100.0
    return min(rows, key=score)


def correction_response(corr, corr_sr, freqs):
    n = 1
    while n < max(len(corr) * 16, 32768): n *= 2
    H = np.fft.rfft(corr, n)
    f = np.fft.rfftfreq(n, 1.0 / corr_sr)
    return np.interp(freqs, f, np.abs(H), left=np.abs(H[0]), right=np.abs(H[-1]))


def stereo_gain_db(row, sr, corr=None, corr_sr=None, left_gain_db=0.0, right_gain_db=0.0):
    n = 32768
    f = np.fft.rfftfreq(n, 1.0 / sr)
    band = (f >= 80.0) & (f <= 16000.0)
    fb = f[band]
    C = 1.0 if corr is None else correction_response(corr, corr_sr, fb)
    weight = 1.0 / fb  # equal energy per octave / pink-like programme weighting
    ears = []
    for ir, gain_db in ((row[2], left_gain_db), (row[3], right_gain_db)):
        H = np.fft.rfft(ir, n)[band]
        p = (np.abs(H) * C * (10.0 ** (gain_db / 20.0))) ** 2
        ears.append(np.sum(p * weight) / np.sum(weight))
    return 10.0 * np.log10((ears[0] + ears[1]) * 0.5 + 1e-30)


def power_average_db(values):
    return 10.0 * np.log10(np.mean(10.0 ** (np.asarray(values) / 10.0)))


def head_level(folder):
    d = HEADS / folder
    manifest = json.loads((d / 'manifest.json').read_text(encoding='utf-8-sig'))
    sr, rows = read_sthrtf(d / manifest['headFile'])
    corr_sr, corr = read_float_wav(d / manifest['correctionFile'])
    left_gain = float(manifest.get('leftGainDb', 0.0)); right_gain = float(manifest.get('rightGainDb', 0.0))
    corrected = []
    raw = []
    for az in np.arange(-180.0, 180.0, 5.0):
        row = nearest_el0(rows, float(az))
        corrected.append(stereo_gain_db(row, sr, corr, corr_sr, left_gain, right_gain))
        raw.append(stereo_gain_db(row, sr, None, None, left_gain, right_gain))
    # Power average across direction, then apply explicit package trim.
    base = power_average_db(corrected)
    raw_base = power_average_db(raw)
    return (base + float(manifest.get('levelTrimDb', 0.0)), base, raw_base,
            float(manifest.get('levelTrimDb', 0.0)))


def validate_model(ttm_path, folder, label):
    model_f, model_db = [], []
    for line in ttm_path.read_text(encoding='utf-8').splitlines():
        parts = line.split()
        if len(parts) == 3:
            try:
                model_f.append(float(parts[0])); model_db.append(float(parts[1]))
            except ValueError: pass
    model_f = np.asarray(model_f); model_db = np.asarray(model_db)
    sr, corr = read_float_wav(HEADS / folder / 'correction.wav')
    n = 131072; H = np.fft.rfft(corr, n); f = np.fft.rfftfreq(n, 1.0 / sr)
    wav_db = 20.0 * np.log10(np.maximum(np.interp(model_f, f, np.abs(H)), 1e-30))
    m = (model_f >= 80.0) & (model_f <= 16000.0)
    err = wav_db[m] - model_db[m]
    offset = float(np.mean(err)); shape_err = err - offset
    if np.max(np.abs(shape_err)) > 0.03:
        raise AssertionError(f'{label} correction shape no longer matches supplied Tone Trace model; max residual {np.max(np.abs(shape_err)):.4f} dB')
    return offset, float(np.max(np.abs(shape_err)))

def main():
    fab_offset, fab_shape = validate_model(FABIAN_TTM, 'FABIAN_HATO0', 'FABIAN')
    full_offset, full_shape = validate_model(FULL2DEG_TTM, 'KU100_FULL2DEG', 'FULL2DEG')
    ircam, _, _, _ = head_level('IRCAM_1050')
    kemar, _, _, _ = head_level('MIT_KEMAR_Normal')
    fabian, fabian_untrimmed, fabian_raw, fab_trim = head_level('FABIAN_HATO0')
    full, full_untrimmed, full_raw, full_trim = head_level('KU100_FULL2DEG')
    ref_power = 10.0 * np.log10((10.0 ** (ircam / 10.0) + 10.0 ** (kemar / 10.0)) * 0.5)
    for label, effective, untrimmed, raw, trim, expected in [
        ('FABIAN', fabian, fabian_untrimmed, fabian_raw, fab_trim, -9.74),
        ('FULL2DEG', full, full_untrimmed, full_raw, full_trim, -4.13),
    ]:
        correction_delta = untrimmed - raw
        if abs(correction_delta) > 0.05:
            raise AssertionError(f'{label} correction is not level-neutral: corrected-vs-Raw {correction_delta:+.3f} dB')
        delta = effective - ref_power
        if abs(delta) > 0.5:
            raise AssertionError(f'{label} fixed trim misses packaged reference level by {delta:+.2f} dB')
        if abs(trim - expected) > 0.02:
            raise AssertionError(f'{label} fixed trim changed unexpectedly: {trim:+.2f} dB')
        print(f'{label} correction/trim PASS: correction level delta={correction_delta:+.4f} dB, untrimmed={untrimmed:.2f} dB, trim={trim:.2f} dB, effective={effective:.2f} dB, reference={ref_power:.2f} dB, delta={delta:+.2f} dB')
    print(f'Tone Trace shape PASS: FABIAN normalization={fab_offset:+.3f} dB max residual={fab_shape:.4f} dB; FULL2DEG normalization={full_offset:+.3f} dB max residual={full_shape:.4f} dB')

if __name__ == '__main__':
    main()
