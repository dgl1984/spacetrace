#!/usr/bin/env python3
"""Regenerate shipped correction plots, or check their source/output fingerprints."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PLOTS = Path("Docs/Correction_Plots")
INDEX = PLOTS / "index.json"
HEADS = (
    ("IRCAM_1050", "IRCAM_1050"),
    ("MIT_KEMAR_Normal", "MIT_KEMAR_Normal"),
    ("KU100_SADIE_D1", "KU100_SADIE_D1"),
    ("KU100_FULL2DEG", "FULL2DEG_KU100"),
    ("FABIAN_HATO0", "FABIAN_HATO0"),
)
AXES = {"low_hz": 20, "high_hz": 20000, "y_min_db": -18, "y_max_db": 32}


def digest(path: Path) -> str:
    return hashlib.sha256((ROOT / path).read_bytes()).hexdigest()


def jobs() -> list[dict]:
    result = []
    for folder, prefix in HEADS:
        head = Path("Heads") / folder
        manifest = json.loads((ROOT / head / "manifest.json").read_text(encoding="utf-8-sig"))
        title = manifest["displayName"]
        for field, stem, label in (
            ("correction", f"{prefix}_correction_response", "Mono correction IR"),
            ("stereoPairLeftCorrection", f"StereoPair/{folder}_left_correction_response", "Stereo Pair left correction IR"),
            ("stereoPairRightCorrection", f"StereoPair/{folder}_right_correction_response", "Stereo Pair right correction IR"),
        ):
            source = head / manifest[field + "File"]
            if digest(source) != manifest[field + "Sha256"]:
                raise ValueError(f"Correction file does not match its head manifest: {source}")
            result.append({"source": source.as_posix(), "image": (PLOTS / (stem + ".png")).as_posix(), "title": f"{title} — {label}"})
        model = Path("Docs/Correction_Models") / f"{prefix}_ToneTrace.ttm"
        image = PLOTS / f"{prefix}_ToneTrace_model.png"
        result.append({"source": model.as_posix(), "image": image.as_posix(), "title": f"{title} — Tone Trace model"})
        # Retain existing alternate image paths, but derive them from the same current model.
        if folder != "KU100_FULL2DEG":
            alternate = PLOTS / "Models" / f"{prefix}_ToneTrace_model_curve.png"
            result.append({"source": model.as_posix(), "image": alternate.as_posix(), "title": f"{title} — Tone Trace model"})
    return result


def record(job: dict) -> dict:
    image = Path(job["image"])
    description = image.with_suffix(".txt")
    return {**job, "description": description.as_posix(),
            "source_sha256": digest(Path(job["source"])),
            "image_sha256": digest(image), "description_sha256": digest(description)}


def current_index(entries: list[dict]) -> dict:
    return {"schema_version": 1, "axes": AXES,
            "plotter_sha256": digest(Path("scripts/plot_correction.py")),
            "generator_sha256": digest(Path("scripts/refresh_correction_plots.py")),
            "plots": [record(job) for job in entries]}


def check(entries: list[dict]) -> None:
    actual = {p.relative_to(ROOT).as_posix() for p in (ROOT / PLOTS).rglob("*.png")}
    expected = {job["image"] for job in entries}
    if actual != expected:
        raise ValueError(f"Plot inventory differs: missing={sorted(expected-actual)}, extra={sorted(actual-expected)}")
    saved = json.loads((ROOT / INDEX).read_text(encoding="utf-8"))
    if saved != current_index(entries):
        raise ValueError("Correction plots are stale or modified. Run python scripts/refresh_correction_plots.py")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Check all source and output hashes without rendering or changing files.")
    args = parser.parse_args()
    entries = jobs()
    if not args.check:
        from plot_correction import read_ttm, wav_curve
        for job in entries:
            source = Path(job["source"])
            curve = wav_curve if source.suffix == ".wav" else read_ttm
            frequencies, gains = curve(ROOT / source)
            visible = gains[(frequencies >= AXES["low_hz"]) & (frequencies <= AXES["high_hz"])]
            if visible.min() < AXES["y_min_db"] or visible.max() > AXES["y_max_db"]:
                raise ValueError(f"Curve exceeds the common axes: {source}; expand AXES for all plots.")
        for job in entries:
            command = [sys.executable, "scripts/plot_correction.py", job["source"], job["image"],
                       "--title", job["title"], "--overwrite"]
            for key, value in AXES.items():
                command.extend(["--" + key.replace("_", "-"), str(value)])
            subprocess.run(command, cwd=ROOT, check=True, capture_output=True, text=True)
            description = ROOT / Path(job["image"]).with_suffix(".txt")
            text = description.read_text(encoding="utf-8")
            text += f"Source file: {job['source']}\nSource SHA-256: {digest(Path(job['source']))}\n"
            description.write_bytes(text.encode("utf-8"))
        (ROOT / INDEX).write_bytes((json.dumps(current_index(entries), indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
    check(entries)
    print(f"Correction plots: PASS ({len(entries)} images and descriptions)")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        raise SystemExit(f"Correction plots: FAIL: {exc}")
