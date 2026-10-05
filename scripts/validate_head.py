#!/usr/bin/env python3
"""Validate one arbitrary SpaceTrace external head package."""
from __future__ import annotations

import argparse
from pathlib import Path
import struct
import sys

from check_native_package import inspect
from tool_common import (ToolError, check_duplicate_stable_id, die, load_json, require_file,
                         sha256_file, validate_stable_id)


def wave_summary(path: Path) -> dict:
    data = require_file(path, "Correction WAV").read_bytes()
    if len(data) < 44 or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ToolError(f"Correction is not a RIFF/WAVE file: {path}", hint="Export the correction as a mono or identical-stereo WAV.", code=20)
    pos = 12; fmt = raw = None
    while pos + 8 <= len(data):
        cid = data[pos:pos+4]; size = struct.unpack_from("<I", data, pos+4)[0]
        chunk = data[pos+8:pos+8+size]
        if cid == b"fmt ": fmt = chunk
        elif cid == b"data": raw = chunk
        pos += 8 + size + (size & 1)
    if fmt is None or raw is None or len(fmt) < 16:
        raise ToolError(f"Correction WAV is missing fmt/data chunks: {path}", hint="Re-export the correction WAV.", code=20)
    tag, channels, sr, _, block_align, bits = struct.unpack_from("<HHIIHH", fmt, 0)
    if channels not in (1, 2):
        raise ToolError(f"Correction WAV has {channels} channels; expected mono or identical stereo.", hint="Export a mono correction, or identical left/right channels.", code=20)
    if (tag, bits) not in ((3, 32), (1, 16), (1, 24), (1, 32)):
        raise ToolError(f"Unsupported correction WAV format tag={tag}, bits={bits}.", hint="Use 32-bit float WAV when possible.", code=20)
    frames = len(raw) // block_align if block_align else 0
    return {"sample_rate": sr, "channels": channels, "frames": frames, "duration": frames / sr if sr else 0.0}


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Validate a SpaceTrace Heads/<name>/ folder: manifest identity, hashes, native package, optional correction and stable-ID uniqueness.",
        epilog="Example:\n  python scripts/validate_head.py Heads/MyHead --heads-root Heads",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument("head_dir", type=Path, help="Head package folder containing manifest.json and head.sthrtf.")
    p.add_argument("--heads-root", type=Path, help="Optional Heads folder used to check stable-ID uniqueness among siblings.")
    return p


