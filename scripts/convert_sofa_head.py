#!/usr/bin/env python3
"""Convert a compatible SOFA HRTF into a Raw-only SpaceTrace head folder."""
from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys

from tool_common import (ToolError, check_duplicate_stable_id, die, ensure_parent_writable,
                         require_file, sha256_file, validate_stable_id, write_json)

SUPPORTED_CONVENTIONS = {"SimpleFreeFieldHRIR", "GeneralFIR"}


def require_dependencies() -> None:
    missing = [name for name in ("numpy", "h5py") if importlib.util.find_spec(name) is None]
    if missing:
        raise ToolError(
            "Missing Python package(s): " + ", ".join(missing),
            hint=f"Install the script dependencies with: {sys.executable} -m pip install -r scripts/requirements.txt",
            code=10,
        )


def inspect_sofa(path: Path) -> dict:
    require_dependencies()
    import h5py
    import numpy as np

    try:
        with h5py.File(path, "r") as sofa:
            convention = sofa.attrs.get("SOFAConventions", sofa.attrs.get("Conventions", ""))
            if isinstance(convention, bytes):
                convention = convention.decode("utf-8", "replace")
            convention = str(convention)
            if convention and convention not in SUPPORTED_CONVENTIONS:
                raise ToolError(
                    f"Unsupported SOFA convention {convention!r}.",
                    hint="SpaceTrace currently expects a two-receiver FIR HRTF using SimpleFreeFieldHRIR-compatible Data.IR and SourcePosition data.",
                    code=11,
                )
            def node(dotted: str):
                if dotted in sofa:
                    return sofa[dotted]
                slash = dotted.replace(".", "/")
                if slash in sofa:
                    return sofa[slash]
                raise ToolError(f"SOFA file is missing required dataset {dotted}.", hint="Verify this is an HRIR/HRTF SOFA file rather than a different SOFA data type.", code=11)
            ir = np.asarray(node("Data.IR")[...])
            squeezed = np.squeeze(ir)
            while squeezed.ndim > 3:
                singleton = next((i for i, n in enumerate(squeezed.shape) if n == 1), None)
                if singleton is None:
                    break
                squeezed = np.squeeze(squeezed, axis=singleton)
            if squeezed.ndim != 3:
                raise ToolError(f"Unsupported Data.IR shape {tuple(ir.shape)}.", hint="Expected measurement × 2 receivers × samples (singleton emitter axes are accepted).", code=11)
            if squeezed.shape[1] == 2:
                measurements, receivers, taps = squeezed.shape
            elif squeezed.shape[0] == 2:
                receivers, measurements, taps = squeezed.shape
            else:
                raise ToolError(f"SOFA Data.IR must have exactly two receivers; got shape {tuple(ir.shape)}.", hint="SpaceTrace currently supports ordinary binaural heads only.", code=11)
            sr = np.asarray(node("Data.SamplingRate")[...], dtype=float).reshape(-1)
            if sr.size != 1 or not np.isfinite(sr[0]) or sr[0] <= 0:
                raise ToolError("SOFA must contain exactly one valid sampling rate.", hint="Resample or export the HRTF as a single-rate SOFA before conversion.", code=11)
            pos = np.asarray(node("SourcePosition")[...])
            def text_attr(name: str) -> str:
                v = sofa.attrs.get(name, "")
                if isinstance(v, bytes):
                    return v.decode("utf-8", "replace")
                try:
                    if hasattr(v, "size") and v.size == 0:
                        return ""
                except Exception:
                    pass
                return str(v) if v is not None else ""
            return {
                "convention": convention or "unspecified",
                "measurements": int(measurements),
                "receivers": int(receivers),
                "taps": int(taps),
                "sample_rate": float(sr[0]),
                "source_position_shape": tuple(int(x) for x in pos.shape),
                "title": text_attr("Title"),
                "database": text_attr("DatabaseName"),
                "listener": text_attr("ListenerShortName"),
                "license": text_attr("License"),
                "references": text_attr("References"),
                "comment": text_attr("Comment"),
            }
    except ToolError:
        raise
    except OSError as exc:
        raise ToolError(f"Could not open SOFA file {path}: {exc}", hint="The file may be truncated, corrupt, locked, or not actually an HDF5/SOFA file.", code=11) from exc


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Convert a compatible binaural SOFA HRTF into a Raw-only SpaceTrace Heads/<name>/ package.",
        epilog=("Example:\n  python scripts/convert_sofa_head.py MyHead.sofa Heads/MyHead "
                "--stable-id mylab.myhead.48000 --display-name \"My Head\" --source-url https://example.org/head.sofa\n\n"
                "The original SOFA remains the archival source. The generated .sthrtf is a SpaceTrace runtime derivative."),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument("source_sofa", type=Path, help="Input SOFA HRTF file.")
    p.add_argument("output_head_dir", type=Path, help="Destination head folder, normally under SpaceTrace/Heads/.")
    p.add_argument("--stable-id", required=True, help="Permanent unique project-state identity for this head.")
    p.add_argument("--display-name", default="", help="Name shown in SpaceTrace. Defaults to the SOFA title or filename.")
    p.add_argument("--source-url", default="", help="Canonical public source URL for provenance.")
    p.add_argument("--license-file", type=Path, help="License/notice text to copy into LICENSE.txt and native metadata.")
    p.add_argument("--attribution", default="", help="Additional attribution text stored in the native package.")
    p.add_argument("--level-trim-db", type=float, default=0.0, help="Fixed head loudness trim. Usually leave 0 until measured separately.")
    p.add_argument("--heads-root", type=Path, help="Folder whose sibling manifests should be checked for duplicate stable IDs. Defaults to output parent.")
    p.add_argument("--overwrite", action="store_true", help="Replace an existing output head folder. Without this flag, existing output is refused.")
    return p


