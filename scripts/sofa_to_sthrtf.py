#!/usr/bin/env python3
"""Convert a SimpleFreeFieldHRIR SOFA file to SpaceTrace native .sthrtf v1.

This is a release/build tool, not a runtime dependency. It keeps file-format
knowledge outside the renderer and packages the original HRIRs plus an optional
non-destructive common dataset compensation IR.
"""
from __future__ import annotations

import argparse
import hashlib
import math
from pathlib import Path
import struct
import sys
import wave

import h5py
import numpy as np

MAGIC = b"STHRTF1\0"
VERSION = 2
ENDIAN = 0x01020304
FLAG_COMPENSATION = 1


def text_attr(obj, name: str, default: str = "") -> str:
    v = obj.attrs.get(name, default)
    if isinstance(v, bytes):
        return v.decode("utf-8", "replace")
    if isinstance(v, np.ndarray):
        if v.size == 1:
            v = v.reshape(-1)[0]
            if isinstance(v, bytes):
                return v.decode("utf-8", "replace")
        return ", ".join(map(str, v.reshape(-1).tolist()))
    return str(v)


def get_node(f: h5py.File, dotted: str):
    if dotted in f:
        return f[dotted]
    slash = dotted.replace(".", "/")
    if slash in f:
        return f[slash]
    raise KeyError(dotted)


def optional_node(f: h5py.File, dotted: str):
    try:
        return get_node(f, dotted)
    except KeyError:
        return None


def normalise_ir_shape(a: np.ndarray) -> np.ndarray:
    a = np.asarray(a, dtype=np.float32)
    # SimpleFreeFieldHRIR is normally M,R,N. Accept singleton emitter axes too.
    while a.ndim > 3:
        singleton = next((i for i, n in enumerate(a.shape) if n == 1), None)
        if singleton is None:
            raise ValueError(f"unsupported Data.IR shape {a.shape}")
        a = np.squeeze(a, axis=singleton)
    if a.ndim != 3:
        raise ValueError(f"unsupported Data.IR shape {a.shape}; expected M,R,N")
    if a.shape[1] != 2:
        # Some writers put receivers first.
        if a.shape[0] == 2:
            a = np.transpose(a, (1, 0, 2))
        else:
            raise ValueError(f"Data.IR must contain exactly two receivers, got {a.shape}")
    return np.ascontiguousarray(a, dtype=np.float32)


def source_positions_to_canonical(node, count: int) -> np.ndarray:
    p = np.asarray(node[...], dtype=np.float64)
    p = np.squeeze(p)
    if p.ndim != 2 or p.shape[0] != count or p.shape[1] != 3:
        raise ValueError(f"SourcePosition shape {p.shape} does not match M={count}")

    kind = text_attr(node, "Type", "spherical").strip().lower()
    units = text_attr(node, "Units", "degree, degree, meter").lower()

    if kind == "cartesian":
        x, y, z = p[:, 0], p[:, 1], p[:, 2]
        # SOFA Cartesian: x front, y left, z up. This exactly matches the
        # SpaceTrace azimuth sign convention (+90 is listener-left).
        radius = np.sqrt(x*x + y*y + z*z)
        az = np.degrees(np.arctan2(y, x))
        el = np.degrees(np.arctan2(z, np.sqrt(x*x + y*y)))
        p = np.column_stack((az, el, radius))
    elif kind == "spherical":
        if "radian" in units:
            p[:, 0:2] = np.degrees(p[:, 0:2])
    else:
        raise ValueError(f"unsupported SourcePosition Type={kind!r}")

    if "millimeter" in units or "millimetre" in units or " mm" in units:
        p[:, 2] /= 1000.0
    elif "centimeter" in units or "centimetre" in units or " cm" in units:
        p[:, 2] /= 100.0

    p[:, 0] = ((p[:, 0] + 180.0) % 360.0) - 180.0
    if np.any(~np.isfinite(p)) or np.any(p[:, 2] <= 0):
        raise ValueError("SourcePosition contains invalid values")
    return p


