# Making Your Own SpaceTrace Head from a SOFA File

After finalizing the global correction and channel/head gains, run
`python scripts/prepare_mono_level.py Heads/YourHead` to measure a mono level
trim at front-center (80–16000 Hz pink-weighted power). Add `--write` to save it.
The optional manifest field `monoTrimDb` defaults to 0 for older packages and
must be within ±6 dB. This fixed gain applies once in Modern Mono Point Source,
including Raw/Custom, and does not apply to Stereo Pair or Papa Pan. Regenerate
it whenever its input correction or head/channel gains change.

SpaceTrace does not load SOFA files directly while the plug-in is running. That is deliberate, and it is not because SOFA is a bad format. Quite the opposite: **SOFA is the source and archival format you should keep.** SpaceTrace converts compatible SOFA HRTFs into a small native runtime package called `.sthrtf` because the plug-in has a much narrower job than a general acoustics tool.

The native package gives SpaceTrace exactly what its renderer needs, in the layout it expects, without putting an HDF5/SOFA parser into every VST3 or CLAP instance. It also lets the external `Heads/` system use stable IDs, SHA-256 identity checks, lazy preparation, and the shared immutable head cache that keeps multi-instance project loading fast.

There is a tradeoff. Conversion is not a lossless archive of every SOFA field. Depending on the source, HRIR samples may be reduced from float64 to float32, coordinates are normalized into SpaceTrace's coordinate convention, and metadata or dimensions that SpaceTrace does not use are not carried into the runtime file. The directional HRIR data needed by the supported renderer is preserved, but the original SOFA remains the authoritative source. **Keep it.** If the SpaceTrace format changes later, the SOFA is what you regenerate from.

## What you need

The preparation tools live in `scripts/` and are separate from the plug-in. SpaceTrace itself has no Python dependency.

Create a Python environment and install the required packages from the conventional requirements file.

Windows:

```text
python -m venv .venv
.venv\Scripts\python -m pip install -r scripts\requirements.txt
```

macOS or Linux:

```text
python3 -m venv .venv
.venv/bin/python -m pip install -r scripts/requirements.txt
```

Run any tool with `--help` before using it if you want the full argument list and examples. The scripts are supposed to tell you what went wrong and how to fix it. If you get an ordinary setup or input error, the intended result is an `ERROR:` line followed by a `HOW TO FIX:` suggestion rather than a Python traceback being the only explanation.

## Step 1: Convert the SOFA

A typical conversion looks like this:

```text
python scripts/convert_sofa_head.py MyHead.sofa Heads/MyHead \
  --stable-id mylab.myhead.48000 \
  --display-name "My Head" \
  --source-url https://example.org/MyHead.sofa
```

The stable ID is important. It is the identity saved in projects, not merely a filename or display label. Once people can save sessions using a head, changing its stable ID casually is a good way to make old projects unable to find it.

The converter validates the SOFA before creating the package. A successful Raw head folder contains at least:

```text
Heads/
    MyHead/
        head.sthrtf
        manifest.json
        provenance.json
```

If you supply a license file, that is copied into the package as well.

The new head is intentionally **Raw-only** at this point. That is a valid SpaceTrace head. You do not need to invent or borrow a correction merely to use it.

## Step 2: Validate it

Run:

```text
python scripts/validate_head.py Heads/MyHead --heads-root Heads
```

This checks the manifest, native package, hashes, stable-ID uniqueness, correction metadata if present, sample-rate consistency, and fixed trim range.

If validation fails, fix the reported problem instead of editing hashes until the warning goes away. The hash is there to tell SpaceTrace which exact data a project refers to.

## Step 3: Listen to the Raw head first

Before correcting anything, make sure the head actually localizes well and that movement behaves properly. A tonal correction can make a useful head easier to listen to, but it cannot turn a poor directional dataset into a good one.

For a custom head, I would listen to front, sides, rear, vertical movement, and a slow circle before spending time matching the tone.

## Step 4: Optional Tone Trace correction

SpaceTrace's normal **Mono Point Source** path uses one fixed global correction for the whole dataset. It is not a per-position EQ. The idea is to reduce the head's overall tonal coloration while leaving the directional changes that make the HRTF useful.

