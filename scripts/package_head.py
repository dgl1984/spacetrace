#!/usr/bin/env python3
"""Add/remove a fixed correction and level trim in an existing SpaceTrace head package."""
from __future__ import annotations

import argparse
from pathlib import Path
import shutil

from check_native_package import inspect
from validate_head import wave_summary
from head_analysis import front_channel_calibration
from tool_common import ToolError, die, load_json, require_file, sha256_file, write_json


def parser():
    p=argparse.ArgumentParser(description="Finalize a converted SpaceTrace head package by attaching an approved normalized correction and/or a separately measured fixed level trim.", epilog="Example:\n  python scripts/package_head.py Heads/MyHead --correction correction-neutral.wav --correction-name \"My Head Tonal Restoration\" --correction-version tonetrace-fixed-v1 --level-trim-db -2.1", formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("head_dir",type=Path)
    p.add_argument("--correction",type=Path,help="Approved correction WAV to copy as correction.wav. Omit to leave Raw-only.")
    p.add_argument("--correction-name",default="")
    p.add_argument("--correction-version",default="tonetrace-fixed-v1")
    p.add_argument("--level-trim-db",type=float,help="Separate fixed head-to-head loudness trim. Does not alter correction shape.")
    p.add_argument("--calibrate-front-center", action="store_true", help="Measure 0°/0° raw L/R balance and write equal-and-opposite leftGainDb/rightGainDb calibration.")
    p.add_argument("--max-channel-imbalance-db", type=float, default=3.0, help="Refuse automatic front-center calibration above this L-R imbalance (default 3 dB).")
    p.add_argument("--allow-large-channel-calibration", action="store_true", help="Allow calibration above --max-channel-imbalance-db after manual inspection.")
    p.add_argument("--remove-correction",action="store_true",help="Remove correction metadata/file and return the package to Raw-only.")
    p.add_argument("--overwrite",action="store_true",help="Allow replacing an existing correction.wav.")
    return p


def main():
    args=parser().parse_args(); d=args.head_dir
    if not d.is_dir(): raise ToolError(f"Head folder not found: {d}",hint="Run convert_sofa_head.py first, then point this command to its output folder.",code=3)
    manifest_path=require_file(d/"manifest.json","manifest.json"); manifest=load_json(manifest_path); head=require_file(d/str(manifest.get("headFile","head.sthrtf")),"Native head file"); pkg=inspect(head)
    if args.remove_correction and args.correction: raise ToolError("Choose either --correction or --remove-correction, not both.",hint="Use --remove-correction for Raw-only, or --correction to attach one approved correction.",code=6)
    if args.calibrate_front_center:
        cal = front_channel_calibration(head)
        if abs(cal["imbalance_db"]) > args.max_channel_imbalance_db and not args.allow_large_channel_calibration:
            raise ToolError(f"Front-center L/R imbalance is {cal['imbalance_db']:+.3f} dB, above the automatic-calibration limit of {args.max_channel_imbalance_db:g} dB.", hint="Inspect the source dataset first, or rerun with --allow-large-channel-calibration if the imbalance is known and intentional.", code=23)
        manifest["leftGainDb"] = round(cal["left_gain_db"], 6)
        manifest["rightGainDb"] = round(cal["right_gain_db"], 6)
        manifest["frontCenterImbalanceDb"] = round(cal["imbalance_db"], 6)
        manifest["channelCalibrationMethod"] = "pink-weighted 80-16000 Hz at 0 deg azimuth / 0 deg elevation; equal-and-opposite gains"

    if args.level_trim_db is not None:
        if not -24<=args.level_trim_db<=24: raise ToolError("--level-trim-db must be between -24 and +24 dB.",hint="Measure level matching separately from correction normalization.",code=6)
        manifest["levelTrimDb"]=float(args.level_trim_db)
    if args.remove_correction:
        old=manifest.get("correctionFile")
        stereo_files=[manifest.get("stereoPairLeftCorrectionFile"), manifest.get("stereoPairRightCorrectionFile")]
        for k in ("correctionFile","correctionSha256","correctionName","correctionVersion",
                  "stereoPairLeftCorrectionFile","stereoPairLeftCorrectionSha256",
                  "stereoPairRightCorrectionFile","stereoPairRightCorrectionSha256",
                  "stereoPairCorrectionName","stereoPairCorrectionVersion",
                  "stereoPairLeftGainDb","stereoPairRightGainDb","stereoPairTrimDb",
                  "stereoPairCalibrationMethod"):
            manifest.pop(k,None)
        for name in [old, *stereo_files]:
            if name:
                p=d/str(name)
                if p.exists(): p.unlink()
        print("Correction removed; package is Raw-only, including Stereo Pair correction assets.")
    elif args.correction:
        src=require_file(args.correction,"Correction WAV"); summary=wave_summary(src)
        dest=d/"correction.wav"
        if dest.exists() and dest.resolve()!=src.resolve() and not args.overwrite: raise ToolError(f"Correction already exists: {dest}",hint="Use --overwrite only after confirming the new correction is the approved replacement.",code=12)
        if dest.resolve()!=src.resolve(): shutil.copy2(src,dest)
        manifest["correctionFile"]="correction.wav"; manifest["correctionSha256"]=sha256_file(dest); manifest["correctionName"]=args.correction_name or f"{manifest.get('displayName','SpaceTrace Head')} Tonal Restoration"; manifest["correctionVersion"]=args.correction_version
        print(f"Correction attached: {dest}")
        if abs(summary["sample_rate"] - pkg["sample_rate"]) > 1e-6:
            print(f"  note: correction is {summary['sample_rate']} Hz while head source is {pkg['sample_rate']:g} Hz; runtime resampling is supported: SpaceTrace resamples correction and head independently for the host rate")
    write_json(manifest_path,manifest)
    print("Head package metadata update: PASS")
    print(f"  stable ID: {manifest.get('stableId','<missing>')}")
    print(f"  level trim: {float(manifest.get('levelTrimDb',0.0)):+.2f} dB")
    print(f"  channel gains: left {float(manifest.get('leftGainDb',0.0)):+.3f} dB, right {float(manifest.get('rightGainDb',0.0)):+.3f} dB")
    print("NEXT: Run validate_head.py before using or distributing this head.")
    return 0

if __name__=="__main__":
    try: raise SystemExit(main())
    except Exception as exc: raise SystemExit(die(exc))
