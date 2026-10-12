# Changelog

## 1.0.2

- Added macOS support for Apple silicon and Intel, with Audio Unit, VST3, and CLAP in a complete installer. The macOS port was contributed by [Victor Tsaran (@vick08)](https://github.com/vick08) in [PR #1](https://github.com/dgl1984/spacetrace/pull/1).
- Improved the precision of correction-filter sample-rate conversion, preserving narrow bass features when a session runs at a different rate from the correction WAVs.
- Added macOS installation instructions and automated universal-build, bundled-head, and Audio Unit validation.

## 1.0.1

- Optimized corrective equalization across the five included heads in Stereo Pair mode.
- Refined IRCAM Stereo Pair balance and output level.
- Refined the mono Dataset Corrected calibration for IRCAM LISTEN 1050, MIT KEMAR Normal Pinna, and SADIE II D1 / Neumann KU100 using Tone Trace Custom Max with the same deterministic pink-noise source for reference and target measurement.
- Recalibrated the corresponding mono output trims so corrected front-center output remains level-matched across the included heads.
- Updated correction-response plots and accompanying documentation.

Available as Windows x64 VST3 and CLAP.
