# Papa Pan provenance and fidelity boundary

## Public historical evidence

The Papa Engine was the proprietary real-time binaural engine used by Papa Sangre-era titles and later Audio Defence. Public reverse-engineering work on Audio Defence documents the historical binaural panner in unusually concrete detail.

The relevant evidence reports:

- IRCAM LISTEN subject 1050 as the HRTF basis;
- only elevation-zero measurements retained for the binaural panner;
- 24 horizontal directions at 15-degree spacing;
- nearest-HRTF direction selection;
- direction index movement limited to at most one step per 256-frame processing buffer;
- a front-hemisphere output gain contour from 1.0 to 1.5;
- IRCAM azimuth orientation with +90 degrees toward listener-left.

Reference reviewed during this integration:
`lbk2907/AudioDefence-Windows`, especially `tools/build_hrtf.py` and the S3D engine reconstruction.

The original Papa Engine site also describes the technology as real-time HRTF processing around the full 360-degree horizontal field. Crucially for input topology, its own middleware description says that 3D sound is created on the fly from **mono recordings**, while mono and stereo playback are listed separately as **non-3D** sound capabilities. A Creative Industries Council case study of Somethin' Else repeats the same distinction: dynamic 3D processing from mono recordings, with ordinary mono/stereo playback alongside it.

Historical references reviewed for this boundary:
- `https://papaengine.wordpress.com/` (Papa Engine middleware description)
- Creative Industries Council, “Tech Case: The 3D Audio World of Papa Sangre”

## What SpaceTrace reproduces

Papa Pan 0.5.0 reproduces the behaviours above using SpaceTrace's official/public IRCAM LISTEN 1050 source package. The historical 256-frame cadence is converted to a time interval so the renderer has the same movement timing at non-44.1-kHz host rates.

## What SpaceTrace does not reproduce

The reverse-engineering report identifies an apparent partitioned-convolution quirk: a later multiplication overwrites an earlier partition product, leaving only part of the intended convolution contribution. That may be part of the exact historical binary sound, but it is also plausibly an implementation defect.

SpaceTrace does not intentionally reproduce that quirk in the initial Papa Pan renderer. It uses correctly prepared IRCAM kernels and preserves the historical sector selection, movement limiting, and gain contour.

SpaceTrace also does not redistribute HRTF blobs extracted from Papa Sangre or Audio Defence binaries.

SpaceTrace therefore keeps Papa Pan intentionally **mono-source only**. Stereo Pair is a Modern HRTF feature and does not instantiate two Papa Pan renderers. This matches the documented historical boundary instead of inventing an undocumented stereo-spatialized Papa mode.

## Product meaning

Papa Pan is therefore a historically grounded **character renderer**, not a claim of bit-identical emulation of a proprietary game binary. Modern HRTF remains SpaceTrace's technically transparent continuous 3D renderer.