def delays_in_samples(node, count: int, sample_rate: float) -> np.ndarray:
    if node is None:
        return np.zeros((count, 2), dtype=np.float64)
    d = np.asarray(node[...], dtype=np.float64)
    d = np.squeeze(d)
    if d.ndim == 1 and d.shape[0] == 2:
        d = np.tile(d[None, :], (count, 1))
    elif d.ndim == 2 and d.shape == (1, 2):
        d = np.tile(d, (count, 1))
    elif d.ndim != 2 or d.shape != (count, 2):
        raise ValueError(f"unsupported Data.Delay shape {d.shape}")

    units = text_attr(node, "Units", "second").lower()
    if "sample" not in units:
        # SOFA convention expresses Data.Delay in seconds unless explicitly stated otherwise.
        d *= sample_rate
    return d




def next_pow2(n: int) -> int:
    return 1 << max(0, int(n - 1).bit_length())


def correlation_lag(a: np.ndarray, b: np.ndarray, quarter_sample: bool = True) -> float:
    """Return the lag in samples by which ``a`` trails ``b``.

    Compute the linear cross-correlation with an FFT so long official HRIRs do
    not make dataset preparation needlessly quadratic. Parabolic interpolation
    around the peak is retained, then the build representation is quantised to
    the quarter-sample grid.
    """
    a = np.asarray(a, dtype=np.float64)
    b = np.asarray(b, dtype=np.float64)
    full = len(a) + len(b) - 1
    nfft = next_pow2(full)
    corr = np.fft.irfft(np.fft.rfft(a, nfft) * np.fft.rfft(b[::-1], nfft), nfft)[:full]
    i = int(np.argmax(corr))
    lag = float(i - (len(b) - 1))
    if 0 < i < len(corr) - 1:
        ym1, y0, yp1 = float(corr[i - 1]), float(corr[i]), float(corr[i + 1])
        denom = ym1 - 2.0 * y0 + yp1
        if abs(denom) > 1.0e-20:
            frac = 0.5 * (ym1 - yp1) / denom
            lag += float(np.clip(frac, -0.5, 0.5))
    if quarter_sample:
        lag = round(lag * 4.0) / 4.0
    return lag


