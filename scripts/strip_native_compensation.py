#!/usr/bin/env python3
"""Create a Raw-only SpaceTrace .sthrtf by removing its embedded compensation payload.

Directional HRTF bytes and metadata are preserved exactly. Only the native-package
compensation flag/rate/length are cleared and the trailing compensation samples are
removed. This is used for the 1.0 external Heads/ layout where correction.wav is a
separate immutable resource.
"""
from __future__ import annotations
import argparse, struct
from pathlib import Path

MAGIC=b'STHRTF1\0'
FLAG_COMP=1

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('source')
    ap.add_argument('destination')
    args=ap.parse_args()
    src=Path(args.source); dst=Path(args.destination)
    data=bytearray(src.read_bytes())
    if len(data)<48 or bytes(data[:8])!=MAGIC:
        raise SystemExit('not a SpaceTrace native HRTF package')
    version,endian,flags=struct.unpack_from('<III',data,8)
    if endian!=0x01020304 or version not in (1,2):
        raise SystemExit('unsupported SpaceTrace native package')
    comp_len=struct.unpack_from('<I',data,44)[0]
    trim=comp_len*4 if flags & FLAG_COMP else 0
    if trim>len(data)-48:
        raise SystemExit('invalid compensation length')
    struct.pack_into('<I',data,16,flags & ~FLAG_COMP)
    struct.pack_into('<d',data,36,0.0)
    struct.pack_into('<I',data,44,0)
    if trim:
        del data[-trim:]
    dst.parent.mkdir(parents=True,exist_ok=True)
    dst.write_bytes(data)
    print(f'Raw-only package: {dst} ({len(data)} bytes)')

if __name__=='__main__':
    main()
