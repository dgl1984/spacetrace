#!/usr/bin/env python3
"""Validate frozen SpaceTrace built-in packages against their official SOFA sources.

Build/checkpoint QA only; not a runtime dependency.
"""
from __future__ import annotations
import argparse, hashlib, struct, sys
from pathlib import Path
import h5py
import numpy as np
from sofa_to_sthrtf import normalise_ir_shape, source_positions_to_canonical, read_wave_mono

MAGIC=b"STHRTF1\0"


def fail(msg: str):
    raise ValueError(msg)


def text(blob: bytes, off: int):
    if off+4>len(blob): fail("truncated string length")
    n=struct.unpack_from('<I',blob,off)[0]; off+=4
    if off+n>len(blob): fail("truncated string")
    return blob[off:off+n].decode('utf-8','replace'), off+n


def load_pkg(path: Path):
    b=path.read_bytes(); off=0
    if b[:8]!=MAGIC: fail(f"{path.name}: bad magic")
    off=8
    fmt='<IIIdIIdI'; sz=struct.calcsize(fmt)
    ver,endian,flags,sr,count,n,comp_sr,comp_n=struct.unpack_from(fmt,b,off); off+=sz
    if ver!=2 or endian!=0x01020304: fail(f"{path.name}: bad version/endian")
    keys=['name','subject','source_format','source_path','source_url','database','license','attribution','source_sha256','processing','comp_name','comp_version']
    meta={}
    for k in keys:
        meta[k],off=text(b,off)
    pos=np.empty((count,5),np.float64)
    ir=np.empty((count,2,n),np.float32)
    for i in range(count):
        if off+20>len(b): fail(f"{path.name}: truncated measurement header")
        pos[i]=struct.unpack_from('<fffff',b,off); off+=20
        need=2*n*4
        if off+need>len(b): fail(f"{path.name}: truncated IR data")
        ir[i,0]=np.frombuffer(b,dtype='<f4',count=n,offset=off); off+=n*4
        ir[i,1]=np.frombuffer(b,dtype='<f4',count=n,offset=off); off+=n*4
    comp=np.empty(0,np.float32)
    if comp_n:
        if off+comp_n*4>len(b): fail(f"{path.name}: truncated compensation")
        comp=np.frombuffer(b,dtype='<f4',count=comp_n,offset=off).copy(); off+=comp_n*4
    if off!=len(b): fail(f"{path.name}: trailing bytes ({len(b)-off})")
    return dict(version=ver,flags=flags,sr=sr,count=count,n=n,comp_sr=comp_sr,comp_n=comp_n,meta=meta,pos=pos,ir=ir,comp=comp)


def sofa_data(path: Path):
    raw=path.read_bytes(); sha=hashlib.sha256(raw).hexdigest()
    with h5py.File(path,'r') as f:
        ir=normalise_ir_shape(f['Data.IR'][...]).astype(np.float64)
        sr=float(np.asarray(f['Data.SamplingRate'][...]).reshape(-1)[0])
        pos=source_positions_to_canonical(f['SourcePosition'],ir.shape[0])
    return sha,sr,pos,ir


def corr_lag(a,b):
    a=np.asarray(a,np.float64); b=np.asarray(b,np.float64)
    full=len(a)+len(b)-1
    nfft=1 << max(0,int(full-1).bit_length())
    c=np.fft.irfft(np.fft.rfft(a,nfft)*np.fft.rfft(b[::-1],nfft),nfft)[:full]
    i=int(np.argmax(c)); lag=float(i-(len(b)-1))
    if 0<i<len(c)-1:
        ym1,y0,yp1=map(float,c[i-1:i+2]); den=ym1-2*y0+yp1
        if abs(den)>1e-20: lag+=float(np.clip(0.5*(ym1-yp1)/den,-0.5,0.5))
    return lag