def minimum_phase_ir(ir: np.ndarray, taps: int) -> np.ndarray:
    """Minimum-phase impulse with the input magnitude response."""
    ir = np.asarray(ir, dtype=np.float64)
    if taps <= 0:
        raise ValueError("minimum-phase tap count must be positive")
    nfft = next_pow2(max(8192, len(ir) * 16, taps * 16))
    mag = np.abs(np.fft.rfft(ir, nfft))
    logmag = np.log(np.maximum(mag, 1.0e-12))
    cep = np.fft.irfft(logmag, nfft)
    folded = np.zeros(nfft, dtype=np.float64)
    folded[0] = cep[0]
    folded[1:nfft // 2] = 2.0 * cep[1:nfft // 2]
    folded[nfft // 2] = cep[nfft // 2]
    spectrum = np.exp(np.fft.rfft(folded))
    mp = np.fft.irfft(spectrum, nfft)[:taps]
    if taps >= 16:
        fade = min(16, taps)
        mp[-fade:] *= np.cos(np.linspace(0.0, np.pi / 2.0, fade)) ** 2
    return np.asarray(mp, dtype=np.float32)


def minimum_phase_set(ir: np.ndarray, delays: np.ndarray, taps: int) -> tuple[np.ndarray, np.ndarray, float]:
    """Convert all HRIRs to minimum phase while preserving onset and ITD.

    The left/right delay pair is adjusted so the broadband interaural lag of
    the original HRIR pair survives removal of excess phase. A common global
    base delay is then removed because it carries no localisation information.
    """
    m = ir.shape[0]
    out = np.empty((m, 2, taps), dtype=np.float32)
    out_delay = np.empty((m, 2), dtype=np.float64)
    for i in range(m):
        raw_l = np.asarray(ir[i, 0], dtype=np.float64)
        raw_r = np.asarray(ir[i, 1], dtype=np.float64)
        mp_l = minimum_phase_ir(raw_l, taps)
        mp_r = minimum_phase_ir(raw_r, taps)
        out[i, 0] = mp_l
        out[i, 1] = mp_r

        onset_l = float(delays[i, 0]) + correlation_lag(raw_l, mp_l)
        onset_r = float(delays[i, 1]) + correlation_lag(raw_r, mp_r)
        target_itd = (float(delays[i, 1]) - float(delays[i, 0])) + correlation_lag(raw_r, raw_l)
        residual_itd = correlation_lag(mp_r, mp_l)
        correction = target_itd - residual_itd
        mean_onset = 0.5 * (onset_l + onset_r)
        out_delay[i, 0] = mean_onset - 0.5 * correction
        out_delay[i, 1] = mean_onset + 0.5 * correction

    base = float(np.min(out_delay))
    out_delay -= base
    out_delay = np.round(out_delay * 4.0) / 4.0
    return out, out_delay, base


def read_wave_mono(path: Path) -> tuple[np.ndarray, float]:
    data = path.read_bytes()
    if len(data) < 44 or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError(f"{path} is not a RIFF/WAVE file")
    pos = 12
    fmt = None
    raw = None
    while pos + 8 <= len(data):
        cid = data[pos:pos+4]
        size = struct.unpack_from("<I", data, pos+4)[0]
        chunk = data[pos+8:pos+8+size]
        if cid == b"fmt ":
            fmt = chunk
        elif cid == b"data":
            raw = chunk
        pos += 8 + size + (size & 1)
    if fmt is None or raw is None or len(fmt) < 16:
        raise ValueError(f"{path} has incomplete WAVE chunks")
    tag, channels, sr, _, _, bits = struct.unpack_from("<HHIIHH", fmt, 0)
    if channels < 1:
        raise ValueError("WAVE has no channels")
    if tag == 3 and bits == 32:
        a = np.frombuffer(raw, dtype="<f4")
    elif tag == 1 and bits == 16:
        a = np.frombuffer(raw, dtype="<i2").astype(np.float32) / 32768.0
    elif tag == 1 and bits == 32:
        a = np.frombuffer(raw, dtype="<i4").astype(np.float32) / 2147483648.0
    elif tag == 1 and bits == 24:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        x = b[:,0].astype(np.int32) | (b[:,1].astype(np.int32)<<8) | (b[:,2].astype(np.int32)<<16)
        x = np.where(x & 0x800000, x - 0x1000000, x)
        a = x.astype(np.float32) / 8388608.0
    else:
        raise ValueError(f"unsupported WAVE format tag={tag}, bits={bits}")
    if a.size % channels:
        raise ValueError("WAVE data size is not divisible by channel count")
    a = a.reshape(-1, channels)
    if channels > 1 and not np.allclose(a[:, 0], a[:, 1], atol=1e-6, rtol=1e-5):
        raise ValueError("dataset compensation must be mono or identical stereo")
    return np.asarray(a[:, 0], dtype=np.float32), float(sr)


def write_string(f, s: str):
    b = s.encode("utf-8")
    f.write(struct.pack("<I", len(b)))
    f.write(b)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("source_sofa", type=Path)
    ap.add_argument("output_sthrtf", type=Path)
    ap.add_argument("--name", default="")
    ap.add_argument("--subject", default="")
    ap.add_argument("--database", default="")
    ap.add_argument("--source-url", default="")
    ap.add_argument("--licence-file", type=Path)
    ap.add_argument("--attribution", default="")
    ap.add_argument("--compensation", type=Path)
    ap.add_argument("--compensation-name", default="")
    ap.add_argument("--compensation-version", default="")
    ap.add_argument("--minimum-phase-taps", type=int, default=0,
                    help="Convert every HRIR to minimum phase with this many taps and preserve onset/ITD in delays")
    ap.add_argument("--processing-description", default="")
    args = ap.parse_args()

    source_bytes = args.source_sofa.read_bytes()
    source_sha = hashlib.sha256(source_bytes).hexdigest()

    with h5py.File(args.source_sofa, "r") as sofa:
        ir_node = get_node(sofa, "Data.IR")
        ir = normalise_ir_shape(ir_node[...])
        m, receivers, n = ir.shape
        sr_node = get_node(sofa, "Data.SamplingRate")
        sr_values = np.asarray(sr_node[...], dtype=np.float64).reshape(-1)
        if sr_values.size != 1:
            raise ValueError("SpaceTrace v0.1 requires one sampling rate per SOFA file")
        sample_rate = float(sr_values[0])
        positions = source_positions_to_canonical(get_node(sofa, "SourcePosition"), m)
        delays = delays_in_samples(optional_node(sofa, "Data.Delay"), m, sample_rate)

        processing = args.processing_description.strip()
        if args.minimum_phase_taps:
            ir, delays, removed_base_delay = minimum_phase_set(ir, delays, args.minimum_phase_taps)
            n = ir.shape[2]
            if not processing:
                processing = (f"Minimum-phase {n}-tap HRIRs generated from the official SOFA data; "
                              f"broadband onset and interaural delay preserved on a quarter-sample grid; "
                              f"common base delay {removed_base_delay:.2f} samples removed")
        elif not processing:
            processing = "Raw SOFA HRIR samples; source coordinates normalized to SpaceTrace canonical convention"

        name = args.name or text_attr(sofa, "Title", args.source_sofa.stem)
        subject = args.subject or text_attr(sofa, "ListenerShortName", "")
        database = args.database or text_attr(sofa, "DatabaseName", "")
        licence = args.licence_file.read_text(encoding="utf-8") if args.licence_file else text_attr(sofa, "License", "")
        attribution = args.attribution or text_attr(sofa, "References", "")

    comp = np.empty(0, dtype=np.float32)
    comp_sr = 0.0
    if args.compensation:
        comp, comp_sr = read_wave_mono(args.compensation)
        if abs(comp_sr - sample_rate) > 1e-6:
            raise ValueError(f"compensation sample rate {comp_sr} != SOFA sample rate {sample_rate}")

    flags = FLAG_COMPENSATION if comp.size else 0
    args.output_sthrtf.parent.mkdir(parents=True, exist_ok=True)
    with args.output_sthrtf.open("wb") as f:
        f.write(MAGIC)
        f.write(struct.pack("<III d II d I", VERSION, ENDIAN, flags, sample_rate, m, n, comp_sr, comp.size))
        strings = [
            name, subject, "SOFA", args.source_sofa.name, args.source_url,
            database, licence, attribution, source_sha, processing,
            args.compensation_name if comp.size else "",
            args.compensation_version if comp.size else "",
        ]
        for s in strings:
            write_string(f, s)
        for i in range(m):
            az, el, dist = positions[i]
            ld, rd = delays[i]
            f.write(struct.pack("<fffff", float(az), float(el), float(dist), float(ld), float(rd)))
            f.write(np.asarray(ir[i, 0], dtype="<f4").tobytes())
            f.write(np.asarray(ir[i, 1], dtype="<f4").tobytes())
        if comp.size:
            f.write(np.asarray(comp, dtype="<f4").tobytes())

    print(f"SpaceTrace package: {args.output_sthrtf}")
    print(f"  source SHA-256: {source_sha}")
    print(f"  sample rate: {sample_rate:g} Hz")
    print(f"  measurements: {m}")
    print(f"  HRIR length: {n}")
    print(f"  processing: {processing}")
    print(f"  compensation samples: {comp.size}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"SpaceTrace SOFA conversion failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