SpaceTrace can also carry an **optional Stereo Pair correction pair** for the centered stereo reference geometry: input Left at +90 degrees and input Right at 270 degrees. Those two correction IRs are applied separately to the two binaural source legs before they are summed. Each IR remains mono/common to both ears of its source leg, so it does not alter that leg's left/right HRTF ratio or timing. The pair is intended to make ordinary stereo program material substantially more tonally transparent at the canonical 90°/270° reference geometry (Stereo Pair center Azimuth 0°, Width Offset 0°). It is not a general per-azimuth flattening system. Width Offset may move the sources by up to ±45° for creative placement; the same reference corrections remain active and some tonal deviation away from the calibrated angles is expected.


Tone Trace is a separate LanesAudio tool and is not required to run SpaceTrace. The official project and current releases are:

`https://github.com/dgl1984/ToneTrace`

`https://github.com/dgl1984/ToneTrace/releases`

The recommended measurement is simple:

1. Use **unprocessed pink noise** as the Tone Trace **reference**.
2. Load the newly converted head in SpaceTrace.
3. Select **Raw**.
4. Set the source to **0 degrees, front-center**.
5. Run the same pink noise through SpaceTrace and use that processed signal as the Tone Trace **target**.
6. Create and audition the correction in Tone Trace.
7. Export the approved correction WAV.

Do not reuse another head's correction simply because the two datasets were measured on the same dummy head. Different rooms, microphones, measurement systems, grids, processing, and dataset preparation can produce different global responses. Correct the exact head you are going to ship or use.


### Optional Stereo Pair correction and loudness calibration

The procedure below creates the original pair from Raw measurements. For a common
residual measured with an existing Dataset Corrected Stereo Pair already active,
keep that original pair and compose the residual into both source-leg responses
offline. Follow "Add a common residual to an existing correction pair" in
`scripts/README.md`. Do not replace the original response with the residual or
reapply the initial normalization and gain calibration automatically.

If the head will be used with **Input Mode: Stereo Pair**, first finish and approve the normal 0-degree correction above. Then make two additional Tone Trace captures from the exact same **Raw** head:

1. keep Stereo Pair center Azimuth at **0 degrees**, Width Offset at **0 degrees**, and Elevation at **0 degrees**;
2. capture the **Left source leg at +90 degrees** using the same unprocessed pink-noise reference;
3. capture the **Right source leg at 270 degrees** using the same reference;
4. export both approved Tone Trace correction WAVs without using Correction Gain as loudness compensation;
5. run:

```text
python scripts/prepare_stereo_pair.py Heads/MyHead left-90.wav right-270.wav
```

`prepare_stereo_pair.py` deliberately keeps three jobs separate. It applies one constant normalization gain to each side correction so that correction changes tone rather than source level; it derives equal-and-opposite `stereoPairLeftGainDb` / `stereoPairRightGainDb` values so the two source legs have matched total binaural energy at the centered reference geometry; and it derives one `stereoPairTrimDb` so independent unit-power stereo pink noise has the same total pink-weighted output power as the dry stereo reference.

The plug-in uses these optional assets only for **Dataset Corrected + Stereo Pair**. If a head has no Stereo Pair correction pair, SpaceTrace falls back to the normal dataset correction. The fixed Stereo Pair source-balance and pair-level calibration are dataset metadata, not an AGC, limiter, or content-dependent loudness processor.

Do not create corrections for every azimuth merely to make every direction spectrally flat. Direction-dependent HRTF coloration contains localization information. The Stereo Pair pair is a bounded reference correction for the product's default 90/270-degree stereo geometry.

## Step 5: Make the correction level-neutral

A tonal correction should change tone, not secretly become the head's volume control.

Run:

```text
python scripts/normalize_correction.py correction.wav correction-neutral.wav
```

By default SpaceTrace measures the correction against ideal pink-noise energy over 80 Hz to 16 kHz and applies one constant gain offset. The detailed EQ shape is left alone.

That means there are two separate jobs:

- **correction.wav** fixes global tonality;
- **levelTrimDb** matches the overall loudness of this head to the others.

Keeping those separate matters. It lets Raw and Dataset Corrected stay sensibly level-matched within one head, while the fixed trim handles a head that is simply hotter or quieter than the rest of the collection.

## Step 6: Check front-center left/right balance

The 0-degree measurement can be a little left- or right-heavy even when the coordinate itself is nominally centered. Do not bury that inside the tonal correction. Measure it separately:

