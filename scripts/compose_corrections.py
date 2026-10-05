#!/usr/bin/env python3
"""Compose an existing correction and a residual IR offline, without normalization."""
from __future__ import annotations

import argparse
from pathlib import Path

from normalize_correction import read_wav, require_numpy, write_float32_mono
from tool_common import ToolError, die


def compose(existing: Path, residual: Path, output: Path, *, overwrite: bool = False,
            tail_fade_ms: float = 0.0) -> dict:
    """Write the full convolution, optionally tapering its end; never normalize."""
    np = require_numpy()
    if output.resolve() in (existing.resolve(), residual.resolve()):
        raise ToolError("Output must not replace either source IR.",
                        hint="Keep the source components and choose a separate combined WAV.", code=12)
    if output.exists() and not overwrite:
        raise ToolError(f"Output already exists: {output}",
                        hint="Choose a new output, or use --overwrite to rebuild from preserved components.", code=12)
    rate, base = read_wav(existing)
    residual_rate, extra = read_wav(residual)
    if rate <= 0 or rate != residual_rate:
        raise ToolError("Correction components must have the same positive sample rate.",
                        hint="Export both components at the same rate before composing them.", code=20)
    for samples in (base, extra):
        if samples.size == 0 or not np.isfinite(samples).all() or not np.any(samples):
            raise ToolError("Correction component is empty, silent, or non-finite.",
                            hint="Use the original valid correction WAVs.", code=20)
    taps = len(base) + len(extra) - 1
    if not np.isfinite(tail_fade_ms) or tail_fade_ms < 0 or tail_fade_ms > 1000.0 * (taps - 1) / rate:
        raise ToolError("Tail fade must be finite, non-negative and shorter than the response.",
                        hint="Use 0 for no taper, or a short fade confined to the decayed tail.", code=20)
    fade_samples = round(rate * tail_fade_ms / 1000.0)
    if tail_fade_ms > 0 and fade_samples < 2:
        raise ToolError("Tail fade must cover at least two samples.",
                        hint="Increase --tail-fade-ms or use 0 for no taper.", code=20)
    nfft = 1 << (taps - 1).bit_length()
    combined = np.fft.irfft(np.fft.rfft(base, nfft) * np.fft.rfft(extra, nfft), nfft)[:taps]
    if not np.isfinite(combined).all() or np.max(np.abs(combined)) > np.finfo(np.float32).max:
        raise ToolError("Combined correction exceeds finite float32 storage.",
                        hint="Inspect the component gains before composition.", code=20)
    if fade_samples:
        # Half cosine: unity at the start, zero at the end, zero slope at both.
        # Leave the onset and preceding response untouched. No gain makeup.
        combined[-fade_samples:] *= 0.5 * (1.0 + np.cos(np.linspace(0.0, np.pi, fade_samples)))
        combined[-1] = 0.0
    write_float32_mono(output, rate, combined)
    return {"sampleRate": rate, "baseTaps": len(base), "residualTaps": len(extra),
            "outputTaps": taps, "durationSeconds": taps / rate,
            "normalizationDb": 0.0, "tailFadeMs": tail_fade_ms, "tailFadeSamples": fade_samples,
            "tailFadeShape": "half cosine" if fade_samples else "none",
            "method": "full linear convolution; optional final tail taper; float64 calculation, float32 WAV"}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("existing", type=Path)
    parser.add_argument("residual", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--overwrite", action="store_true")
    parser.add_argument("--tail-fade-ms", type=float, default=0.0,
                        help="Optional half-cosine fade over the final milliseconds (default: none).")
    args = parser.parse_args()
    result = compose(args.existing, args.residual, args.output, overwrite=args.overwrite,
                     tail_fade_ms=args.tail_fade_ms)
    print(f"Correction composition: PASS ({result['outputTaps']} taps at {result['sampleRate']} Hz)")
    print(f"Full length retained; final tail fade: {result['tailFadeMs']:g} ms. No normalization or gain calibration.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        raise SystemExit(die(exc))