def common_checks(pkg, sofa_sha, sofa_sr, sofa_pos, expected_name, expected_count, expected_n, expected_radius,
                  require_compensation=True, require_quarter_sample_delays=False):
    if pkg['sr']!=sofa_sr: fail(f"{expected_name}: sample-rate mismatch")
    if pkg['count']!=expected_count or pkg['count']!=len(sofa_pos): fail(f"{expected_name}: measurement-count mismatch")
    if pkg['n']!=expected_n: fail(f"{expected_name}: unexpected packaged IR length {pkg['n']}")
    if pkg['meta']['source_sha256']!=sofa_sha: fail(f"{expected_name}: source SHA metadata mismatch")
    if pkg['meta']['name']!=expected_name: fail(f"{expected_name}: package name mismatch")
    if not np.all(np.isfinite(pkg['ir'])) or not np.all(np.isfinite(pkg['pos'])): fail(f"{expected_name}: non-finite data")
    if np.min(pkg['pos'][:,3:]) < -1e-7: fail(f"{expected_name}: negative delay")
    if require_quarter_sample_delays and np.max(np.abs(pkg['pos'][:,3:]*4.0-np.round(pkg['pos'][:,3:]*4.0))) > 1e-6:
        fail(f"{expected_name}: delays are not on quarter-sample grid")
    if np.max(np.abs(pkg['pos'][:,:3]-sofa_pos)) > 2e-4: fail(f"{expected_name}: geometry does not match SOFA")
    radii=np.unique(np.round(pkg['pos'][:,2],4))
    if len(radii)!=1 or abs(float(radii[0])-expected_radius)>1e-4: fail(f"{expected_name}: radius mismatch {radii}")
    if require_compensation:
        if pkg['comp_n']==0 or pkg['comp_sr']!=pkg['sr'] or not np.all(np.isfinite(pkg['comp'])):
            fail(f"{expected_name}: invalid dataset compensation")


def validate_ircam(sofa: Path, package: Path):
    sha,sr,pos,raw=sofa_data(sofa); pkg=load_pkg(package)
    common_checks(pkg,sha,sr,pos,'IRCAM LISTEN 1050',187,128,2.06,require_quarter_sample_delays=True)
    if sha!='5efc0dcc002551313b60a9bce6c957db1b07c8d25d1474c9f9333c5175c4dd83':
        fail('IRCAM source differs from the frozen 0.2.0 official SOFA')
    horiz=np.flatnonzero(np.abs(pkg['pos'][:,1])<1e-5)
    if len(horiz)!=24: fail(f'IRCAM: expected 24 measured horizontal positions, got {len(horiz)}')
    itd=[]
    for i in range(pkg['count']):
        rawlag=corr_lag(raw[i,1],raw[i,0])
        compactlag=corr_lag(pkg['ir'][i,1],pkg['ir'][i,0]) + (pkg['pos'][i,4]-pkg['pos'][i,3])
        itd.append(compactlag-rawlag)
    ae=np.abs(np.asarray(itd))
    p95=float(np.percentile(ae,95)); worst=float(np.max(ae))
    if p95>0.35 or worst>0.55: fail(f'IRCAM: ITD preservation outside tolerance: p95={p95:.3f}, max={worst:.3f} samples')
    return f"IRCAM PASS: 187 positions, 128 taps, ITD abs error p95={p95:.3f} sample, max={worst:.3f}"


def validate_kemar(sofa: Path, package: Path):
    sha,sr,pos,raw=sofa_data(sofa); pkg=load_pkg(package)
    common_checks(pkg,sha,sr,pos,'MIT KEMAR Normal Pinna',710,512,1.4)
    if sha!='e7035994f5fd754058424c061380ee92b1d5ed58fccef2887a4266916616acdf':
        fail('KEMAR source differs from the frozen 0.2.0 official SOFA')
    horiz=np.flatnonzero(np.abs(pkg['pos'][:,1])<1e-5)
    if len(horiz)!=72: fail(f'KEMAR: expected 72 horizontal positions, got {len(horiz)}')
    err=float(np.max(np.abs(pkg['ir'].astype(np.float64)-raw.astype(np.float32).astype(np.float64))))
    if err>1e-7: fail(f'KEMAR: raw HRIR copy mismatch {err}')
    if np.max(np.abs(pkg['pos'][:,3:]))>1e-7: fail('KEMAR: unexpected nonzero delay')
    return f"KEMAR PASS: 710 positions, 512 raw taps, max float-copy error={err:.3g}"



