#!/usr/bin/env python3
import argparse, hashlib, json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADS = ROOT / 'Heads'

EXPECTED = {
    'IRCAM_1050': ('ircam.listen.1050.spacetrace-mp128.44100', True),
    'MIT_KEMAR_Normal': ('mit.kemar.normal-pinna.44100', True),
    'KU100_SADIE_D1': ('sadie2.d1.ku100.44100', True),
    'KU100_FULL2DEG': ('thk.ku100.full2deg.48000', True),
    'FABIAN_HATO0': ('fabian.hato0.44100', True),
}

def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()

def validate(folder: str, stable_id: str, expect_correction: bool, required: bool):
    d = HEADS / folder
    manifest = d / 'manifest.json'
    head = d / 'head.sthrtf'
    any_present = d.exists() and any(d.iterdir())
    if not required and not manifest.exists() and not head.exists():
        return 'SKIP'
    if not manifest.exists(): raise AssertionError(f'{folder}: manifest.json missing')
    data = json.loads(manifest.read_text(encoding='utf-8-sig'))
    if data.get('schemaVersion') != 1: raise AssertionError(f'{folder}: schemaVersion must be 1')
    if data.get('stableId') != stable_id: raise AssertionError(f'{folder}: stableId mismatch')
    head_name = data.get('headFile')
    if not head_name: raise AssertionError(f'{folder}: headFile missing')
    head = d / head_name
    if not head.is_file(): raise AssertionError(f'{folder}: head file missing')
    actual = sha256(head)
    if actual.lower() != str(data.get('headSha256','')).lower():
        raise AssertionError(f'{folder}: head SHA-256 mismatch')
    trim = data.get('levelTrimDb', 0.0)
    if not isinstance(trim, (int, float)) or not -24.0 <= float(trim) <= 24.0:
        raise AssertionError(f'{folder}: levelTrimDb must be numeric and between -24 and +24 dB')
    lg = data.get('leftGainDb', 0.0); rg = data.get('rightGainDb', 0.0)
    if not isinstance(lg, (int,float)) or not -6.0 <= float(lg) <= 6.0:
        raise AssertionError(f'{folder}: leftGainDb must be numeric and between -6 and +6 dB')
    if not isinstance(rg, (int,float)) or not -6.0 <= float(rg) <= 6.0:
        raise AssertionError(f'{folder}: rightGainDb must be numeric and between -6 and +6 dB')
    if abs(float(lg) + float(rg)) > 0.05:
        raise AssertionError(f'{folder}: channel gains must be equal-and-opposite')
    corr_name = data.get('correctionFile')
    if expect_correction:
        if not corr_name: raise AssertionError(f'{folder}: correctionFile missing')
        corr = d / corr_name
        if not corr.is_file(): raise AssertionError(f'{folder}: correction file missing')
        if sha256(corr).lower() != str(data.get('correctionSha256','')).lower():
            raise AssertionError(f'{folder}: correction SHA-256 mismatch')
    elif corr_name:
        raise AssertionError(f'{folder}: unexpected correctionFile')
    return 'PASS'

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--require-all', action='store_true')
    args = ap.parse_args()
    if not HEADS.is_dir(): raise AssertionError('Heads directory missing')
    results = []
    for folder, (stable, corrected) in EXPECTED.items():
        required = args.require_all or folder != 'KU100_SADIE_D1'
        results.append((folder, validate(folder, stable, corrected, required)))
    print('SpaceTrace external head packages: ' + ', '.join(f'{k}={v}' for k,v in results))
    return 0

if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f'FAIL: {exc}')
        raise SystemExit(1)
