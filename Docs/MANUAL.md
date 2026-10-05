# SpaceTrace 1.0 User Manual

Mono Point Source is calibrated to nominal input level at front-center, 1 metre,
Dataset Corrected, and 0 dB Output, using an 80 Hz–16 kHz pink-weighted reference.
The fixed adjustment preserves each head's EQ and spatial cues. It also applies
in Raw and Custom Correction, preserving their relative level differences.
Stereo Pair uses its own calibration. When folding a stereo recording to mono,
SpaceTrace averages left and right: unrelated channels can be quieter, and
opposite-polarity channels can cancel. This is expected summing behavior.

## Start here: get one sound moving first

SpaceTrace can get engineering-heavy because HRTFs are engineering-heavy. You do not need to start there. The fastest way to understand the plug-in is to put a sound in front of you and move it.

Insert SpaceTrace on a mono or stereo track, put on headphones, and start with:

- **Head:** IRCAM LISTEN 1050
- **Renderer:** Modern HRTF
- **Input Mode:** Mono Point Source
- **Tone:** Dataset Corrected
- **Azimuth:** 0°
- **Elevation:** 0°
- **Distance:** 1.00 m
- **Air Loss:** On
- **Output:** 0.0 dB

Now move **Azimuth** slowly. Once horizontal movement makes sense, try **Elevation**, and then **Distance**. I recommend learning those three separately before automating all of them at once. It is much easier to hear what is helping and what is not.

One detail matters immediately: **0° and 360° are the same physical position: straight ahead.** Increasing Azimuth from 0° initially moves the source **left**. If I want a sound to begin front-center and move **right**, I start at **360°** and move downward. Nothing is discontinuous in the physical position; I am simply choosing which end of the control gives me the direction of travel I want.

## What the controls mean in practice

### Head

**Head** chooses which measured HRTF dataset supplies the directional ear cues. This is not just an EQ choice. A different measured head can change front/back confidence, height, image width, how smoothly a circle tracks, and whether a particular direction simply clicks for you.

SpaceTrace 1.0 ships five heads:

1. IRCAM LISTEN 1050
2. MIT KEMAR Normal Pinna
3. SADIE II D1 / Neumann KU100
4. TH Köln / Bernschütz FULL2DEG KU100
5. FABIAN HATO 0

The corrected versions are intentionally normalized closely enough that changing heads can be hard to detect from broad tone or loudness alone. That is the point. I want a head comparison to be mostly about **spatial character**, not “this one is 4 dB louder” or “this one has more treble.”

Head is also a structural selection. Projects save a stable head identity and content hash so SpaceTrace can tell the difference between the exact package a project used, a missing package, and a different package carrying the same stable ID.

### Renderer

**Modern HRTF** is the normal SpaceTrace renderer. It uses the selected measured head, supports horizontal and vertical movement, interpolates between measured directions, and uses the current Distance model.

**Papa Pan — historical** is deliberately separate. It is a horizontal-only historical renderer reconstructed from documented Papa Engine behaviour while using public IRCAM LISTEN data. It is not a hidden mode of Modern HRTF and it does not use proprietary game HRTF assets.

Renderer is a structural choice rather than something I recommend automating while audio is running. Changing it can require a different prepared rendering path.

### Input Mode

**Mono Point Source** is the default. A stereo input is intentionally folded to mono and rendered as one spatial source at the selected Azimuth/Elevation. This is the simplest SpaceTrace mental model: **I have a sound; put it somewhere.**

**Stereo Pair** is available with Modern HRTF. It treats the input channels as two linked binaural sources rather than preserving hard headphone stereo:

- at Width Offset 0°, input Left = center Azimuth + 90° and input Right = center Azimuth - 90°;
- **Width Offset** is Stereo Pair only, ranges from -45° to +45°, and moves both source positions symmetrically;
- both channels share Head, Elevation, Distance, Tone, Air Loss and Output.

