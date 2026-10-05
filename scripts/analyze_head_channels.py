#!/usr/bin/env python3
"""Measure and optionally write SpaceTrace front-center L/R calibration."""
from __future__ import annotations

import argparse
from pathlib import Path

from head_analysis import front_channel_calibration
from tool_common import ToolError, die, load_json, require_file, write_json


def parser():
    p = argparse.ArgumentParser(
        description="Measure pink-weighted left/right balance at the head's 0° azimuth / 0° elevation measurement and derive equal-and-opposite fixed channel gains.",
        epilog=("Example:\n  python scripts/analyze_head_channels.py Heads/MyHead --write-manifest\n\n"
                "The calibration preserves geometric mean channel gain, not exact stereo energy: half of the measured L-R difference is added to the quieter side and removed from the louder side. "
                "It does not alter correction.wav or levelTrimDb."),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument("head_dir", type=Path, help="Head package folder containing manifest.json and head.sthrtf.")
    p.add_argument("--low-hz", type=float, default=80.0, help="Lower analysis bound (default 80 Hz).")
    p.add_argument("--high-hz", type=float, default=16000.0, help="Upper analysis bound (default 16 kHz).")
    p.add_argument("--front-tolerance-deg", type=float, default=0.5, help="Maximum azimuth/elevation error accepted as front-center (default 0.5°).")
    p.add_argument("--max-imbalance-db", type=float, default=3.0, help="Refuse automatic calibration above this measured L-R imbalance unless --allow-large-calibration is supplied (default 3 dB).")
    p.add_argument("--allow-large-calibration", action="store_true", help="Allow writing calibration even when the front-center imbalance exceeds --max-imbalance-db. Use only after inspecting the dataset.")
    p.add_argument("--write-manifest", action="store_true", help="Write leftGainDb/rightGainDb and measured front-center metadata into manifest.json. Without this flag the command is report-only.")
    return p


def main():
    args = parser().parse_args()
    if args.front_tolerance_deg <= 0 or args.max_imbalance_db <= 0:
        raise ToolError("Tolerance and maximum imbalance must be positive.", hint="Use positive degree/dB limits.", code=6)
    d = args.head_dir
    if not d.is_dir():
        raise ToolError(f"Head folder not found: {d}", hint="Point to the folder containing manifest.json and head.sthrtf.", code=3)
    mp = require_file(d / "manifest.json", "manifest.json")
    manifest = load_json(mp)
    head = require_file(d / str(manifest.get("headFile", "head.sthrtf")), "Native head file")
    result = front_channel_calibration(head, low_hz=args.low_hz, high_hz=args.high_hz, tolerance_deg=args.front_tolerance_deg)
    if abs(result["imbalance_db"]) > args.max_imbalance_db and not args.allow_large_calibration:
        raise ToolError(
            f"Front-center L/R imbalance is {result['imbalance_db']:+.3f} dB, above the automatic-calibration limit of {args.max_imbalance_db:g} dB.",
            hint="Inspect the source measurement/coordinate convention first. If the imbalance is genuinely part of the dataset and you still want to compensate it, rerun with --allow-large-calibration.",
            code=23,
        )
    print("SpaceTrace front-center channel analysis: PASS")
    print(f"  head: {manifest.get('displayName', d.name)}")
    print(f"  measurement: azimuth {result['azimuth']:+.4f}°, elevation {result['elevation']:+.4f}°, distance {result['distance']:.3f} m")
    print(f"  analysis band: {result['low_hz']:g}-{result['high_hz']:g} Hz, pink-weighted")
    print(f"  raw front-center level: left {result['left_db']:+.3f} dB, right {result['right_db']:+.3f} dB")
    print(f"  measured L-R imbalance: {result['imbalance_db']:+.3f} dB")
    print(f"  derived fixed channel gains: left {result['left_gain_db']:+.3f} dB, right {result['right_gain_db']:+.3f} dB")
    if args.write_manifest:
        manifest["leftGainDb"] = round(result["left_gain_db"], 6)
        manifest["rightGainDb"] = round(result["right_gain_db"], 6)
        manifest["frontCenterImbalanceDb"] = round(result["imbalance_db"], 6)
        manifest["channelCalibrationMethod"] = f"pink-weighted {result['low_hz']:g}-{result['high_hz']:g} Hz at 0 deg azimuth / 0 deg elevation; equal-and-opposite gains"
        write_json(mp, manifest)
        print(f"  manifest updated: {mp}")
    else:
        print("  manifest unchanged (report-only; add --write-manifest to store the gains)")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        raise SystemExit(die(exc))