def validate_ku100(sofa: Path, package: Path, compensation: Path):
    sha,sr,pos,raw=sofa_data(sofa); pkg=load_pkg(package)
    common_checks(pkg,sha,sr,pos,'SADIE II D1 / Neumann KU100',8802,256,1.2,require_compensation=True)
    if pkg['meta']['database']!='SADIE II': fail('KU100: database identity mismatch')
    if 'D1' not in pkg['meta']['subject'] or 'KU100' not in pkg['meta']['subject']:
        fail('KU100: subject identity mismatch')
    if pkg['meta']['comp_name']!='Tone Trace raw-HRTF global restoration':
        fail('KU100: correction name mismatch')
    if pkg['meta']['comp_version']!='tonetrace-raw-v1':
        fail('KU100: correction version mismatch')
    expected_comp,expected_sr=read_wave_mono(compensation)
    if abs(expected_sr-pkg['comp_sr'])>1e-6 or len(expected_comp)!=pkg['comp_n']:
        fail('KU100: correction WAV shape/sample-rate mismatch')
    comp_err=float(np.max(np.abs(pkg['comp'].astype(np.float64)-expected_comp.astype(np.float64))))
    if comp_err>1e-7: fail(f'KU100: correction samples differ from the measured Tone Trace asset ({comp_err})')
    err=float(np.max(np.abs(pkg['ir'].astype(np.float64)-raw.astype(np.float32).astype(np.float64))))
    if err>1e-7: fail(f'KU100: raw HRIR copy mismatch {err}')
    horiz=np.flatnonzero(np.abs(pkg['pos'][:,1])<1e-5)
    if len(horiz)<4: fail(f'KU100: expected a populated horizontal ring, got {len(horiz)} points')
    def nearest(az):
        da=np.abs(((pkg['pos'][:,0]-az+180.0)%360.0)-180.0)
        score=da + 10.0*np.abs(pkg['pos'][:,1])
        return int(np.argmin(score))
    li=nearest(90.0); ri=nearest(-90.0)
    if abs(pkg['pos'][li,1])>0.1 or abs(((pkg['pos'][li,0]-90.0+180.0)%360.0)-180.0)>0.1:
        fail('KU100: +90/elevation-zero measurement not found')
    if abs(pkg['pos'][ri,1])>0.1 or abs(((pkg['pos'][ri,0]+90.0+180.0)%360.0)-180.0)>0.1:
        fail('KU100: -90/elevation-zero measurement not found')
    def energy(x): return float(np.dot(x.astype(np.float64),x.astype(np.float64)))
    ild_l=10.0*np.log10(max(energy(pkg['ir'][li,0]),1e-30)/max(energy(pkg['ir'][li,1]),1e-30))
    ild_r=10.0*np.log10(max(energy(pkg['ir'][ri,0]),1e-30)/max(energy(pkg['ir'][ri,1]),1e-30))
    if ild_l<=6.0 or ild_r>=-6.0:
        fail(f'KU100: left/right orientation check failed (+90 {ild_l:.2f} dB, -90 {ild_r:.2f} dB)')
    return (f"KU100 PASS: 8802 positions, 256 raw taps, radius 1.2 m, "
            f"max HRIR float-copy error={err:.3g}, correction error={comp_err:.3g}, "
            f"side ILD=({ild_l:.1f},{ild_r:.1f}) dB")

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--ircam-sofa',type=Path)
    ap.add_argument('--kemar-sofa',type=Path)
    ap.add_argument('--ku100-sofa',type=Path)
    ap.add_argument('--ircam-package',type=Path)
    ap.add_argument('--kemar-package',type=Path)
    ap.add_argument('--ku100-package',type=Path)
    ap.add_argument('--ku100-compensation',type=Path)
    a=ap.parse_args()
    pairs=[
        ('IRCAM',a.ircam_sofa,a.ircam_package,validate_ircam),
        ('KEMAR',a.kemar_sofa,a.kemar_package,validate_kemar),
        ('KU100',a.ku100_sofa,a.ku100_package,None),
    ]
    ran=0
    for label,sofa,package,fn in pairs:
        if (sofa is None)!=(package is None):
            fail(f'{label}: source SOFA and package must be supplied together')
        if sofa is not None:
            if label == 'KU100':
                if a.ku100_compensation is None:
                    fail('KU100: measured Tone Trace compensation WAV must be supplied')
                print(validate_ku100(sofa,package,a.ku100_compensation))
            else:
                print(fn(sofa,package))
            ran+=1
    if ran==0: fail('no dataset pair supplied for validation')
    print('SpaceTrace built-in dataset validation: PASS')
    return 0

if __name__=='__main__':
    try: raise SystemExit(main())
    except Exception as e:
        print(f'SpaceTrace built-in dataset validation: FAIL: {e}',file=sys.stderr)
        raise SystemExit(1)