So at Width Offset 0°, a center Azimuth of 0° puts input Left at 90° and input Right at 270°. Center 45° produces Left 135° / Right 315°; center 90° produces Left 180° / Right 0°. With center Azimuth 0°, Width Offset -45° produces 45° / 315° (bowed toward the front), while +45° produces 135° / 225° (bowed toward the rear). The normal 0–360 wrap applies.

The spatial display mirrors this geometry: Mono Point Source shows one source marker; Stereo Pair shows two smaller L/R markers. Azimuth rotates the linked pair together and Width Offset changes their symmetric positions. The display remains read-only; Azimuth and Width Offset are the actual editable controls.

Stereo Pair remains a linear sum and has no limiter, AGC, or content-dependent normalization. Shipping heads can include optional **90°/270° Stereo Pair correction assets** plus fixed source-balance and pair-level calibration measured with pink noise at the centered 0° pair geometry. Those corrections are applied independently to the two binaural source legs before summing and identically to both ears within each leg, preserving the leg's interaural level/timing relationship. The fixed calibration makes ordinary stereo material much closer to the original tonal balance and reference level without turning SpaceTrace into a loudness processor. The shipping 90°/270° correction and level calibration are exact at **Width Offset 0°**. Moving Width Offset away from zero deliberately trades some tonal calibration accuracy for placement flexibility; SpaceTrace does not pretend to carry a different correction for every angle.

Heads 1 (IRCAM), 3 (SADIE II D1) and 4 (KU100 FULL2DEG) also include a common Tone Trace residual measured with the existing Dataset Corrected Stereo Pair active at default settings. Each residual is combined offline with both original source-leg responses, preserving its gain and all existing calibration gains. It stays active at every Stereo Pair position and adds no runtime convolution stage. The original pair trim remains the baseline calibration; it has not been recalibrated to remove any loudness change from the residual. Final combined response plots and matching text descriptions are in `Docs/Correction_Plots/StereoPair/`.

Those three heads' combined Stereo Pair responses have a smooth half-cosine fade over their final 40 ms, reaching zero without gain makeup. The measured magnitude change from the fade is below 0.001 dB over 20 Hz-20 kHz. The fade is included in the final response plots.

If a custom head has no Stereo Pair correction assets, Dataset Corrected Stereo Pair falls back to that head's normal global dataset correction. Raw remains Raw, and Custom Correction IR remains the user's one common correction. With a mono host input, SpaceTrace still renders one point source because there is no second input channel to form a pair.

Both pair members receive the same requested Elevation. SpaceTrace does not invent an automatic left/right elevation correction to compensate for natural or measurement-derived HRTF asymmetry. At extreme elevations, some heads may therefore produce a perceived tonal or height tilt between the two members of the pair.

Papa Pan remains intentionally mono-only. When Papa Pan is selected, Input Mode is unavailable in the custom editor; a stored Stereo Pair selection is remembered for when Modern HRTF is selected again.

### Azimuth

Azimuth is horizontal direction from **0° to 360°**.

- 0° = front
- 90° = left
- 180° = rear
- 270° = right
- 360° = front again

That convention is worth saying twice because many panners use a different sign convention: **increasing from 0° moves left.** For a front-to-right move, begin at 360° and automate downward.

Arrow keys move one degree. **Page Up/Page Down** move by 10°. **Home** goes to 0° and **End** goes to 360°. Enter or F2 opens exact numeric entry.

### Elevation

Elevation is vertical direction from **-90° to +90°**.

- 0° = ear level
- positive values = above
- negative values = below
- +90° = directly overhead
- -90° = directly underneath

Arrow keys move one degree. **Page Up/Page Down** move by 10°. **Home** goes to -90° and **End** goes to +90°. Enter or F2 opens exact numeric entry.

Height is one of the places where head choice matters most. Do not assume a head is bad because one particular measured geometry does not give you strong vertical cues. Try the other heads before drawing that conclusion.

### Distance

Distance runs from **0.25 m to 20.00 m**, with **1.00 m as the neutral reference**.

