#!/usr/bin/env python3
"""Select the intended SADIE II D1 44.1 kHz / 256-tap HRIR SOFA from an extracted archive."""
from __future__ import annotations
import sys
from pathlib import Path
import h5py
import numpy as np
from sofa_to_sthrtf import normalise_ir_shape


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: select_sadie_d1_sofa.py <extracted-directory>", file=sys.stderr)
        return 2
    root = Path(sys.argv[1])
    matches: list[Path] = []
    for path in sorted(root.rglob("*.sofa")):
        try:
            with h5py.File(path, "r") as f:
                if "Data.IR" not in f or "Data.SamplingRate" not in f:
                    continue
                ir = normalise_ir_shape(f["Data.IR"][...])
                sr = float(np.asarray(f["Data.SamplingRate"][...], dtype=np.float64).reshape(-1)[0])
                if abs(sr - 44100.0) < 0.1 and ir.shape == (8802, 2, 256):
                    matches.append(path.resolve())
        except Exception:
            continue
    if len(matches) != 1:
        print(f"expected exactly one SADIE D1 44.1 kHz / 8802-position / 256-tap SOFA, found {len(matches)}", file=sys.stderr)
        for path in matches:
            print(f"  {path}", file=sys.stderr)
        return 1
    print(matches[0])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
