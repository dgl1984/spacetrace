#!/usr/bin/env python3
"""Shared native-head analysis helpers used by SpaceTrace preparation tools."""
from __future__ import annotations

from pathlib import Path
import struct

from tool_common import ToolError, require_file


def require_numpy():
    try:
        import numpy as np
        return np
    except Exception as exc:
        raise ToolError(
            "Missing Python package: numpy",
            hint="Install dependencies with: python -m pip install -r scripts/requirements.txt",
            code=10,
        ) from exc


def _read_string(f) -> str:
    raw = f.read(4)
    if len(raw) != 4:
        raise ToolError("Native head package is truncated while reading metadata.", hint="Re-convert the head from the preserved SOFA.", code=22)
    n = struct.unpack("<I", raw)[0]
    data = f.read(n)
    if len(data) != n:
        raise ToolError("Native head package is truncated while reading metadata.", hint="Re-convert the head from the preserved SOFA.", code=22)
    return data.decode("utf-8", "replace")


def read_native_measurements(path: Path) -> tuple[float, list[dict]]:
    """Read only the measurement fields needed for offline analysis."""
    np = require_numpy()
    path = require_file(path, "Native head file")
    try:
        with path.open("rb") as f:
            if f.read(8) != b"STHRTF1\0":
                raise ToolError(f"Not a SpaceTrace native head package: {path}", hint="Run convert_sofa_head.py first.", code=22)
            header = f.read(12)
            if len(header) != 12:
                raise ToolError("Native head package header is truncated.", hint="Re-convert the head from the preserved SOFA.", code=22)
            version, endian, _flags = struct.unpack("<III", header)
            if version not in (1, 2) or endian != 0x01020304:
                raise ToolError(f"Unsupported native head version/endian: {version}/{endian:#x}", hint="Rebuild this head with the current SpaceTrace converter.", code=22)
            raw = f.read(16)
            if len(raw) != 16:
                raise ToolError("Native head package dimensions are truncated.", hint="Re-convert the head from the preserved SOFA.", code=22)
            sample_rate = struct.unpack("<d", raw[:8])[0]
            count, taps = struct.unpack("<II", raw[8:16])
            if len(f.read(12)) != 12:
                raise ToolError("Native head package compensation header is truncated.", hint="Re-convert the head from the preserved SOFA.", code=22)
            # v1 has nine HRTF metadata strings; v2 adds processing. Then two compensation strings.
            for _ in range(9):
                _read_string(f)
            if version >= 2:
                _read_string(f)
            _read_string(f); _read_string(f)
            rows = []
            for _ in range(count):
                mh = f.read(20)
                if len(mh) != 20:
                    raise ToolError("Native head package measurement header is truncated.", hint="Re-convert the head from the preserved SOFA.", code=22)
                az, el, dist, left_delay, right_delay = struct.unpack("<fffff", mh)
                lb = f.read(taps * 4); rb = f.read(taps * 4)
                if len(lb) != taps * 4 or len(rb) != taps * 4:
                    raise ToolError("Native head package HRIR data is truncated.", hint="Re-convert the head from the preserved SOFA.", code=22)
                rows.append({
                    "azimuth": float(az), "elevation": float(el), "distance": float(dist),
                    "left_delay": float(left_delay), "right_delay": float(right_delay),
                    "left": np.frombuffer(lb, dtype="<f4").astype(np.float64),
                    "right": np.frombuffer(rb, dtype="<f4").astype(np.float64),
                })
            return float(sample_rate), rows
    except ToolError:
        raise
    except OSError as exc:
        raise ToolError(f"Could not read native head package {path}: {exc}", hint="Check file permissions or re-convert from the SOFA.", code=22) from exc


def nearest_front_center(rows: list[dict], *, tolerance_deg: float = 0.5) -> dict:
    if not rows:
        raise ToolError("Native head contains no measurements.", hint="Re-convert from a valid binaural SOFA.", code=22)
    def angular_error(row):
        az = abs(((row["azimuth"] + 180.0) % 360.0) - 180.0)
        el = abs(row["elevation"])
        return az, el
    row = min(rows, key=lambda r: sum(angular_error(r)))
    az_err, el_err = angular_error(row)
    if az_err > tolerance_deg or el_err > tolerance_deg:
        raise ToolError(
            f"No front-center measurement close enough to 0°/0°. Nearest is azimuth {row['azimuth']:+.3f}°, elevation {row['elevation']:+.3f}°.",
            hint="Do not guess a channel calibration from an off-axis measurement. Use a head with a real front-center measurement or raise the tolerance only after inspecting the dataset.",
            code=23,
        )
    return row