This is intentionally conservative. SpaceTrace does not pretend that the bundled far-field HRTFs contain measured near-field ear changes that they do not contain.

Beyond 1 m, the model uses ordinary free-field level falloff plus a small high-frequency loss when **Air Loss** is enabled. The high-frequency loss is bounded and subtle rather than turning Distance into an exaggerated low-pass effect.

Inside 1 m, SpaceTrace softens the normal inverse-distance boost. Full 1/r gain would make 0.25 m about 12 dB louder than 1 m, which sounds dramatic but implies a level/proximity certainty the source HRTFs do not support. The current model uses a softened curve instead.

So Distance means: **believable level-and-air perspective around a measured directional HRTF**, not a room simulator and not a synthetic near-field HRTF generator.

Arrow keys make the normal fine step. Home/End go to the range limits. Enter or F2 gives exact entry.

### Width Offset

Width Offset runs from **-45° to +45°**, with **0° as the calibrated reference**. It is available only in Modern HRTF Stereo Pair. Minus values bow the two sources toward the front of the pair; plus values bow them toward the rear. Azimuth rotates that whole arrangement together.

It has no effect in Mono Point Source or Papa Pan, even if a host restores or automates a nonzero value. Arrow keys move one degree, Home/End select -45°/+45°, and Enter/F2 opens exact numeric entry. I leave it at zero when comparing the prepared head corrections, then move it when the placement calls for it.

### Air Loss

Air Loss controls only SpaceTrace's synthetic high-frequency air absorption beyond 1 m. It defaults **On** to preserve the intended Distance behavior.

Turning Air Loss **Off** does **not** disable Distance: the ordinary level falloff beyond 1 m and the softened proximity gain inside 1 m continue normally. It simply makes the high-frequency distance stage spectrally transparent.

At exactly **1.00 m**, Air Loss On and Off are identical: the air-loss gain is unity, so the default/reference Distance removes no high-frequency information.

### Tone

Tone has three jobs, and keeping them distinct is important.

**Raw** gives you the converted HRTF dataset without SpaceTrace's fixed tonal restoration. Raw is what I use when I want to hear what the dataset itself is doing, audition a new custom head, or measure a new correction.

**Dataset Corrected** applies the approved fixed correction for that head. Mono Point Source uses the global minimum-phase correction. Stereo Pair uses the optional fixed side-reference corrections before summing, or the normal global correction when a custom head has no pair assets. These corrections do not change from angle to angle. That is deliberate: I want to reduce broad dataset coloration without equalizing away the directional spectral differences that are doing the localization work.

**Custom Correction IR** replaces the built-in dataset correction with a correction IR you load. This is a tonal-correction slot, not a general-purpose reverb/convolution slot. If you feed it a reverb, you are no longer using it for the job the renderer and documentation assume.

Tone is a structural selection rather than a continuously automatable EQ mode. Home/End on the combo box select its first/last item.

### Output

Output is a final trim from **-24.0 dB to +12.0 dB**. It is deliberately last in the user-facing signal path. Use it when the rendered result actually needs a level adjustment; do not use it to compensate for a head that was packaged incorrectly.

The bundled heads already carry separate head-to-head level trim and front-center channel calibration where needed. Output is your mix control, not part of that factory calibration.

### Bypass

SpaceTrace exposes one real **Bypass** parameter. The host wrapper is pointed at that same parameter rather than creating a second hidden or duplicate bypass. Entering bypass also clears stale renderer/correction history so coming back from bypass does not drag an old convolution tail into the new state.

## The five shipping heads

There is no universally best HRTF. A measured head that works wonderfully for me may be less convincing for somebody with different pinna geometry. The five-head set exists to give you genuinely different spatial candidates without making head switching a loudness/EQ contest.

### 1. IRCAM LISTEN 1050

IRCAM 1050 is the reference/default head and is also the public dataset used by Papa Pan.

