#!/usr/bin/env python3
"""Prepare optional Stereo Pair correction/calibration assets for one SpaceTrace head.

The input correction WAVs are the Tone Trace exports measured from the exact Raw
head at center-0 Stereo Pair source positions with Width Offset 0: input Left at
+90 degrees and input Right at 270 degrees. The script applies only constant normalization gains to the
correction IRs, then derives fixed source-balance and pair-level calibration.
"""
from __future__ import annotations

import argparse
import math
from pathlib import Path
import shutil

from head_analysis import read_native_measurements, correction_response_magnitude
from normalize_correction import read_wav, write_float32_mono
from tool_common import ToolError, die, load_json, require_file, sha256_file, write_json


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


def nearest_horizontal(rows: list[dict], azimuth: float, *, tolerance_deg: float = 0.6) -> dict:
    def score(row):
        da = abs(((row["azimuth"] - azimuth + 180.0) % 360.0) - 180.0)
        return da + abs(row["elevation"]) * 1000.0
    row = min(rows, key=score)
    da = abs(((row["azimuth"] - azimuth + 180.0) % 360.0) - 180.0)
    if da > tolerance_deg or abs(row["elevation"]) > tolerance_deg:
        raise ToolError(
            f"No measurement close enough to azimuth {azimuth:g}° / elevation 0°. "
            f"Nearest is {row['azimuth']:+.3f}° / {row['elevation']:+.3f}°.",
            hint="Stereo Pair calibration requires real 90° and 270° horizontal measurements.",
            code=23,
        )
    return row