def main() -> int:
    args = parser().parse_args()
    d = args.head_dir
    if not d.exists():
        raise ToolError(f"Head folder was not found: {d}", hint="Check the path. Point to the folder containing manifest.json.", code=3)
    if not d.is_dir():
        raise ToolError(f"Head path is not a folder: {d}", hint="Point to the head package directory, not directly to head.sthrtf.", code=3)
    manifest_path = require_file(d / "manifest.json", "manifest.json")
    manifest = load_json(manifest_path)
    if manifest.get("schemaVersion") != 1:
        raise ToolError(f"Unsupported manifest schemaVersion {manifest.get('schemaVersion')!r}.", hint="SpaceTrace 1.0 custom heads use manifest schemaVersion 1.", code=21)
    stable = validate_stable_id(str(manifest.get("stableId", "")))
    if not manifest.get("displayName"):
        raise ToolError("manifest.json has no displayName.", hint="Add a human-readable displayName.", code=21)
    head_name = manifest.get("headFile")
    if not head_name:
        raise ToolError("manifest.json has no headFile.", hint="Set headFile to the generated native filename, normally head.sthrtf.", code=21)
    head = require_file(d / str(head_name), "Native head file")
    expected = str(manifest.get("headSha256", "")).lower()
    actual = sha256_file(head)
    if expected != actual:
        raise ToolError(f"Head SHA-256 mismatch. Manifest={expected or '<missing>'}, actual={actual}.", hint="Do not edit/replace head.sthrtf without regenerating the manifest hash. Re-package the head from the preserved SOFA.", code=22)
    try:
        pkg = inspect(head)
    except Exception as exc:
        raise ToolError(f"Native head package could not be read: {exc}", hint="Re-run convert_sofa_head.py from the original SOFA.", code=22) from exc
    trim = manifest.get("levelTrimDb", 0.0)
    if not isinstance(trim, (int, float)) or not -24 <= float(trim) <= 24:
        raise ToolError("levelTrimDb must be numeric and between -24 and +24 dB.", hint="Use 0 until head-to-head loudness matching has been measured.", code=21)

    left_gain = manifest.get("leftGainDb", 0.0)
    mono_trim = manifest.get("monoTrimDb", 0.0)
    if isinstance(mono_trim, bool) or not isinstance(mono_trim, (int, float)) or not -6 <= float(mono_trim) <= 6:
        raise ToolError("monoTrimDb must be numeric and between -6 and +6 dB.", hint="Measure the centered mono reference with prepare_mono_level.py, or omit the field for legacy unity gain.", code=21)
    right_gain = manifest.get("rightGainDb", 0.0)
    for key, value in (("leftGainDb", left_gain), ("rightGainDb", right_gain)):
        if not isinstance(value, (int, float)) or not -6 <= float(value) <= 6:
            raise ToolError(f"{key} must be numeric and between -6 and +6 dB.", hint="Run analyze_head_channels.py to derive a safe front-center calibration, or use 0.0 until measured.", code=21)
    if abs(float(left_gain) + float(right_gain)) > 0.05:
        raise ToolError("leftGainDb/rightGainDb must be equal-and-opposite within 0.05 dB.", hint="Use analyze_head_channels.py --write-manifest so channel centering does not change overall head level.", code=21)

    corr_name = manifest.get("correctionFile")
    corr_summary = None
    if corr_name:
        corr = require_file(d / str(corr_name), "Correction file referenced by manifest")
        corr_hash = sha256_file(corr)
        expected_corr = str(manifest.get("correctionSha256", "")).lower()
        if corr_hash != expected_corr:
            raise ToolError(f"Correction SHA-256 mismatch. Manifest={expected_corr or '<missing>'}, actual={corr_hash}.", hint="Re-run package_head.py after choosing the intended correction WAV.", code=22)
        corr_summary = wave_summary(corr)

    stereo_left_name = manifest.get("stereoPairLeftCorrectionFile")
    stereo_right_name = manifest.get("stereoPairRightCorrectionFile")
    if bool(stereo_left_name) != bool(stereo_right_name):
        raise ToolError("Stereo Pair correction metadata must provide both left and right correction files.", hint="Run prepare_stereo_pair.py with approved 90° and 270° correction WAVs, or remove all Stereo Pair fields.", code=21)
    stereo_summaries = None
    if stereo_left_name:
        stereo_summaries = []
        for side, name, hash_key in (
            ("left", stereo_left_name, "stereoPairLeftCorrectionSha256"),
            ("right", stereo_right_name, "stereoPairRightCorrectionSha256"),
        ):
            corr = require_file(d / str(name), f"Stereo Pair {side} correction file referenced by manifest")
            actual_hash = sha256_file(corr)
            expected_hash = str(manifest.get(hash_key, "")).lower()
            if actual_hash != expected_hash:
                raise ToolError(f"Stereo Pair {side} correction SHA-256 mismatch. Manifest={expected_hash or '<missing>'}, actual={actual_hash}.", hint="Re-run prepare_stereo_pair.py after choosing the intended side-angle correction WAVs.", code=22)
            stereo_summaries.append(wave_summary(corr))
        pair_left = manifest.get("stereoPairLeftGainDb", 0.0)
        pair_right = manifest.get("stereoPairRightGainDb", 0.0)
        pair_trim = manifest.get("stereoPairTrimDb", 0.0)
        for key, value, low, high in (
            ("stereoPairLeftGainDb", pair_left, -6.0, 6.0),
            ("stereoPairRightGainDb", pair_right, -6.0, 6.0),
            ("stereoPairTrimDb", pair_trim, -12.0, 12.0),
        ):
            if not isinstance(value, (int, float)) or not low <= float(value) <= high:
                raise ToolError(f"{key} must be numeric and between {low:g} and {high:+g} dB.", hint="Re-run prepare_stereo_pair.py to derive the calibration from the head and side-angle corrections.", code=21)
        if abs(float(pair_left) + float(pair_right)) > 0.05:
            raise ToolError("stereoPairLeftGainDb/stereoPairRightGainDb must be equal-and-opposite within 0.05 dB.", hint="Re-run prepare_stereo_pair.py so source balance does not change the pair's geometric mean level.", code=21)
    if args.heads_root:
        check_duplicate_stable_id(args.heads_root, stable, output_dir=d)

    print("SpaceTrace head validation: PASS")
    print(f"  display name: {manifest['displayName']}")
    print(f"  stable ID: {stable}")
    print(f"  sample rate: {pkg['sample_rate']:g} Hz")
    print(f"  measurements: {pkg['count']}")
    print(f"  HRIR samples: {pkg['taps']}")
    print(f"  native SHA-256: {actual}")
    print(f"  fixed level trim: {float(trim):+.2f} dB")
    print(f"  fixed channel gains: left {float(left_gain):+.3f} dB, right {float(right_gain):+.3f} dB")
    if "frontCenterImbalanceDb" in manifest:
        print(f"  measured raw front-center L-R imbalance: {float(manifest['frontCenterImbalanceDb']):+.3f} dB")
    if corr_summary:
        print(f"  correction: PASS ({corr_summary['frames']} samples, {corr_summary['duration']:.3f} s at {corr_summary['sample_rate']} Hz; runtime resampling supported)")
    else:
        print("  correction: none (Raw-only; this is valid)")
    if stereo_summaries:
        print(f"  Stereo Pair corrections: PASS (left {stereo_summaries[0]['sample_rate']} Hz, right {stereo_summaries[1]['sample_rate']} Hz; runtime resampling supported)")
        print(f"  Stereo Pair source gains: left {float(manifest.get('stereoPairLeftGainDb',0.0)):+.3f} dB, right {float(manifest.get('stereoPairRightGainDb',0.0)):+.3f} dB")
        print(f"  Stereo Pair common level trim: {float(manifest.get('stereoPairTrimDb',0.0)):+.3f} dB")
    else:
        print("  Stereo Pair corrections: none (falls back to the normal dataset correction)")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        raise SystemExit(die(exc))