Its runtime preparation is unusual among the five. The long IRCAM measurements are represented as a 128-tap minimum-phase ear shape plus explicit per-ear delay. That keeps the directional spectral shape compact while retaining the important broadband interaural timing relationship.

If you do not know where to start, start here.

### 2. MIT KEMAR Normal Pinna

This is the classic MIT KEMAR normal-pinna dataset. Its measurement geometry is quite different from IRCAM, which makes it useful as more than a cosmetic alternative.

KEMAR is also a good reminder that “dummy head” is not one generic sound. The pinnae, measurement grid, source processing and dataset history all matter.

### 3. SADIE II D1 / Neumann KU100

This is subject D1 from SADIE II, measured with a Neumann KU100. It has been a particularly useful horizontal alternative in listening, with convincing circular movement for some listeners.

Do not interchange its correction with the FULL2DEG KU100 below. Both say KU100, but they are different measurements and different datasets.

### 4. TH Köln / Bernschütz FULL2DEG KU100

FULL2DEG is a dense KU100 set with **16,020 measured positions**. SpaceTrace preserves it as its own head with its own correction, head trim and front-center calibration.

This dataset also has the strictest redistribution boundary of the five. The source SOFA and the author's publication identify CC BY-SA 3.0, so SpaceTrace treats the converted `.sthrtf` and, conservatively, the FULL2DEG-specific correction/model/plot material as CC BY-SA 3.0 adaptations. That does **not** turn unrelated SpaceTrace code or the other head packages into BY-SA material merely because they are distributed together.

### 5. FABIAN HATO 0

FABIAN HATO 0 earned its place because it has produced unusually convincing vertical movement in listening, including motion rising or falling directly in front and behind. Rear horizontal placement can be softer than some of the alternatives. I would rather document that tradeoff than equalize or process it until every head behaves the same.

The source metadata also matters here: portions below 200 Hz and elevations below -64° include numerically modelled substitutions made by the FABIAN dataset authors. SpaceTrace preserves that fact in provenance instead of presenting everything in the runtime package as directly measured.

## Raw vs Dataset Corrected vs Custom Correction IR

This is where several independent calibrations can easily get confused.

### Dataset correction changes tone

The bundled `correction.wav` is a **global tonal restoration**. It is designed from a front-center Raw measurement and applied identically regardless of position. The shape is normalized against a defined pink-weighted reference over 80 Hz–16 kHz so it is not secretly doing the overall head-loudness job.

### `levelTrimDb` matches overall head level

After correction is settled, a separate fixed head trim is used where needed to bring the bundled heads into a sensible common loudness neighborhood. This is why switching among corrected heads may now be surprisingly subtle in broad EQ/loudness.

### Left/right front-center calibration centers nominal front

A third value pair, `leftGainDb` and `rightGainDb`, can correct a small measured L/R imbalance at nominal 0°/0°. Those gains are equal-and-opposite, so their geometric mean stays neutral. They are not a replacement for the global head trim.

That separation is intentional:

- normal correction = Mono Point Source/global tone;
- head trim = overall head level;
- L/R calibration = nominal front-center output-ear balance;
- optional Stereo Pair side corrections = centered 90°/270° stereo-source tone;
- optional Stereo Pair source gains = centered left/right source-leg balance;
- optional Stereo Pair trim = fixed centered-pair reference level.

If a custom head needs these calibrations, measure them separately instead of baking level or balance into one mystery IR. `Docs/CUSTOM_HEADS.md` documents the reproducible preparation sequence.

## Papa Pan — why it exists

Papa Pan is there because the old Papa Engine had a recognizable movement character that is interesting in its own right. Recreating that character inside Modern HRTF would make the modern renderer harder to reason about, so I keep it separate.

The current Papa Pan path uses a horizontal IRCAM ring with 24 directions, one every 15°. It chooses the nearest historical sector rather than continuously interpolating the Modern HRTF way, advances movement at the documented historical cadence, and applies the front-hemisphere gain contour inside that renderer.