def weighted_source_power(row: dict, head_sr: float, correction, correction_sr: float,
                          left_gain_db: float, right_gain_db: float,
                          *, low_hz: float, high_hz: float) -> float:
    np = require_numpy()
    nfft = 1
    max_taps = max(len(row["left"]), len(row["right"]), len(correction) if correction is not None else 1)
    while nfft < max(32768, max_taps * 16):
        nfft <<= 1
    f = np.fft.rfftfreq(nfft, 1.0 / head_sr)
    high = min(high_hz, head_sr * 0.49)
    band = (f >= low_hz) & (f <= high)
    fb = f[band]
    weights = 1.0 / np.maximum(fb, 1e-12)
    C = None if correction is None else correction_response_magnitude(correction, correction_sr, fb)
    total = 0.0
    for ir, gain_db in ((row["left"], left_gain_db), (row["right"], right_gain_db)):
        H = np.abs(np.fft.rfft(ir, nfft)[band]) * (10.0 ** (gain_db / 20.0))
        if C is not None:
            H = H * C
        total += float(np.sum((H ** 2) * weights) / np.sum(weights))
    return total


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description=(
            "Prepare SpaceTrace Stereo Pair correction assets. The supplied left/right WAVs are "
            "Tone Trace corrections measured from the exact Raw head at +90° and 270°. Each IR "
            "is normalized with one constant gain, then fixed source-balance and pair-level "
            "calibration are written to the head manifest."
        ),
        epilog=(
            "Example:\n"
            "  python scripts/prepare_stereo_pair.py Heads/MyHead left-90.wav right-270.wav --overwrite\n\n"
            "This does not flatten interaural differences: each correction remains mono/common to "
            "both ears of its source leg. It does not modify correction EQ shape beyond one constant "
            "normalization gain."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument("head_dir", type=Path)
    p.add_argument("left_correction", type=Path, help="Tone Trace correction WAV measured from Raw +90°.")
    p.add_argument("right_correction", type=Path, help="Tone Trace correction WAV measured from Raw 270°.")
    p.add_argument("--low-hz", type=float, default=80.0)
    p.add_argument("--high-hz", type=float, default=16000.0)
    p.add_argument("--version", default="tonetrace-stereo-pair-v1")
    p.add_argument("--overwrite", action="store_true")
    return p


def main() -> int:
    args = parser().parse_args()
    np = require_numpy()
    d = args.head_dir
    if not d.is_dir():
        raise ToolError(f"Head folder not found: {d}", hint="Point to a valid Heads/<name> package folder.", code=3)
    manifest_path = require_file(d / "manifest.json", "manifest.json")
    manifest = load_json(manifest_path)
    head_path = require_file(d / str(manifest.get("headFile", "head.sthrtf")), "Native head file")
    head_sr, rows = read_native_measurements(head_path)
    if not (0.0 < args.low_hz < args.high_hz < head_sr * 0.5):
        raise ToolError(
            f"Invalid analysis band {args.low_hz:g}-{args.high_hz:g} Hz for {head_sr:g} Hz head data.",
            hint="Choose a band below Nyquist.", code=6)

    left_row = nearest_horizontal(rows, 90.0)
    right_row = nearest_horizontal(rows, 270.0)
    left_sr, left_ir = read_wav(require_file(args.left_correction, "Left Stereo Pair correction WAV"))
    right_sr, right_ir = read_wav(require_file(args.right_correction, "Right Stereo Pair correction WAV"))

    left_gain_db = float(manifest.get("leftGainDb", 0.0))
    right_gain_db = float(manifest.get("rightGainDb", 0.0))
    head_trim_db = float(manifest.get("levelTrimDb", 0.0))

    output_left = d / "stereo_pair_left_correction.wav"
    output_right = d / "stereo_pair_right_correction.wav"
    for path in (output_left, output_right):
        if path.exists() and not args.overwrite:
            raise ToolError(
                f"Stereo Pair correction already exists: {path}",
                hint="Use --overwrite only after confirming the replacement measurements are approved.",
                code=12,
            )

    details = []
    prepared = []
    for label, row, sr, ir, out in (
        ("left", left_row, float(left_sr), left_ir, output_left),
        ("right", right_row, float(right_sr), right_ir, output_right),
    ):
        raw_power = weighted_source_power(
            row, head_sr, None, sr, left_gain_db, right_gain_db,
            low_hz=args.low_hz, high_hz=args.high_hz)
        corrected_power = weighted_source_power(
            row, head_sr, ir, sr, left_gain_db, right_gain_db,
            low_hz=args.low_hz, high_hz=args.high_hz)
        if not np.isfinite(raw_power) or not np.isfinite(corrected_power) or raw_power <= 0.0 or corrected_power <= 0.0:
            raise ToolError("Stereo Pair normalization produced invalid power.", hint="Verify the head and correction WAVs.", code=20)
        correction_delta_db = 10.0 * math.log10(corrected_power / raw_power)
        normalization_db = -correction_delta_db
        normalized = np.asarray(ir, dtype=np.float64) * (10.0 ** (normalization_db / 20.0))
        write_float32_mono(out, int(round(sr)), normalized)
        final_power = weighted_source_power(
            row, head_sr, normalized, sr, left_gain_db, right_gain_db,
            low_hz=args.low_hz, high_hz=args.high_hz) * (10.0 ** (head_trim_db / 10.0))
        prepared.append(final_power)
        details.append((label, correction_delta_db, normalization_db, final_power, row["azimuth"], row["elevation"]))

    source_imbalance_db = 10.0 * math.log10(prepared[0] / prepared[1])
    pair_left_gain_db = -0.5 * source_imbalance_db
    pair_right_gain_db = +0.5 * source_imbalance_db
    balanced_left = prepared[0] * (10.0 ** (pair_left_gain_db / 10.0))
    balanced_right = prepared[1] * (10.0 ** (pair_right_gain_db / 10.0))
    # Independent unit-power L/R input has total dry stereo power 2. Match the
    # two binaural source legs to that reference without changing their internal
    # ear relationship.
    pair_trim_db = 10.0 * math.log10(2.0 / (balanced_left + balanced_right))

    if abs(pair_left_gain_db) > 6.0 or abs(pair_right_gain_db) > 6.0:
        raise ToolError(
            f"Stereo Pair source imbalance requires {pair_left_gain_db:+.2f}/{pair_right_gain_db:+.2f} dB, above the ±6 dB safety range.",
            hint="Inspect the 90°/270° measurements and source dataset before packaging.", code=23)
    if not -12.0 <= pair_trim_db <= 12.0:
        raise ToolError(
            f"Stereo Pair level trim {pair_trim_db:+.2f} dB is outside the ±12 dB safety range.",
            hint="Inspect the head level trim and side measurements before packaging.", code=23)

    manifest["stereoPairLeftCorrectionFile"] = output_left.name
    manifest["stereoPairLeftCorrectionSha256"] = sha256_file(output_left)
    manifest["stereoPairRightCorrectionFile"] = output_right.name
    manifest["stereoPairRightCorrectionSha256"] = sha256_file(output_right)
    manifest["stereoPairCorrectionName"] = f"{manifest.get('displayName', 'SpaceTrace Head')} Stereo Pair Tonal Restoration"
    manifest["stereoPairCorrectionVersion"] = args.version
    manifest["stereoPairLeftGainDb"] = round(pair_left_gain_db, 6)
    manifest["stereoPairRightGainDb"] = round(pair_right_gain_db, 6)
    manifest["stereoPairTrimDb"] = round(pair_trim_db, 6)
    manifest["stereoPairCalibrationMethod"] = (
        "90/270 deg Raw source legs at Width Offset 0; per-leg Tone Trace correction normalized to preserve pink-weighted "
        "80-16000 Hz binaural source power after head channel calibration; equal-and-opposite source balance; "
        "common pair trim matches independent unit-power stereo reference at center azimuth 0"
    )
    write_json(manifest_path, manifest)

    print("SpaceTrace Stereo Pair preparation: PASS")
    print(f"  head: {manifest.get('displayName', d.name)}")
    for label, delta, norm, power, az, el in details:
        print(f"  {label}: measurement {az:+.3f}°/{el:+.3f}°, correction pre-normalization delta {delta:+.4f} dB, constant normalization {norm:+.4f} dB")
        print(f"    calibrated source power before pair-source balance: {power:.8f}")
    print(f"  source balance: left {pair_left_gain_db:+.4f} dB, right {pair_right_gain_db:+.4f} dB")
    print(f"  common Stereo Pair trim: {pair_trim_db:+.4f} dB")
    print("  correction EQ shapes: unchanged; only constant normalization gains were applied")
    print("NEXT: Run validate_head.py before using or distributing this head.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        raise SystemExit(die(exc))