```text
python scripts/analyze_head_channels.py Heads/MyHead --write-manifest
```

SpaceTrace measures pink-weighted energy at the real 0-degree azimuth / 0-degree elevation HRIR and derives equal-and-opposite fixed channel gains. If the left side is 0.4 dB hotter than the right, for example, the calibration removes 0.2 dB from the left and adds 0.2 dB to the right. Overall level stays effectively unchanged; only the nominal front-center balance is corrected.

Large imbalances are refused by default because a badly off-center result can be a sign of wrong coordinates, the wrong measurement, or a problem in the source dataset.

This gives us deliberately separate calibration layers:

- `correction.wav` — Mono Point Source/global tonal restoration;
- `levelTrimDb` — overall head-to-head loudness matching;
- `leftGainDb` / `rightGainDb` — front-center output-ear balance;
- optional `stereo_pair_left_correction.wav` / `stereo_pair_right_correction.wav` — centered Stereo Pair tonal restoration for the +90/270 source legs;
- optional `stereoPairLeftGainDb` / `stereoPairRightGainDb` — equal-and-opposite Stereo Pair source-leg balance;
- optional `stereoPairTrimDb` — fixed centered Stereo Pair total-level calibration.

## Step 7: Generate correction plots

Correction plots are useful documentation, but they do not belong in the audio path and SpaceTrace does not need to load them. Generate them from the same files you actually package:

```text
python scripts/plot_correction.py correction-neutral.wav correction_response.png \
  --title "My Head — Dataset Correction"

python scripts/plot_correction.py MyHead.ttm tone_trace_model.png \
  --title "My Head — Tone Trace Correction Model"
```

The plotting script creates both a PNG and a text description derived from the curve. The description reports the strongest boost, deepest cut, and broad-band averages, which makes it useful as an audit record and for somebody who cannot see the graph.

## Step 8: Attach the correction and optional trim

For example:

```text
python scripts/package_head.py Heads/MyHead \
  --correction correction-neutral.wav \
  --correction-name "My Head Tonal Restoration" \
  --correction-version tonetrace-fixed-v1 \
  --level-trim-db -2.1
```

Then validate the folder again:

```text
python scripts/validate_head.py Heads/MyHead --heads-root Heads
```

If you do not have an approved correction yet, stop before this step and leave the package Raw-only. SpaceTrace supports that intentionally.

## What the conversion preserves, and what it does not

For the straightforward binaural `SimpleFreeFieldHRIR` datasets SpaceTrace currently targets, the conversion preserves the measured directions and left/right HRIR data needed by the renderer. The converter normalizes the coordinate representation into SpaceTrace's convention and stores runtime samples as float32.

The `.sthrtf` file is **not** intended to preserve every possible SOFA feature. A SOFA with unusual receiver layouts, unsupported dimensions, multiple incompatible listener configurations, or geometry SpaceTrace cannot represent should be rejected with an explanation rather than guessed at.

That is why the conversion tools validate aggressively and why keeping the original SOFA matters.

## Why not load SOFA directly in the plug-in?

We considered it. For SpaceTrace, the native runtime format is the better tradeoff:

- no HDF5/SOFA parser has to ship inside the audio plug-in;
- native packages are often smaller because SpaceTrace stores only the data it uses and normally uses float32 HRIR samples;
- manifests and hashes give projects stable head identity;
- loading and preparation fit directly into the shared immutable cache;
- VST3 and CLAP can use the exact same prepared data path;
- the original SOFA remains available for research, archival use, or regeneration.

The important part is that `.sthrtf` is not a private black box. The scripts in this folder are the same conversion path used for reproducible SpaceTrace asset preparation, and they are included so a user can make a compatible head without depending on LanesAudio to do the conversion for them.

## When something goes wrong

Start with the error message. The tools test and report common problems including missing or misspelled files, missing Python dependencies, invalid HDF5/SOFA input, unsupported SOFA conventions, wrong receiver counts, duplicate stable IDs, malformed manifests, hash mismatches, missing or invalid correction files, invalid correction WAVs, existing output files, and invalid output paths.

If you think you have found a valid SOFA dataset that the converter rejects, keep the original file and the complete error output. That gives us something concrete to support rather than silently accepting geometry we do not understand.
