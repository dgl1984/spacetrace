# SpaceTrace head-preparation scripts

`python scripts/prepare_mono_level.py Heads/IRCAM_1050` measures the fixed
Modern Mono Point Source reference gain at front-center, Dataset Corrected,
using 80–16000 Hz pink weighting. Add `--write` to store `monoTrimDb` in the
manifest and its recipe in provenance. This leaves all audio assets unchanged.
Run it after finalizing the global correction and ear/head calibration; repeat
it if those inputs change. Optional monoTrimDb defaults to 0 for older heads
and must be within ±6 dB. It does not apply to Stereo Pair or Papa Pan.

These scripts make the custom-head workflow reproducible. They are **asset-preparation tools, not plug-in runtime dependencies**. SpaceTrace itself does not require Python, NumPy, h5py, SOFA libraries, or any of these scripts at runtime.

## Why SpaceTrace converts SOFA instead of loading it directly

SOFA is the archival/interchange source format and should be kept. SpaceTrace's `.sthrtf` is a derived runtime format built for the plug-in: it uses the data layout the renderer needs, avoids shipping an HDF5/SOFA parser in every plug-in binary, participates directly in the manifest/SHA-256 identity system, and loads efficiently into the process-wide immutable head cache.

Conversion deliberately normalizes source coordinates into SpaceTrace's convention and stores HRIR samples as float32. A source SOFA may contain float64 samples and metadata or dimensions that SpaceTrace does not need. That means `.sthrtf` should never be treated as a replacement archive for the original SOFA. Keep the SOFA and its license/provenance; regenerate the runtime package from it when needed.

## Install the preparation environment

Python 3.10 or newer is recommended.

Windows:

```text
python -m venv .venv
.venv\Scripts\python -m pip install -r scripts\requirements.txt
```

macOS/Linux:

```text
python3 -m venv .venv
.venv/bin/python -m pip install -r scripts/requirements.txt
```

The tools intentionally use the conventional `scripts/requirements.txt` file. The current preparation dependencies are NumPy, h5py, and matplotlib. Matplotlib is used only to generate documentation plots; the plug-in has no runtime dependency on it.

## 1. Convert a SOFA into a Raw SpaceTrace head

```text
python scripts/convert_sofa_head.py MyHead.sofa Heads/MyHead \
  --stable-id mylab.myhead.48000 \
  --display-name "My Head" \
  --source-url https://example.org/MyHead.sofa
```

Add `--license-file path/to/LICENSE.txt` when the source has a separate license/notice file. The command validates the source shape/rate before writing anything important, creates `head.sthrtf`, writes `manifest.json` and `provenance.json`, and leaves the head **Raw-only**.

Stable IDs are not display names. Once users can save projects with a head, its stable ID should not be casually changed.

## 2. Validate the package

```text
python scripts/validate_head.py Heads/MyHead --heads-root Heads
```

Validation checks the manifest schema, stable ID, native package, SHA-256 hashes, optional normal and Stereo Pair corrections, sample-rate consistency, calibration ranges, and duplicate stable IDs among sibling head folders.

## 3. Optional: make a Tone Trace correction

A correction is not required for a valid head. Raw-only heads are supported.


Tone Trace is a separate LanesAudio tool and is not required to run SpaceTrace. The official project and current releases are:

`https://github.com/dgl1984/ToneTrace`

`https://github.com/dgl1984/ToneTrace/releases`

Recommended SpaceTrace/Tone Trace workflow:

1. Use unprocessed pink noise as the Tone Trace **reference**.
2. Load this exact converted head in SpaceTrace and select **Raw**.
3. Put the source at **0 degrees, front-center**.
4. Render the same pink noise through that Raw head and use it as the Tone Trace **target**.
5. Create/export the correction from that comparison.
6. Do not borrow another head's correction, even when two datasets use the same dummy head model.

### Normalize the correction without changing its EQ shape

```text
python scripts/normalize_correction.py correction.wav correction-neutral.wav
```

By default this applies one constant gain so ideal pink-noise RMS is unchanged over 80 Hz-16 kHz. The detailed correction curve is untouched. If the head itself remains louder or quieter than the others, fix that with the separate `levelTrimDb` described below—not by hiding level matching inside the correction.


### Optional: prepare Stereo Pair correction assets

The shipping residual refinements for heads 1, 3 and 4 use the separate offline
composition procedure below. Do not rerun the initial calibration procedure on
those combined responses: it would change the gain policy of the approved residuals.

For a head that already has an approved normal dataset correction, Stereo Pair can optionally carry two side-reference corrections. With SpaceTrace in **Raw**, Stereo Pair center Azimuth **0 degrees**, Width Offset **0 degrees**, and Elevation **0 degrees**, make one Tone Trace correction for the Left source at **+90 degrees** and one for the Right source at **270 degrees**. Use the same unprocessed pink-noise Reference for both.

Then run:

```text
python scripts/prepare_stereo_pair.py Heads/MyHead left-90.wav right-270.wav
```

The tool copies normalized assets as `stereo_pair_left_correction.wav` and `stereo_pair_right_correction.wav`, updates their SHA-256 values, derives equal-and-opposite source-leg balance, and derives one fixed centered-pair level trim. It changes each correction only by one constant normalization gain; it does not alter the learned EQ shape. The calibration is deterministic and pink-weighted over 80 Hz-16 kHz.

