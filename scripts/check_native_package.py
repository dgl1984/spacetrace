#!/usr/bin/env python3
"""Fast standard-library check for an existing SpaceTrace native package.

This deliberately avoids h5py/numpy so BUILD_WINDOWS.ps1 can decide whether a
cached/generated package is current before installing build-only Python modules
or touching network/cache state.
"""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

MAGIC = b"STHRTF1\0"
HEADER = struct.Struct("<IIIdIIdI")
KEYS = [
    "name", "subject", "source_format", "source_path", "source_url",
    "database", "license", "attribution", "source_sha256", "processing",
    "comp_name", "comp_version",
]


def read_text(blob: bytes, offset: int) -> tuple[str, int]:
    if offset + 4 > len(blob):
        raise ValueError("truncated string length")
    size = struct.unpack_from("<I", blob, offset)[0]
    offset += 4
    if offset + size > len(blob):
        raise ValueError("truncated string")
    return blob[offset:offset + size].decode("utf-8", "replace"), offset + size


def inspect(path: Path) -> dict:
    blob = path.read_bytes()
    if len(blob) < len(MAGIC) + HEADER.size or blob[:8] != MAGIC:
        raise ValueError("not a SpaceTrace native package")
    offset = 8
    version, endian, flags, sample_rate, count, taps, comp_rate, comp_count = HEADER.unpack_from(blob, offset)
    offset += HEADER.size
    if version != 2 or endian != 0x01020304:
        raise ValueError(f"unsupported native package version/endian: {version}/{endian:#x}")
    meta = {}
    for key in KEYS:
        meta[key], offset = read_text(blob, offset)
    return {
        "flags": flags,
        "sample_rate": sample_rate,
        "count": count,
        "taps": taps,
        "comp_rate": comp_rate,
        "comp_count": comp_count,
        "meta": meta,
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("package", type=Path)
    ap.add_argument("--name", default="")
    ap.add_argument("--measurement-count", type=int)
    ap.add_argument("--ir-length", type=int)
    ap.add_argument("--compensation-version", default="")
    ap.add_argument("--require-compensation", action="store_true")
    args = ap.parse_args()

    try:
        if not args.package.is_file():
            raise ValueError("package file is absent")
        pkg = inspect(args.package)
        if args.name and pkg["meta"]["name"] != args.name:
            raise ValueError(f"name is {pkg['meta']['name']!r}")
        if args.measurement_count is not None and pkg["count"] != args.measurement_count:
            raise ValueError(f"measurement count is {pkg['count']}")
        if args.ir_length is not None and pkg["taps"] != args.ir_length:
            raise ValueError(f"IR length is {pkg['taps']}")
        if args.require_compensation and (pkg["comp_count"] == 0 or not (pkg["flags"] & 1)):
            raise ValueError("dataset compensation is absent")
        if args.compensation_version and pkg["meta"]["comp_version"] != args.compensation_version:
            raise ValueError(f"compensation version is {pkg['meta']['comp_version']!r}")
        print(f"SpaceTrace package check: PASS: {args.package}")
        return 0
    except Exception as exc:
        # stdout intentionally: Windows PowerShell 5.1 + ErrorActionPreference
        # Stop can promote stderr from native programs into NativeCommandError.
        print(f"SpaceTrace package check: STALE: {args.package}: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
