#!/usr/bin/env python3
"""Generate a documentation correction plot and a text audit description."""
from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path
import math

from normalize_correction import read_wav
from tool_common import ToolError, die, ensure_parent_writable, require_file


def require_plot_deps():
    missing = [name for name in ("numpy", "matplotlib") if importlib.util.find_spec(name) is None]
    if missing:
        raise ToolError(
            "Missing Python package(s): " + ", ".join(missing),
            hint="Install dependencies with: python -m pip install -r scripts/requirements.txt",
            code=10,
        )
    import numpy as np
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    return np, plt


def read_ttm(path: Path):
    np, _ = require_plot_deps()
    path = require_file(path, "Tone Trace model")
    freqs, gains = [], []
    try:
        lines = path.read_text(encoding="utf-8-sig", errors="strict").splitlines()
    except (OSError, UnicodeError) as exc:
        raise ToolError(f"Could not read Tone Trace model {path}: {exc}", hint="Choose the original .ttm file exported by Tone Trace.", code=24) from exc
    if not lines or not lines[0].strip().startswith("ToneTraceModel"):
        raise ToolError(f"Not a recognized Tone Trace model: {path}", hint="Choose a .ttm file beginning with 'ToneTraceModel'.", code=24)
    for line in lines:
        parts = line.split()
        if len(parts) != 3:
            continue
        try:
            f, g, _confidence = map(float, parts)
        except ValueError:
            continue
        if math.isfinite(f) and math.isfinite(g) and f > 0:
            freqs.append(f); gains.append(g)
    if len(freqs) < 8:
        raise ToolError(f"Tone Trace model has too few usable frequency nodes: {len(freqs)}", hint="Re-export the model from Tone Trace instead of hand-editing it.", code=24)
    f = np.asarray(freqs, dtype=float); g = np.asarray(gains, dtype=float)
    order = np.argsort(f)
    return f[order], g[order]


def wav_curve(path: Path):
    np, _ = require_plot_deps()
    sr, x = read_wav(require_file(path, "Correction WAV"))
    if len(x) < 2:
        raise ToolError("Correction WAV is too short to plot.", hint="Choose the exported correction impulse response, not an empty file.", code=20)
    nfft = 1
    while nfft < max(131072, len(x) * 16):
        nfft <<= 1
    H = np.fft.rfft(x, n=nfft)
    f = np.fft.rfftfreq(nfft, 1.0 / sr)
    db = 20.0 * np.log10(np.maximum(np.abs(H), 1e-12))
    return f, db


def summarize(freqs, gains, low_hz: float, high_hz: float, source_kind: str, title: str) -> str:
    np, _ = require_plot_deps()
    mask = (freqs >= low_hz) & (freqs <= high_hz)
    if not np.any(mask):
        raise ToolError(f"No correction data falls inside {low_hz:g}-{high_hz:g} Hz.", hint="Choose plot bounds that overlap the correction/model frequency range.", code=6)
    f = freqs[mask]; g = gains[mask]
    min_i = int(np.argmin(g)); max_i = int(np.argmax(g))
    bands = [(80, 250, "low bass / low mids"), (250, 1000, "low mids / mids"), (1000, 4000, "mids / presence"), (4000, 16000, "upper mids / treble")]
    band_lines = []
    for lo, hi, label in bands:
        bm = (freqs >= lo) & (freqs <= hi)
        if np.any(bm):
            band_lines.append(f"- {label} ({lo:g}-{hi:g} Hz): mean {float(np.mean(gains[bm])):+.2f} dB")
    return "\n".join([
        f"Correction plot audit — {title}",
        f"Source type: {source_kind}",
        f"Plotted range: {low_hz:g}-{high_hz:g} Hz (log-frequency axis)",
        f"Strongest boost in plotted range: {float(g[max_i]):+.2f} dB at approximately {float(f[max_i]):.0f} Hz",
        f"Deepest cut in plotted range: {float(g[min_i]):+.2f} dB at approximately {float(f[min_i]):.0f} Hz",
        "Broad-band means:",
        *band_lines,
        "Interpretation note: this description reports the plotted correction data only. It does not infer perceptual quality, localization quality, or head-to-head loudness matching.",
    ]) + "\n"