In practice:

- use **Modern HRTF** for normal 3D work, elevation and the current continuous renderer;
- use **Papa Pan** when you specifically want the historical horizontal behaviour.

When Papa Pan is selected, Input Mode, Head, Elevation and Width Offset are unavailable because Papa Pan is intentionally one horizontal mono point source. Stereo Pair belongs to Modern HRTF and does not create two Papa renderers.

## Accessibility and keyboard use

SpaceTrace follows a hard rule: **one parameter, one user-facing control**.

The visual position display is output-only. It reflects Azimuth/Elevation state for sighted users, but it does not take keyboard focus, accept mouse edits, appear as another screen-reader slider, or write automation. I do not want a blind user and a sighted user operating two different interfaces that can drift apart.

There is also no private OneCore/MSAPI/NVDA speech layer. The ordinary JUCE controls expose ordinary accessibility information and the screen reader reads that.

Normal Tab/Shift+Tab navigation follows Head Model, Renderer, Input Mode, Tonal Compensation, Load Custom IR, Azimuth, Elevation, Distance, Width Offset, Air Loss, Output, and Bypass. Controls that do not apply are disabled and skipped. Ending exact entry returns focus to its owning slider.

### Keyboard reference

On sliders:

- Arrow keys: fine change
- Home: minimum
- End: maximum
- Page Up/Page Down: 10° coarse changes on Azimuth and Elevation
- Enter or F2: exact numeric entry

On Renderer, Input Mode, Head and Tone combo boxes:

- Arrow keys: previous/next item
- Home: first item
- End: last item
- Enter: open the normal combo box menu

The Home/End/Page key behaviour is supplied explicitly by SpaceTrace because JUCE 8.0.12 does not provide those commands for these controls by default.

Air Loss and Bypass are ordinary toggle controls and remain reachable by normal Tab navigation and screen-reader interaction.

The host's generic parameter view remains a fallback for automatable parameters, including Input Mode, Width Offset and Air Loss. Renderer, Head and Tone are structural state because changing them can require prepared DSP resources; they are intentionally not another set of realtime automation parameters.

## Missing or changed heads

SpaceTrace's head files live outside the plug-in binaries. A project stores the selected head's stable identity and content hash.

If the selected head package is missing, SpaceTrace reports that instead of silently loading IRCAM and pretending the project is unchanged.

If a package with the expected stable ID exists but its data hash differs, SpaceTrace can report that the installed data differs from what the project saved. This is why a distributed custom head's stable ID should not be casually reused for materially different data.

A head folder is expected to contain at least:

- `head.sthrtf`
- `manifest.json`
- `provenance.json`
- `LICENSE.txt`

A corrected head also carries its correction WAV and hashes/metadata in the manifest.

## Making a custom head from SOFA

The full step-by-step workflow is in `CUSTOM_HEADS.md`. The Python tooling is deliberately shipped with the portable release so `.sthrtf` is not a private format you have to ask me to make for you.

The practical sequence is:

1. Keep the original SOFA permanently.
2. Read the source dataset's license before converting it.
3. Install `scripts/requirements.txt` in a Python virtual environment.
4. Convert with `scripts/convert_sofa_head.py`.
5. Validate with `scripts/validate_head.py`.
6. Audition the head in **Raw** mode before correcting anything.
7. Measure nominal front-center L/R balance if necessary.
8. Optionally create a Tone Trace correction.
9. Normalize that correction for tonal neutrality.
10. Keep overall head trim separate.
11. Generate the correction PNG and deterministic text description.
12. Validate again after packaging the correction/calibration.

Every public preparation script has `--help`, explicit diagnostics and non-zero failure exits. The test suite also exercises bad paths, missing dependencies/resources, malformed SOFA, bad manifests, hash mismatches, invalid correction WAVs and other failure cases. A conversion tool is not reproducible if the only useful response to a bad input is a Python traceback.

## The Tone Trace correction workflow

