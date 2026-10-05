#!/usr/bin/env python3
"""Apply one constant gain to make a correction IR pink-noise-RMS neutral."""
from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path
import struct
import sys

from tool_common import ToolError, die, ensure_parent_writable, require_file
from head_analysis import head_weighted_correction_delta_db


def require_numpy():
    if importlib.util.find_spec("numpy") is None:
        raise ToolError("Missing Python package: numpy", hint=f"Install dependencies with: {sys.executable} -m pip install -r scripts/requirements.txt", code=10)
    import numpy as np
    return np


def read_wav(path: Path):
    np = require_numpy()
    data = require_file(path, "Correction WAV").read_bytes()
    if len(data) < 44 or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ToolError(f"Not a RIFF/WAVE file: {path}", hint="Export a mono or identical-stereo WAV from Tone Trace.", code=20)
    pos=12; fmt=raw=None
    while pos+8 <= len(data):
        cid=data[pos:pos+4]; size=struct.unpack_from("<I", data, pos+4)[0]; chunk=data[pos+8:pos+8+size]
        if cid==b"fmt ": fmt=chunk
        elif cid==b"data": raw=chunk
        pos += 8+size+(size&1)
    if fmt is None or raw is None or len(fmt)<16:
        raise ToolError("WAV is missing required fmt/data chunks.", hint="Re-export the correction WAV.", code=20)
    tag,ch,sr,_,_,bits=struct.unpack_from("<HHIIHH",fmt,0)
    if tag==3 and bits==32: a=np.frombuffer(raw,dtype="<f4").astype(np.float64)
    elif tag==1 and bits==16: a=np.frombuffer(raw,dtype="<i2").astype(np.float64)/32768.0
    elif tag==1 and bits==24:
        b=np.frombuffer(raw,dtype=np.uint8).reshape(-1,3); x=b[:,0].astype(np.int32)|(b[:,1].astype(np.int32)<<8)|(b[:,2].astype(np.int32)<<16); x=np.where(x&0x800000,x-0x1000000,x); a=x.astype(np.float64)/8388608.0
    elif tag==1 and bits==32: a=np.frombuffer(raw,dtype="<i4").astype(np.float64)/2147483648.0
    else: raise ToolError(f"Unsupported WAV format tag={tag}, bits={bits}.", hint="32-bit float WAV is recommended.", code=20)
    if ch not in (1,2) or a.size % ch: raise ToolError(f"Expected mono or identical-stereo correction; got {ch} channels.", hint="Export a mono correction WAV.", code=20)
    a=a.reshape(-1,ch)
    if ch==2 and not np.allclose(a[:,0],a[:,1],atol=1e-7,rtol=1e-6): raise ToolError("Stereo correction channels are not identical.", hint="Dataset correction must be mono/common to both ears; export mono or identical stereo.", code=20)
    return sr, np.asarray(a[:,0],dtype=np.float64)


def write_float32_mono(path: Path, sr: int, x) -> None:
    np=require_numpy(); ensure_parent_writable(path)
    raw=np.asarray(x,dtype="<f4").tobytes(); fmt=struct.pack("<HHIIHH",3,1,int(sr),int(sr)*4,4,32)
    fact=struct.pack("<I",len(x)); riff_size=4+(8+len(fmt))+(8+len(fact))+(8+len(raw))
    with path.open("wb") as f:
        f.write(b"RIFF"+struct.pack("<I",riff_size)+b"WAVE")
        f.write(b"fmt "+struct.pack("<I",len(fmt))+fmt)
        f.write(b"fact"+struct.pack("<I",len(fact))+fact)
        f.write(b"data"+struct.pack("<I",len(raw))+raw)


def parser():
    p=argparse.ArgumentParser(description="Normalize a Tone Trace correction with one constant gain so ideal pink-noise RMS is unchanged over a chosen band.", epilog="Example:\n  python scripts/normalize_correction.py correction.wav correction-neutral.wav\n\nOnly a constant gain is applied. The correction's EQ shape is not altered.", formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("input_wav",type=Path); p.add_argument("output_wav",type=Path)
    p.add_argument("--low-hz",type=float,default=80.0,help="Lower pink-noise normalization bound (default 80 Hz).")
    p.add_argument("--high-hz",type=float,default=16000.0,help="Upper normalization bound (default 16 kHz).")
    p.add_argument("--head", type=Path, help="Optional head.sthrtf for head-aware normalization. When supplied, one constant gain is chosen so Dataset Corrected matches that head's Raw average level over the elevation-zero ring.")
    p.add_argument("--overwrite",action="store_true")
    return p


def main():
    args=parser().parse_args(); np=require_numpy(); sr,x=read_wav(args.input_wav)
    if args.output_wav.exists() and not args.overwrite: raise ToolError(f"Output file already exists: {args.output_wav}",hint="Choose a new output filename or add --overwrite.",code=12)
    nyq=sr/2.0
    if not (0<args.low_hz<args.high_hz<nyq): raise ToolError(f"Invalid normalization band {args.low_hz:g}-{args.high_hz:g} Hz for {sr} Hz audio.",hint=f"Choose 0 < low < high < Nyquist ({nyq:g} Hz).",code=6)
    nfft=1
    while nfft < max(16384,len(x)*16): nfft <<= 1
    H=np.fft.rfft(x,n=nfft); f=np.fft.rfftfreq(nfft,1.0/sr); m=(f>=args.low_hz)&(f<=args.high_hz)
    if args.head:
        delta_db = head_weighted_correction_delta_db(require_file(args.head, "Native head file"), x, float(sr), low_hz=args.low_hz, high_hz=args.high_hz)
        gain_db = -delta_db
        gain = 10.0 ** (gain_db / 20.0)
        method = f"head-aware Raw-vs-Corrected level neutral over elevation-zero ring; pre-normalization delta {delta_db:+.4f} dB"
    else:
        # Ideal pink noise has power proportional to 1/f. Choose one constant gain so
        # mean output power under that spectrum equals input power across the band.
        weights=1.0/np.maximum(f[m],1e-12); power=float(np.sum((np.abs(H[m])**2)*weights)/np.sum(weights))
        if not np.isfinite(power) or power<=0: raise ToolError("Correction response has invalid/zero energy in the normalization band.",hint="Verify the correction WAV is the intended non-empty IR.",code=20)
        gain=1.0/np.sqrt(power); gain_db=20.0*np.log10(gain)
        method = "ideal pink-noise RMS neutral; EQ shape unchanged"
    y=x*gain
    peak=float(np.max(np.abs(y))) if len(y) else 0.0
    write_float32_mono(args.output_wav,sr,y)
    print("Correction normalization: PASS")
    print(f"  input: {args.input_wav}")
    print(f"  output: {args.output_wav}")
    print(f"  sample rate: {sr} Hz")
    print(f"  band: {args.low_hz:g}-{args.high_hz:g} Hz")
    print(f"  constant gain applied: {gain_db:+.4f} dB")
    print(f"  output sample peak: {peak:.6f}")
    print(f"  method: {method}")
    print("  EQ shape: unchanged; only one constant gain was applied")
    return 0

if __name__=="__main__":
    try: raise SystemExit(main())
    except Exception as exc: raise SystemExit(die(exc))
