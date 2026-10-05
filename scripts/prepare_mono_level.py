"""Measure a fixed mono reference trim without modifying any correction WAV."""
import argparse
import hashlib
import json
import math
from pathlib import Path

import numpy as np

from head_analysis import read_native_measurements, nearest_front_center, correction_response_magnitude
from normalize_correction import read_wav


def measure(folder):
    manifest = json.loads((folder / 'manifest.json').read_text(encoding='utf-8-sig'))
    head = folder / manifest['headFile']
    correction = folder / manifest['correctionFile']
    sr, rows = read_native_measurements(head)
    row = nearest_front_center(rows)
    correction_sr, ir = read_wav(correction)
    n = 131072
    f = np.fft.rfftfreq(n, 1 / sr)
    band = (f >= 80) & (f <= 16000)
    weights = 1 / f[band]
    c = correction_response_magnitude(ir, correction_sr, f[band])
    power = 0.0
    for side in ('left', 'right'):
        h = np.fft.rfft(row[side], n)[band]
        # RealtimeFIRRenderer implements fractional delay by linear interpolation.
        # Its magnitude is not unity; include it in the reference measurement.
        fraction = row[side + '_delay'] % 1
        h *= (1 - fraction) + fraction * np.exp(-2j * np.pi * f[band] / sr)
        gain = 10 ** ((manifest.get(side + 'GainDb', 0) + manifest.get('levelTrimDb', 0)) / 20)
        power += float(np.sum(abs(h * c * gain) ** 2 * weights) / np.sum(weights)) / 2
    trim = -10 * math.log10(power)
    if not math.isfinite(trim) or not -6 <= trim <= 6:
        raise ValueError(f'{folder.name}: mono trim {trim} exceeds +/-6 dB')
    return manifest, {
        'method': 'Front center 0/0 degrees at native head sample rate, including runtime fractional-delay magnitude, Dataset Corrected, 1 metre, 80-16000 Hz pink weighting; average ear power matches unit mono source. Fixed gain also applies in Raw/Custom to preserve correction gain differences. Stereo Pair and Papa Pan excluded.',
        'referenceSampleRate': sr,
        'referenceGainBeforeDb': -trim,
        'monoTrimDb': round(trim, 6),
        'headSha256': hashlib.sha256(head.read_bytes()).hexdigest(),
        'correctionSha256': hashlib.sha256(correction.read_bytes()).hexdigest(),
        'headLevelTrimDb': manifest.get('levelTrimDb', 0),
        'leftGainDb': manifest.get('leftGainDb', 0),
        'rightGainDb': manifest.get('rightGainDb', 0),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('head', type=Path)
    parser.add_argument('--write', action='store_true', help='Update manifest and provenance; WAVs remain unchanged')
    args = parser.parse_args()
    manifest, result = measure(args.head)
    if args.write:
        provenance_path = args.head / 'provenance.json'
        provenance = json.loads(provenance_path.read_text(encoding='utf-8-sig'))
        manifest['monoTrimDb'] = result['monoTrimDb']
        provenance['monoLevelCalibration'] = result
        for path, data in ((args.head / 'manifest.json', manifest), (provenance_path, provenance)):
            path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
