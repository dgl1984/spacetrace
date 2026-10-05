#!/usr/bin/env python3
"""Developer audit for packaged Stereo Pair correction/calibration assets."""
from pathlib import Path
import json, math, sys
import numpy as np
from normalize_correction import read_wav
from head_analysis import read_native_measurements, correction_response_magnitude

ROOT=Path(__file__).resolve().parents[1]
HEADS=['IRCAM_1050','MIT_KEMAR_Normal','KU100_SADIE_D1','KU100_FULL2DEG','FABIAN_HATO0']

def nearest(rows,az):
    return min(rows,key=lambda r:abs(r['elevation'])*1000+abs(((r['azimuth']-az+180)%360)-180))

def leg_curve(row,hsr,corr,csr,earL,earR,srcGain,headTrim,pairTrim):
    n=65536; f=np.fft.rfftfreq(n,1/hsr); m=(f>=80)&(f<=min(16000,hsr*.49)); fb=f[m]
    C=correction_response_magnitude(corr,csr,fb)
    L=np.abs(np.fft.rfft(row['left'],n)[m])*C*10**(earL/20)
    R=np.abs(np.fft.rfft(row['right'],n)[m])*C*10**(earR/20)
    # Power-equivalent common magnitude for one stereo input channel. A dry
    # input channel contributes unit total power, so compare against sqrt(L^2+R^2).
    mag=np.sqrt(L*L+R*R)*10**((srcGain+headTrim+pairTrim)/20)
    db=20*np.log10(np.maximum(mag,1e-12))
    w=1/np.maximum(fb,1e-12)
    power=float(np.sum((mag*mag)*w)/np.sum(w))
    level=10*math.log10(power)
    return fb,db,level

def shape_metrics(row, hsr, corr=None, csr=None, earL=0.0, earR=0.0):
    n=65536; f=np.fft.rfftfreq(n,1/hsr); m=(f>=80)&(f<=min(16000,hsr*.49)); fb=f[m]
    L=np.abs(np.fft.rfft(row['left'],n)[m])*10**(earL/20)
    R=np.abs(np.fft.rfft(row['right'],n)[m])*10**(earR/20)
    if corr is not None:
        C=correction_response_magnitude(corr,csr,fb); L*=C; R*=C
    mag=np.sqrt(L*L+R*R); db=20*np.log10(np.maximum(mag,1e-12)); w=1/np.maximum(fb,1e-12)
    mean=np.sum(db*w)/np.sum(w); shape=db-mean
    rms=float(np.sqrt(np.sum(shape*shape*w)/np.sum(w))); hi=(fb>=5000)&(fb<=10000)
    hirms=float(np.sqrt(np.sum(shape[hi]**2*(1/fb[hi]))/np.sum(1/fb[hi])))
    return rms,hirms

rows=[]
for h in HEADS:
    d=ROOT/'Heads'/h; man=json.loads((d/'manifest.json').read_text(encoding='utf-8-sig'))
    hsr,meas=read_native_measurements(d/'head.sthrtf')
    gl=float(man.get('leftGainDb',0)); gr=float(man.get('rightGainDb',0)); ht=float(man.get('levelTrimDb',0)); pt=float(man.get('stereoPairTrimDb',0))
    side=[]
    for az,key,gkey in [(90,'stereoPairLeftCorrectionFile','stereoPairLeftGainDb'),(270,'stereoPairRightCorrectionFile','stereoPairRightGainDb')]:
        row=nearest(meas,az); csr,c=read_wav(d/man[key]); f,db,lev=leg_curve(row,hsr,c,float(csr),gl,gr,float(man.get(gkey,0)),ht,pt)
        shape=db-np.sum(db*(1/f))/np.sum(1/f)
        rms=float(np.sqrt(np.sum(shape*shape*(1/f))/np.sum(1/f)))
        hi=(f>=5000)&(f<=10000); hirms=float(np.sqrt(np.sum(shape[hi]**2*(1/f[hi]))/np.sum(1/f[hi])))
        rawr,rawhi=shape_metrics(row,hsr,None,None,gl,gr)
        side.append((lev,rms,hirms,rawr,rawhi))
    combined=10*math.log10(10**(side[0][0]/10)+10**(side[1][0]/10))
    rows.append((h,*side[0],*side[1],combined))

print('head,left_level_db,left_corrected_shape_rms_db,left_corrected_5_10k_rms_db,left_raw_shape_rms_db,left_raw_5_10k_rms_db,right_level_db,right_corrected_shape_rms_db,right_corrected_5_10k_rms_db,right_raw_shape_rms_db,right_raw_5_10k_rms_db,pair_total_db_ref2')
for r in rows:
    # pair total relative to dry stereo total power 2: subtract 10log10(2)
    print(','.join([r[0]]+[f'{x:.4f}' for x in r[1:-1]]+[f'{r[-1]-10*math.log10(2):.4f}']))