Tone Trace is optional. You do **not** need it to run SpaceTrace or to use the bundled corrected heads. You need it only when you want to make or revise this style of fixed correction.

Official project:

`https://github.com/dgl1984/ToneTrace`

Current releases:

`https://github.com/dgl1984/ToneTrace/releases`

For a new SpaceTrace head, use:

- **Tone Trace Reference:** unprocessed pink noise;
- **Tone Trace Target:** the exact same pink noise rendered through the new SpaceTrace head in **Raw** mode at **0° Azimuth / 0° Elevation**.

Then export the correction and normalize it:

```text
python scripts/normalize_correction.py correction.wav correction-neutral.wav
```

That normalization applies one constant gain so the correction changes the curve without quietly becoming the head-loudness calibration.

If the corrected head is still globally louder or quieter than the shipping set, change `levelTrimDb` separately. If nominal front-center is offset between the ears, measure `leftGainDb`/`rightGainDb` separately. Do not solve three different calibration problems by hiding all of them in one correction WAV.

## Correction plots and text descriptions

The correction PNGs in `Docs/Correction_Plots/` are documentation, not runtime assets. The plug-in never needs to decode them.

You can regenerate the response of a correction WAV with:

```text
python scripts/plot_correction.py correction.wav correction_response.png
```

or inspect a Tone Trace model directly:

```text
python scripts/plot_correction.py MyHead.ttm tone_trace_model.png
```

The same command writes a deterministic `.txt` description derived from the plotted numeric data. It reports broad-band behavior and notable boost/cut regions. That text is useful both as an audit artifact and as a non-visual description; it does **not** claim to describe localization quality.

## SOFA is the source; `.sthrtf` is the runtime file

I do not want `.sthrtf` to become a mysterious replacement for SOFA.

**SOFA is the archival/interchange source. Keep it.** It carries the dataset's original structure, metadata and provenance in a standard container.

`.sthrtf` is SpaceTrace's prepared runtime representation. The conversion exists for practical reasons:

- the plug-in does not need an HDF5/SOFA parser in every binary;
- samples are stored in the layout the renderer actually uses;
- runtime HRIR storage can be float32 even when a source SOFA used float64;
- coordinates are normalized to SpaceTrace's convention;
- stable IDs and SHA-256 hashes fit directly into project/head identity;
- the same prepared head can be shared by VST3 and CLAP instances in a process;
- startup does not require reparsing a large SOFA every time an instance is created.

What conversion preserves includes the directional HRIR content needed by the renderer, source sample rate, measurement positions after coordinate normalization, ear timing data where represented, and source provenance/license metadata needed to reconstruct the origin.

What can change includes container format, numeric precision, coordinate representation, and omission of source metadata/dimensions the runtime does not consume. IRCAM 1050 additionally uses the documented SpaceTrace minimum-phase-plus-delay preparation.

That is why the original SOFA and its license/provenance must remain the archival authority.

## Deeper engineering notes

### One processor/editor/state implementation for VST3 and CLAP

SpaceTrace has one JUCE `SpaceTrace` target. The VST3 wrapper and CLAP wrapper sit around the same processor, editor, APVTS parameters, structural state, core renderer and head repository. There is no separate “CLAP DSP” that can quietly drift away from the VST3 version.

### Direction interpolation and movement

Modern HRTF finds the elevation rings surrounding the requested position, finds the azimuth neighbors on each ring, and blends up to four directional measurements. Ear delay is blended alongside the directional kernels. When a new position kernel is requested, the realtime renderer transitions from the current kernel instead of hard-switching an impulse response mid-stream.

If a dataset geometry cannot provide the expected ring neighbors, the renderer has a nearest-measurement fallback rather than fabricating invalid indices.

### Shared head loading

The head repository locates the portable `Heads/` folder, validates manifest identity/hashes, and prepares a head for the host sample rate. Parsed source packages and prepared immutable head data are cached process-wide. Multiple plug-in instances at the same head/hash/calibration/sample-rate combination can therefore share that prepared immutable data.