def parser():
    p = argparse.ArgumentParser(
        description="Create a plain, detailed logarithmic frequency-response PNG from a correction WAV or Tone Trace .ttm model, plus a machine-generated text description for audit/accessibility.",
        epilog=("Examples:\n"
                "  python scripts/plot_correction.py correction.wav correction_response.png --title \"My Head — Dataset Correction\"\n"
                "  python scripts/plot_correction.py model.ttm model_curve.png --title \"My Head — Tone Trace Model\"\n\n"
                "A .txt description is written beside the PNG by default. The image is documentation; the plugin does not need it to process audio."),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument("source", type=Path, help="Correction WAV or Tone Trace .ttm model.")
    p.add_argument("output_png", type=Path, help="Destination PNG path.")
    p.add_argument("--description", type=Path, help="Destination text description. Defaults to <output>.txt.")
    p.add_argument("--title", default="SpaceTrace Correction", help="Human-readable chart title.")
    p.add_argument("--low-hz", type=float, default=20.0, help="Left plot bound (default 20 Hz).")
    p.add_argument("--high-hz", type=float, default=20000.0, help="Right plot bound (default 20 kHz).")
    p.add_argument("--y-min-db", type=float, default=-18.0, help="Bottom dB bound (default -18 dB).")
    p.add_argument("--y-max-db", type=float, default=32.0, help="Top dB bound (default +32 dB).")
    p.add_argument("--overwrite", action="store_true", help="Replace existing PNG/description outputs.")
    return p


def main():
    args = parser().parse_args(); np, plt = require_plot_deps()
    src = require_file(args.source, "Correction source")
    if not (0 < args.low_hz < args.high_hz) or not (args.y_min_db < args.y_max_db):
        raise ToolError("Invalid plot axis bounds.", hint="Use positive low/high frequencies with low < high, and y-min < y-max.", code=6)
    ext = src.suffix.lower()
    if ext == ".wav":
        freqs, gains = wav_curve(src); source_kind = "correction WAV frequency response"; ylabel = "Correction Gain (dB)"
    elif ext == ".ttm":
        freqs, gains = read_ttm(src); source_kind = "Tone Trace model nodes"; ylabel = "Tone Trace Model Gain (dB)"
    else:
        raise ToolError(f"Unsupported correction source extension {src.suffix!r}.", hint="Use a Tone Trace .ttm model or a correction .wav file.", code=24)
    nyq_or_max = float(freqs[-1])
    high = min(args.high_hz, nyq_or_max)
    if args.low_hz >= high:
        raise ToolError(f"Requested plot range starts above the source data limit ({nyq_or_max:g} Hz).", hint="Lower --low-hz/--high-hz or choose the intended file.", code=6)
    out = args.output_png; desc = args.description or out.with_suffix(".txt")
    for p in (out, desc):
        if p.exists() and not args.overwrite:
            raise ToolError(f"Output already exists: {p}", hint="Choose a new output path or add --overwrite after confirming replacement is intended.", code=12)
        ensure_parent_writable(p)
    mask = (freqs >= args.low_hz) & (freqs <= high)
    fig = plt.figure(figsize=(10, 6))
    ax = fig.add_subplot(111)
    ax.plot(freqs[mask], gains[mask], linewidth=1.5)
    ax.set_xscale("log"); ax.set_xlim(args.low_hz, high); ax.set_ylim(args.y_min_db, args.y_max_db)
    ax.set_xlabel("Frequency (Hz)"); ax.set_ylabel(ylabel); ax.set_title(args.title)
    ax.grid(True, which="both")
    fig.tight_layout(); fig.savefig(out, dpi=200); plt.close(fig)
    description = summarize(freqs, gains, max(args.low_hz, 20.0), high, source_kind, args.title)
    desc.write_text(description, encoding="utf-8")
    print("Correction plot generation: PASS")
    print(f"  source: {src}")
    print(f"  image: {out}")
    print(f"  description: {desc}")
    print(f"  plotted range: {args.low_hz:g}-{high:g} Hz")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        raise SystemExit(die(exc))
