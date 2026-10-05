# Built-in SpaceTrace datasets

SpaceTrace release binaries load native `.sthrtf` packages from the external portable `Heads/` folder. The files in this directory are preparation and validation inputs, not embedded plug-in assets. The runtime also includes external TH Koeln KU100 FULL2DEG and FABIAN HATO 0 packages; see `Heads/` for their manifests and licenses. End users do **not** download HRTFs on first run. The original official SOFA files are build inputs and are intentionally omitted from compact development checkpoints after validation.

`scripts/Prepare-Datasets.ps1` reproduces the IRCAM and KEMAR packages from their official SOFA sources. `scripts/Prepare-SADIE-D1.ps1` downloads the official SADIE II D1 archive from Zenodo when needed, verifies Zenodo's published archive MD5, extracts the 44.1 kHz / 256-tap D1 SOFA, converts it, embeds the measured KU100 correction, and validates the resulting package against both the official SOFA and the correction WAV.

For the 1.0 source release, validated native packages are intended to be committed with the source so a normal build does not depend on upstream URLs remaining unchanged. The preparation scripts remain as optional provenance/regeneration tools.

## Default: IRCAM LISTEN 1050

`IRC_1050_R_44100.sthrtf` is generated from the official IRCAM LISTEN subject 1050 SOFA file. SpaceTrace converts each 8192-sample source HRIR to a 128-tap minimum-phase kernel and stores explicit per-ear delay on a quarter-sample grid. This keeps the compact realtime representation separate from localization timing.

Official source SHA-256:
`5efc0dcc002551313b60a9bce6c957db1b07c8d25d1474c9f9333c5175c4dd83`

The package contains the fixed global tonal-restoration filter derived from the user's Raw IRCAM Tone Trace match. The same filter is applied to both ears after binaural rendering and never changes with source position. Compensation identity: `tonetrace-raw-v1`.

## Alternative: SADIE II D1 / Neumann KU100

`sadie2_d1_ku100_44100.sthrtf` is generated from the official SADIE II D1 44.1 kHz / 256-tap HRIR SOFA file. D1 is the Neumann KU100 dummy head. The source dataset contains 8,802 measurement directions at a 1.2 m measurement radius.

Its `tonetrace-raw-v1` correction is the user's measured single global restoration filter. No direction-dependent correction is synthesized; the raw KU100 HRIRs remain untouched inside the package and the correction is stored separately.

## Alternative: MIT KEMAR Normal Pinna

`mit_kemar_normal_pinna.sthrtf` contains the official 512-sample KEMAR HRIRs, with coordinates normalized to SpaceTrace's listener convention.

Official source SHA-256:
`e7035994f5fd754058424c061380ee92b1d5ed58fccef2887a4266916616acdf`

Its `tonetrace-raw-v1` tonal-restoration filter is likewise one common post-binaural filter, identical for both ears and independent of source position.

No built-in dataset uses per-position tonal correction.