At runtime, Dataset Corrected Stereo Pair uses these two corrections **before the two binaural source legs are summed**. A package without them remains valid and falls back to the normal dataset correction. Raw remains Raw. Custom Correction IR remains the user's one common correction.

The Stereo Pair calibration is not AGC or limiting. It is fixed package metadata measured at the default centered 90/270-degree geometry with Width Offset 0°. The plug-in may move the pair up to ±45° with Width Offset, but it intentionally keeps the same reference corrections rather than maintaining a per-angle correction bank.

### Add a common residual to an existing correction pair

For a residual measured with the existing Dataset Corrected Stereo Pair active,
compose it with both preserved base WAVs at the same sample rate:

```powershell
python scripts/compose_corrections.py base_left.wav residual.wav combined_left.wav
python scripts/compose_corrections.py base_right.wav residual.wav combined_right.wav
```

This writes the full linear convolution as a mono float32 WAV without normalizing
either component. Keep the original source gains, ear gains, head trim and pair
trim. Install the combined WAVs in the head package, update their manifest SHA-256
values and correction version, and retain the inputs and recipe in provenance.
Never use an already combined WAV as the base for the same residual again.
Regenerate the left/right plots from the actual combined WAVs and validate the head.
The shipped recipes and source hashes are in each affected head's provenance.json;
their inputs are in Docs/Correction_Models/StereoPair/Components/<head>/.
This does not generate a merged Tone Trace measurement model or a new calibration.
For the shipping heads 1, 3 and 4, add `--tail-fade-ms 40` to both commands. This
applies a half-cosine taper only to the final 40 ms, reaching zero smoothly with
no normalization. The measured magnitude change for these six responses is below
0.001 dB over 20 Hz-20 kHz. Other inputs require checking the effect of a fade;
the command does not infer audibility or automatically choose a fade duration.
The bundled combined responses are about 360 ms and the plugin reports a 400 ms
tail. Longer custom compositions require reviewing the host tail report as well.

## 4. Measure front-center channel balance

A nominal 0-degree source is not guaranteed to be perfectly centered in every measured dataset. SpaceTrace keeps this separate from tonal correction and overall head loudness. Measure it explicitly:

```text
python scripts/analyze_head_channels.py Heads/MyHead --write-manifest
```

The analyzer finds the real 0-degree azimuth / 0-degree elevation measurement, compares pink-weighted left/right energy over 80 Hz-16 kHz, and writes equal-and-opposite `leftGainDb` / `rightGainDb` values. Equal-and-opposite gains center the nominal front response and preserve the geometric mean of the two channel gains. They do not preserve total stereo energy exactly; `levelTrimDb` remains a separate calibration.

The tool refuses large corrections by default. If the nominal front direction is several dB off center, that may indicate a coordinate or dataset problem rather than something SpaceTrace should automatically hide. Inspect it before overriding the safety limit.

This calibration is separate from both:

- `correction.wav`, which changes global tonality; and
- `levelTrimDb`, which matches overall loudness between heads.

## 5. Make correction plots and text descriptions

The PNG files are documentation only. SpaceTrace does not need them for audio processing, so there is no reason to carry image decoding or extra graphical state in the plug-in. Users can generate the same plots themselves from the actual correction WAV or the Tone Trace model:

```text
python scripts/plot_correction.py correction-neutral.wav correction_response.png \
  --title "My Head — Dataset Correction"

python scripts/plot_correction.py MyHead.ttm tone_trace_model.png \
  --title "My Head — Tone Trace Correction Model"
```

Each command also writes a `.txt` file beside the PNG. The text description is derived from the plotted data and reports the strongest boost, deepest cut, and broad-band averages. This gives a quick audit trail and a useful non-visual description without pretending the plot says anything about localization quality.

The plot tool has the same defensive behavior as the other preparation tools: missing dependencies, bad filenames, unsupported source types, malformed Tone Trace models, invalid correction WAVs, bad axis ranges, unwritable destinations, and accidental overwrites produce actionable errors.

## 6. Attach the approved correction and/or level trim

```text
python scripts/package_head.py Heads/MyHead \
  --correction correction-neutral.wav \
  --correction-name "My Head Tonal Restoration" \
  --correction-version tonetrace-fixed-v1 \
  --level-trim-db -2.1
```

Then run `validate_head.py` again.

`levelTrimDb` is a fixed calibration for head-to-head loudness matching. It is deliberately separate from the tonal correction.

## Error behavior

The scripts are intended to fail loudly and usefully. Expected user/configuration problems print `ERROR:` plus a `HOW TO FIX:` line and return a non-zero exit code instead of presenting a Python traceback as the primary diagnosis.

Common examples include:

- missing or misspelled input paths;
- missing NumPy/h5py;
- a file that is not actually SOFA/HDF5;
- missing `Data.IR`, `Data.SamplingRate`, or `SourcePosition`;
- unsupported receiver count or SOFA geometry;
- invalid/duplicate stable IDs;
- refusing to overwrite an existing head/correction unless explicitly requested;
- bad manifest JSON;
- head/correction hash mismatch;
- invalid correction sample rate/data;
- invalid correction WAV;
- unwritable/invalid output paths.

If a conversion fails, keep the original SOFA. It is always the authoritative source.

## Low-level/internal scripts

`sofa_to_sthrtf.py` remains the low-level conversion engine used by release preparation. Most users should call `convert_sofa_head.py`, which adds the package manifest, provenance, identity checks, safer overwrite behavior, and clearer diagnostics around that engine.