def pink_weighted_power(ir, sample_rate: float, low_hz: float = 80.0, high_hz: float = 16000.0) -> float:
    np = require_numpy()
    nyq = sample_rate * 0.5
    high_hz = min(high_hz, nyq * 0.98)
    if not 0.0 < low_hz < high_hz:
        raise ToolError(f"Invalid analysis band {low_hz:g}-{high_hz:g} Hz for {sample_rate:g} Hz audio.", hint="Choose a band below Nyquist.", code=6)
    nfft = 1
    while nfft < max(32768, len(ir) * 16):
        nfft <<= 1
    f = np.fft.rfftfreq(nfft, 1.0 / sample_rate)
    m = (f >= low_hz) & (f <= high_hz)
    H = np.fft.rfft(ir, nfft)[m]
    w = 1.0 / np.maximum(f[m], 1e-12)
    return float(np.sum((np.abs(H) ** 2) * w) / np.sum(w))


def front_channel_calibration(path: Path, *, low_hz: float = 80.0, high_hz: float = 16000.0, tolerance_deg: float = 0.5) -> dict:
    np = require_numpy()
    sample_rate, rows = read_native_measurements(path)
    row = nearest_front_center(rows, tolerance_deg=tolerance_deg)
    lp = pink_weighted_power(row["left"], sample_rate, low_hz, high_hz)
    rp = pink_weighted_power(row["right"], sample_rate, low_hz, high_hz)
    if not np.isfinite(lp) or not np.isfinite(rp) or lp <= 0.0 or rp <= 0.0:
        raise ToolError("Front-center channel energy is invalid or zero.", hint="Inspect the source SOFA and converted head before applying calibration.", code=23)
    left_db = 10.0 * np.log10(lp)
    right_db = 10.0 * np.log10(rp)
    imbalance_db = float(left_db - right_db)
    # Equal and opposite dB adjustments preserve the geometric mean / overall calibration level.
    left_gain_db = -0.5 * imbalance_db
    right_gain_db = +0.5 * imbalance_db
    return {
        "sample_rate": sample_rate,
        "azimuth": row["azimuth"], "elevation": row["elevation"], "distance": row["distance"],
        "left_db": float(left_db), "right_db": float(right_db), "imbalance_db": imbalance_db,
        "left_gain_db": float(left_gain_db), "right_gain_db": float(right_gain_db),
        "low_hz": float(low_hz), "high_hz": float(min(high_hz, sample_rate * 0.49)),
    }


def correction_response_magnitude(correction, correction_sr: float, freqs):
    np = require_numpy()
    nfft = 1
    while nfft < max(32768, len(correction) * 16):
        nfft <<= 1
    H = np.fft.rfft(correction, nfft)
    f = np.fft.rfftfreq(nfft, 1.0 / correction_sr)
    return np.interp(freqs, f, np.abs(H), left=np.abs(H[0]), right=np.abs(H[-1]))


def _nearest_el0(rows: list[dict], azimuth: float) -> dict:
    def score(row):
        da = abs(((row["azimuth"] - azimuth + 180.0) % 360.0) - 180.0)
        return da + abs(row["elevation"]) * 100.0
    return min(rows, key=score)


def head_weighted_correction_delta_db(head_path: Path, correction, correction_sr: float,
                                      *, low_hz: float = 80.0, high_hz: float = 16000.0,
                                      azimuth_step_deg: float = 5.0) -> float:
    """Return corrected-minus-raw pink-weighted power average over elevation zero."""
    np = require_numpy()
    sample_rate, rows = read_native_measurements(head_path)
    nfft = 32768
    f = np.fft.rfftfreq(nfft, 1.0 / sample_rate)
    high = min(high_hz, sample_rate * 0.49)
    band = (f >= low_hz) & (f <= high)
    fb = f[band]
    weights = 1.0 / np.maximum(fb, 1e-12)
    C = correction_response_magnitude(correction, correction_sr, fb)
    raw_powers = []
    corrected_powers = []
    for az in np.arange(-180.0, 180.0, azimuth_step_deg):
        row = _nearest_el0(rows, float(az))
        raw_ears = []
        corr_ears = []
        for ir in (row["left"], row["right"]):
            H = np.fft.rfft(ir, nfft)[band]
            raw_ears.append(np.sum((np.abs(H) ** 2) * weights) / np.sum(weights))
            corr_ears.append(np.sum(((np.abs(H) * C) ** 2) * weights) / np.sum(weights))
        raw_powers.append((raw_ears[0] + raw_ears[1]) * 0.5)
        corrected_powers.append((corr_ears[0] + corr_ears[1]) * 0.5)
    raw_avg = float(np.mean(raw_powers)); corr_avg = float(np.mean(corrected_powers))
    if raw_avg <= 0 or corr_avg <= 0 or not np.isfinite(raw_avg) or not np.isfinite(corr_avg):
        raise ToolError("Head-aware correction normalization produced invalid power.", hint="Verify the head package and correction WAV.", code=20)
    return 10.0 * np.log10(corr_avg / raw_avg)
