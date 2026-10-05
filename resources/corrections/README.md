# SpaceTrace built-in tonal restoration

This directory documents the normal global corrections. SpaceTrace keeps tonal restoration separate from directional HRTF data. The
filter for a dataset is one fixed mono/minimum-phase filter applied identically
to both ears after binaural rendering; it never changes with azimuth/elevation.

The built-in filters were measured by comparing pink noise through the
corresponding **Raw** SpaceTrace HRTF against the bypassed source in Tone Trace
CLAP. Each single correction was then auditioned while moving sources around.
The correction therefore targets the common head/dataset coloration while the
position-dependent HRTF coloration remains untouched.

Modern HRTF Stereo Pair has separate optional fixed 90/270-degree correction
assets under `Heads/`, with source models in `Docs/Correction_Models/StereoPair/`.
Those corrections are applied before the binaural source legs are summed and
stay fixed as Azimuth, Elevation or Width Offset changes. Their calibration
reference is center Azimuth 0, Elevation 0 and Width Offset 0.

## IRCAM LISTEN 1050

Source model: `source_models/IRCAM1050_Raw_ToneTrace_2026-09-28.ttm`

The fixed correction restored the source timbre while preserving the IRCAM
spatial cues. IRCAM remains the default built-in HRTF.

## MIT KEMAR Normal Pinna

Source model: `source_models/MIT_KEMAR_Raw_ToneTrace_2026-09-28.ttm`

The same Raw/reference procedure was used. One fixed correction was sufficient
while moving sources around.

## SADIE II D1 / Neumann KU100

Source model: `source_models/SADIE2_D1_KU100_Raw_ToneTrace_2026-09-28.ttm`

Again, one fixed correction restored the broad source tonality while leaving the
KU100 movement cues intact. Listening ranked its overall spatial flexibility
between IRCAM and KEMAR, with elevation somewhat less convincing than IRCAM.


## FABIAN HATO 0

Source model: `source_models/FABIAN_HATO0_Raw_ToneTrace_2026-09-30.ttm`

FABIAN uses the same one-global-correction philosophy. The user-approved Tone Trace
model is retained as the authoritative EQ shape. The originally supplied WAV had
been reduced by about 3.0007 dB globally; SpaceTrace does not treat that manual
volume reduction as part of the correction. The shipped correction instead applies
one constant normalization offset so FABIAN Raw and Dataset Corrected are equal in
aggregate level, while a separate `levelTrimDb` in the external head manifest
matches FABIAN's overall loudness to the other heads.

## IR render settings

The correction WAVs were rendered with the released Tone Trace minimum-phase
engine from the recovered 1.0.5/1.1 code line: 44.1 kHz, 0.18 seconds, Strength
1.0, Sharpness/Q 1.0, Correction Gain 0 dB, full model range, and the model's
18 dB audible correction ceiling. The `.ttm` source models are retained so the
assets are traceable rather than opaque generated WAVs.

The original IRCAM/KEMAR/KU100 native dataset packages identify these filters as
`tonetrace-raw-v1` / `Tone Trace raw-HRTF global restoration`.

SHA-256:
- IRCAM model: d9ba2de81da65c1f9a59d4dbd6f7eb0470388bcbdf9b7cb32fe0b2b666221467
- IRCAM IR: 1557a5a26e15537d486bdefa277d4ea9702016b040ad0901668dbe5f0870cbd7
- KEMAR model: 67956b2aa63d59ba867e49e95b1836c126c242191156817b089076737158ccd2
- KEMAR IR: 02c4e54ea9161e881d2b8f2ad4223667f9d1599f43845015fbb9398d62fc9155
- KU100 model: e6aad9c150202566bc50a2c873654f42e1c0e62ac5a3e1d581972e09935172f7
- KU100 IR: 057fcaaebc9d76a87f6100d8249605077781b91f1b1346e9f314f6d567abf07c

- FABIAN model: bc9782c8a0175a2a25237ee08cc48b0e1ac1ea9a2588c58aa8b3f9eedddc6277
- FABIAN normalized IR: 0aa15f28c350e246259cb211d5543b3b95d209e454b5eee0e4e46e25d696e28f