Each plug-in instance still owns its own convolution history, position transitions, compensation history and automation state. Sharing the immutable head does not make instances share audio history.

### Sample-rate handling

A shipping head keeps its native source rate in `.sthrtf`. When the host runs at another rate, SpaceTrace prepares the HRTF and fixed correction for the host rate before publishing that structural state to the realtime path.

For ordinary measured FIRs, the resampler uses a windowed-sinc path with gain scaling appropriate to FIR coefficients. For data represented as separated minimum-phase shape plus explicit delays, the spectral shape is reconstructed/resampled as minimum phase and the explicit delays are scaled to the new sample rate. Dataset corrections use the same minimum-phase-oriented resampling approach.

That preparation happens outside the audio callback. The realtime callback consumes already prepared state; it does not parse files, resample heads, allocate a new dataset or open a file while processing audio.

### Correction philosophy

The correction system is intentionally conservative. It is not trying to make every HRTF identical. If it did, it would eventually erase the very pinna/directional differences we need.

I correct broad global tonal bias, level-match the shipping heads separately, and correct small nominal front-center channel imbalance separately. Then I leave the spatial differences alone and let you choose the head that works for your ears.

### Provenance and licensing

Every shipping head has its own `LICENSE.txt` and `provenance.json`, and the portable release has `Licenses/THIRD_PARTY_NOTICES.txt`. The first-party `LICENSE.txt` does not blanket-license the five HRTF datasets.

The first-party SpaceTrace license is **Apache License 2.0 with the Commons Clause License Condition v1.0**, matching the model used by Tone Trace. That permits source inspection/modification/distribution subject to the license while the Commons Clause restricts selling the covered SpaceTrace software. Because of that added restriction it should be described as **source-available**, not OSI open source.

That choice does not replace third-party terms:

- IRCAM LISTEN retains the IRCAM notice and acknowledgment requirements/requests;
- MIT KEMAR retains the Gardner/Martin citation condition;
- SADIE II D1 retains University of York's Apache 2.0 terms and dataset-reference requirement;
- FULL2DEG converted/head-specific derivative material is treated as CC BY-SA 3.0;
- FABIAN converted/head-specific response-derived material is treated as CC BY 4.0;
- JUCE and the CLAP wrapper/dependencies retain their own licenses.

One release-administration point matters here: the selected SpaceTrace first-party model is not the AGPL route. A public binary release therefore needs an applicable JUCE 8 licensing route that permits the intended distribution. `Licenses/JUCE_NOTICE.txt` records that boundary; SpaceTrace's own licence cannot grant JUCE rights.

For exact redistribution terms, use `Licenses/THIRD_PARTY_NOTICES.txt` and the head-local notices rather than this summary.

## Portable installation and release layout

The reference Windows release is a portable `SpaceTrace` folder. Its root is intentionally small:

```text
SpaceTrace/
    SpaceTrace.vst3
    SpaceTrace.clap
    README.md
    LICENSE.txt
    Heads/
    Docs/
    Licenses/
    scripts/
```

`Docs/` contains user/reproducibility documentation and correction documentation. `scripts/` contains only the custom-head tools that users may actually need. Build scripts, source code, WIP handoffs, internal audits, release-checkpoint reports and dependency trees are not part of the public portable package.

The plug-in searches for `Heads/` relative to the portable layout (with a documented test/development override). If you separate the binaries from the resource folder, you can break head discovery. The release folder kept together is the reference installation.

## Reporting a problem

A useful report tells me:

- host and version;
- VST3 or CLAP;
- operating system;
- sample rate and block size when relevant;
- Renderer, Head and Tone;
- position values;
- what happened immediately before the problem;
- whether the editor was open;
- whether playback/rendering was running.

For removal/shutdown problems, say whether you removed the plug-in, deleted the whole track, closed the project, or exited the host. Those are different lifecycle paths and the distinction matters.