def main() -> int:
    args = parser().parse_args()
    source = require_file(args.source_sofa, "SOFA input")
    stable_id = validate_stable_id(args.stable_id)
    if not -24.0 <= args.level_trim_db <= 24.0:
        raise ToolError("--level-trim-db must be between -24 and +24 dB.", hint="Use 0 dB until a separate head-level measurement justifies a trim.", code=6)
    if args.license_file:
        require_file(args.license_file, "License file")
    out = args.output_head_dir
    if out.exists() and any(out.iterdir() if out.is_dir() else [out]):
        if not args.overwrite:
            raise ToolError(f"Output head folder already exists and is not empty: {out}", hint="Choose another output folder or re-run with --overwrite after confirming you want to replace it.", code=12)
        if not out.is_dir():
            raise ToolError(f"Output path exists but is not a folder: {out}", hint="Choose a directory path for the head package.", code=12)
        shutil.rmtree(out)
    ensure_parent_writable(out / "placeholder")
    heads_root = args.heads_root or out.parent
    check_duplicate_stable_id(heads_root, stable_id, output_dir=out)

    info = inspect_sofa(source)
    print("SOFA validation: PASS")
    print(f"  convention: {info['convention']}")
    print(f"  sample rate: {info['sample_rate']:g} Hz")
    print(f"  measurements: {info['measurements']}")
    print(f"  receivers: {info['receivers']}")
    print(f"  HRIR samples: {info['taps']}")

    out.mkdir(parents=True, exist_ok=True)
    native = out / "head.sthrtf"
    engine = Path(__file__).with_name("sofa_to_sthrtf.py")
    cmd = [sys.executable, str(engine), str(source), str(native)]
    if args.display_name:
        cmd += ["--name", args.display_name]
    if args.source_url:
        cmd += ["--source-url", args.source_url]
    if args.license_file:
        cmd += ["--licence-file", str(args.license_file)]
    if args.attribution:
        cmd += ["--attribution", args.attribution]
    cmd += ["--processing-description", "Raw SOFA HRIR samples converted to float32; source coordinates normalized to the SpaceTrace canonical convention"]
    proc = subprocess.run(cmd, text=True, capture_output=True)
    if proc.stdout:
        print(proc.stdout.rstrip())
    if proc.returncode != 0:
        detail = (proc.stderr or "conversion engine failed without an error message").strip()
        shutil.rmtree(out, ignore_errors=True)
        raise ToolError(f"SOFA conversion failed: {detail}", hint="Check the SOFA validation information above. Keep the original SOFA and do not use a partially generated head folder.", code=13)

    display = args.display_name or info.get("listener") or info.get("title") or source.stem
    manifest = {
        "schemaVersion": 1,
        "stableId": stable_id,
        "displayName": display,
        "headFile": "head.sthrtf",
        "headSha256": sha256_file(native),
        "levelTrimDb": float(args.level_trim_db),
        "leftGainDb": 0.0,
        "rightGainDb": 0.0,
        "notes": "Raw-only SpaceTrace head generated from the preserved SOFA source. Add a head-specific correction only after measuring this exact converted head.",
    }
    write_json(out / "manifest.json", manifest)
    if args.license_file:
        shutil.copy2(args.license_file, out / "LICENSE.txt")
    elif info.get("license"):
        (out / "LICENSE.txt").write_text(
            "License reported by the source SOFA metadata:\n\n" + info["license"].strip() + "\n",
            encoding="utf-8",
        )
    provenance = {
        "sourceFile": source.name,
        "sourceSha256": sha256_file(source),
        "sourceUrl": args.source_url,
        "conversion": "SpaceTrace scripts/convert_sofa_head.py",
        "conversionNotes": "SOFA remains the archival source. Runtime HRIR samples are float32 and coordinates are normalized to SpaceTrace convention.",
        "sourceSummary": info,
    }
    write_json(out / "provenance.json", provenance)

    print("Head package creation: PASS")
    print(f"  folder: {out}")
    print(f"  stable ID: {stable_id}")
    print(f"  head SHA-256: {manifest['headSha256']}")
    print("  correction: none (Raw-only)")
    print("NEXT: Run validate_head.py on this folder. If you want Dataset Corrected mode, create a Tone Trace correction from this exact Raw head first.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        raise SystemExit(die(exc))
