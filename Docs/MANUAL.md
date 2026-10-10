# SpaceTrace 1.0.1 — Put a sound somewhere

SpaceTrace lets you place sounds around you through headphones. Move a voice to one side, lift a sound above you, or listen to a stereo recording as if it were captured from a different angle.

Each of the five heads is a set of measurements describing how sound reaches the ears from different directions. Their corrections bring tone and level closer together, making it easier to compare where a sound seems to sit and how it moves.

## Contents

- [Install SpaceTrace](#install-spacetrace) — download, extract, and load the plug-in in your DAW.
- [Hear your first moving sound](#hear-your-first-moving-sound) — start here if binaural audio is new to you.
- [Find a head that works for you](#find-a-head-that-works-for-you) — compare the five datasets.
- [Change the angle of a stereo recording](#change-the-angle-of-a-stereo-recording) — explore Stereo Pair and Width Offset.
- [Shape position and distance](#shape-position-and-distance) — understand the movement controls.
- [Choose your tonal correction](#choose-your-tonal-correction) — use Raw, Dataset Corrected, or your own filter.
- [Set precise values and automate](#set-precise-values-and-automate) — exact entry, shortcuts, and host automation.
- [Explore the EQ and level refinements](#explore-the-eq-and-level-refinements) — follow the measurements and filter preparation.
- [Try Papa Pan's historical movement](#try-papa-pans-historical-movement) — explore a second approach to horizontal placement.
- [Updates and project recall](#updates-and-project-recall) — load new head data or preserve an earlier version for a mix.
- [Troubleshooting](#troubleshooting) — find an explanation or fix for a specific problem.
- [Technical reference and sources](#technical-reference-and-sources) — custom heads, engineering, source data, and licences.

## Install SpaceTrace

The Windows release includes VST3 and CLAP versions of the plug-in. Both use the same processor and head data, so choose the format your DAW supports.

1. Download **SpaceTrace_1.0.1_Windows_x64.zip** from the [SpaceTrace release page](https://github.com/dgl1984/spacetrace/releases/latest).
2. Extract the ZIP. Inside is a `SpaceTrace` folder containing the plug-ins and their accompanying files.
3. Place the complete folder in a plug-in location scanned by your DAW. Its plug-in settings or installation guide will show which locations it uses. Keep `Heads/` with the plug-ins: it contains the five head models, correction filters, and calibration settings.
4. Scan for new plug-ins in your DAW, then insert **SpaceTrace** on an audio track.

The extracted folder contains:

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

`Docs/` contains this manual and the preparation guides. The tools in `scripts/` are for creating and analysing head data; see [custom-head preparation](#make-or-refine-a-custom-head) to use them.

## Hear your first moving sound

Put on headphones and play something familiar: a spoken voice, a short percussion loop, or a sound with a clear attack. Familiar material makes it easier to notice what changes as you move it.

With SpaceTrace open on that track, start with these settings:

| Control | Starting value | What it gives you |
| --- | --- | --- |
| Renderer | Modern HRTF | Movement around, above, and below you |
| Head | IRCAM LISTEN 1050 | A starting point for comparison |
| Input Mode | Mono Point Source | One sound to place |
| Tone | Dataset Corrected | The head's prepared tonal correction |
| Azimuth | 0° | Straight ahead |
| Elevation | 0° | Ear level |
| Distance | 1.00 m | No distance-related gain or high-frequency loss |
| Air Loss | On | Subtle high-frequency loss when you move beyond 1 m |
| Output | 0.0 dB | No additional output adjustment |

Move **Azimuth** slowly from 0° toward 90°. The sound moves from in front of you toward your left. Continue to 180° for behind you, then 270° for your right, and 360° to return to the front. Listen for the position as well as changes in tone along the way.

For a movement that starts in front and goes right, start at **360° and move downward**. Both 0° and 360° mean straight ahead; the two ends of the control let you choose how to begin that movement.

Return to the front and try **Elevation**. Positive values place the sound above ear level; negative values place it below. Then return Elevation to zero and try **Distance**. Exploring one control at a time makes it easier to recognise each cue.

### What makes this binaural?

A sound reaches your two ears with differences in timing, level, and frequency response. Your head and outer ears help shape those differences. SpaceTrace uses measured responses to recreate a version of those cues for headphone listening.

An **HRTF**, or head-related transfer function, describes how a sound from a particular direction is filtered on its way to an ear. An **HRIR** is that response represented as an impulse response: a short sequence of samples that a renderer can apply to audio.

Each measured head gives you a different set of cues to try. Compare the same sound at the same position, and listen for the head that makes its location easiest to recognise.

Try the [head comparison below](#find-a-head-that-works-for-you), or explore [how the EQ refinements help that comparison](#explore-the-eq-and-level-refinements).

## Find a head that works for you

Keep a sound playing and switch **Head**. Listen for where the sound seems to sit, how clearly you can follow it, and whether height or front/back placement becomes easier to hear.

Try the same short sequence with each head: front, left, rear, right, then a slow rise in front of you. Keep Distance at 1 m and Tone on Dataset Corrected. That gives the comparison a consistent starting point.

Some datasets use artificial heads with microphones at the ears. The Neumann KU100 is one such recording head; two of the choices below use separate measurements of it.

| Menu order | Head | What distinguishes it |
| --- | --- | --- |
| 1 | IRCAM LISTEN 1050 | Measurements of subject 1050 at 187 directions, prepared as compact filters with separate timing information for each ear. This is SpaceTrace's default head. |
| 2 | MIT KEMAR Normal Pinna | Measurements of an artificial head and ears at 710 positions. The dataset comes from early 3D-audio research and offers a different ear shape to compare with the KU100 models. |
| 3 | SADIE II D1 / Neumann KU100 | From a project focused on immersive media; SADIE KU100 measurements have been used for binaural preview and monitoring of 360°/VR content. |
| 4 | TH Köln / Bernschütz FULL2DEG KU100 | A dense set of 16,020 measured directions around the head. Try small movements as well as full circles when comparing it with the other datasets. |
| 5 | FABIAN HATO 0 | Measurements of a head-and-torso simulator with the head facing straight ahead relative to the torso. This gives you another combination of ear, head, and upper-body geometry to audition. |

The number of measured positions tells you how densely a dataset samples directions around the head. Your listening comparison tells you how well those cues work for you. Even the two KU100 datasets have different measurement methods and corrections, so audition them separately.

You may prefer one head for a voice moving through a scene and another for changing the angle of stereo elements within a soundscape. Compare them on recordings of rain, traffic, or machinery, and listen for how the direction and tonal detail contribute to the environment or texture you are building.

For the measurements and preparation behind each head, follow the [data reference](#where-to-find-the-data-and-measurements). For examples of how these datasets have been used, see [published applications](#published-applications-of-the-five-datasets).

## Change the angle of a stereo recording

Choose **Input Mode: Stereo Pair** with Modern HRTF to place the left and right channels as two linked sources. Each channel reaches both ears through the selected head's response for its position.

Start with Azimuth 0°, Elevation 0°, Distance 1 m, and Width Offset 0°. Play a familiar stereo recording. Then rotate **Azimuth** slowly: the pair moves together around you, changing the angle from which you hear the recording.

For example, put a stereo crowd recording on its own track and use Azimuth to bring it around to one side of a soundscape. Or try a slow rotation of rain or machinery beneath a stationary voice. Each track's placement contributes to the scene while the ambience already in the recording moves with it.

### Choose one source or two

**Mono Point Source** combines the input channels into one signal, then places it at the selected position. Use it when you want a voice, instrument, or effect to occupy one location.

**Stereo Pair** gives each input channel its own position. Use it to turn the stereo image of a field recording, a layered effect, or a musical passage while keeping its two source channels.

Choose between these approaches with **Input Mode** inside SpaceTrace. Mono Point Source performs the summing inside the plug-in; both modes produce a two-channel binaural output for your headphones.

### Rotate the pair, then change its placement

At **Width Offset 0°**, input Left is 90° to the left of the selected centre and input Right is 90° to its right. At centre Azimuth 0°, that means positions of 90° and 270°.

**Width Offset** moves those positions symmetrically. Try a negative value to bring both sources toward the front of the pair, or a positive value to move them toward its rear. The range is −45° to +45°.

| Centre Azimuth | Width Offset | Left source | Right source |
| --- | --- | --- | --- |
| 0° | 0° | 90° — left | 270° — right |
| 0° | −45° | 45° — front-left | 315° — front-right |
| 0° | +45° | 135° — rear-left | 225° — rear-right |
| 90° | 0° | 180° — rear | 0° — front |

Both sources share Elevation and Distance. For example, set Width Offset to −45° to place them front-left and front-right, then raise Elevation to lift the pair together.

The position display follows your settings, showing one marker for Mono Point Source and two L/R markers for Stereo Pair.

### Compare the Stereo Pair corrections

For a head-to-head comparison, use **Azimuth 0°, Elevation 0°, Width Offset 0°, Distance 1 m**. These are the positions used to prepare the five heads' Stereo Pair correction filters and match their reference level.

Then move the pair to suit the recording. The correction filters stay fixed while the head's directional response follows the new positions, so movement can also change the tone you hear.

See [how the Stereo Pair corrections were refined](#how-the-stereo-pair-corrections-were-refined) for the combined filters, tail fade, and level calibration.

## Shape position and distance

### Azimuth: travel around the listener

Azimuth runs from **0° to 360°**. Front is 0°/360°, left is 90°, rear is 180°, and right is 270°.

For a slow full circle, automate from 0° to 360°. Reverse that movement by travelling from 360° to 0°. In Stereo Pair, Azimuth controls the centre around which the two sources are arranged.

The control steps in 1° increments. For exact positions and larger steps, see [value entry and shortcuts](#set-precise-values-and-automate).

### Elevation: move above or below ear level

Elevation runs from **−90° directly below to +90° directly above**, with 0° at ear level. Try moving a sound upward in front of you, then repeat behind you. Compare heads if one part of that movement is difficult to follow.

Elevation uses 1° increments. In Stereo Pair it sets the requested elevation of both sources.

### Distance: move a sound closer or farther away

Distance runs from **0.25 m to 20.00 m**, with **1.00 m as the neutral reference**. Move a sound from 1 m to 2 m and listen first to the level change. With Air Loss enabled, moving farther also adds a subtle loss of high-frequency energy.

Beyond 1 m, doubling the distance reduces the level by about 6 dB. Inside 1 m, a softer gain curve brings a sound closer, reaching about +6 dB at 0.25 m.

For an approaching or receding sound, automate Distance to change its level and high-frequency detail. Any recorded ambience is processed with the source; room reverb can be added with a separate effect.

**Air Loss** switches the synthetic high-frequency distance effect on or off. The distance-related level change continues in either setting. At 1 m, Air Loss On and Off produce the same result, making that a useful reference for comparisons.

The [distance-model details](#how-distance-is-calculated) give the gain curves and high-frequency attenuation used by these controls.

### Output and Bypass: set the result in context

**Output** gives you a final trim from **−24.0 dB to +12.0 dB**. Set the position, then use Output to place the sound at the level your mix needs.

**Bypass** lets you hear the input without SpaceTrace's processing. Toggle it to compare the original sound with your placement.

## Choose your tonal correction

Use **Tone** to choose how the selected head's overall coloration is treated.

### Dataset Corrected: begin with the prepared response

Dataset Corrected is the starting point for normal use and for comparisons between the supplied heads. Mono Point Source uses the head's global tonal correction. Stereo Pair uses separate corrections for its left and right source channels.

The correction adjusts the overall frequency balance measured at the reference position. As the sound moves, the head's response changes with direction while the correction stays fixed.

### Raw: hear the head without tonal correction

Raw disables the correction filter. Try it at a fixed position to hear how the correction changes the response, or use it when measuring a new head.

The head's fixed level and balance gains stay active when you select Raw, so you can compare the correction within the same calibration.

### Custom Correction IR: use your own filter

An **IR**, or impulse response, stores a filter as a short audio file. Here, it describes the tonal correction you want to apply.

Choose **Load Custom IR** and select a correction WAV. Loading it selects Custom Correction IR automatically and applies your filter in place of Dataset Corrected.

Use a **mono WAV, or a stereo WAV with identical channels, no longer than 250 ms**. Applying the same filter to both ears changes overall tone while preserving their relative level and timing cues.

SpaceTrace saves your custom correction in the project.

See [correction and calibration](#explore-the-eq-and-level-refinements) for the filter and gain stages, or [custom-head preparation](#make-or-refine-a-custom-head) to create your own.

## Set precise values and automate

To repeat a placement exactly, focus a slider and press Enter or F2. Type the value, then press Enter to apply it or Escape to cancel. This is useful when comparing heads at the same position or setting up the start of a movement.

These shortcuts help you move between controls and make fine adjustments. Tab and Shift+Tab follow the control order, skipping controls that do not apply to the current mode.

| Control or action | Keys |
| --- | --- |
| Move between controls | Tab / Shift+Tab |
| Adjust a slider | Arrow keys |
| Jump to a slider's minimum or maximum | Home / End |
| Change Azimuth or Elevation by 10° | Page Up / Page Down |
| Enter an exact slider value | Enter or F2 |
| Apply an exact value | Enter |
| Cancel exact entry | Escape |
| Choose a neighbouring combo-box item | Arrow keys |
| Choose the first or last combo-box item | Home / End |
| Open a combo-box menu | Enter |

After exact entry, focus returns to the slider.

### Automate the movement

Azimuth, Elevation, Distance, Input Mode, Width Offset, Air Loss, Output, and Bypass are host parameters. You can also reach these through the host's generic parameter view.

Choose Head, Renderer, and Tone before writing automation. These selections are saved with the project; movement and level are controlled by the host parameters above.

For a first automation pass, try a slow Azimuth movement with a fixed head and correction. Add Elevation or Distance once you can hear what each movement contributes.

## Explore the EQ and level refinements

The refinements bring the heads closer together in tone and reference level so you can hear their spatial differences more clearly. SpaceTrace treats tonal correction, overall level, and balance as separate adjustments. The filters shape the tone; fixed gains set the measured level and balance.

### Global correction for a point source

The global correction begins with a front-centre Raw measurement of the head being prepared. **Tone Trace**, a separate tool for comparing frequency responses and creating correction filters, compares pink noise processed through the head with the unprocessed reference. The resulting correction is used across positions in Mono Point Source.

Pink noise has equal power per octave. Using pink weighting over **80 Hz–16 kHz** gives the calibration a consistent reference across the frequency range. Music and speech distribute their energy differently, so their perceived level can vary around that reference.

A separate Mono Point Source trim sets the reference level at Azimuth 0°, Elevation 0°, Distance 1 m, Tone Dataset Corrected, and Output 0 dB. At those settings, the average power of the two output channels matches the mono source's power. Stereo input is first averaged as `(L + R) / 2`; identical input channels keep their amplitude through that step.

### How the Stereo Pair corrections were refined

Each of the five heads has a left-source correction and a right-source correction for the centred 90°/270° arrangement. Each source's filter is applied equally to its two ear outputs before the sources are summed, preserving the level and timing relationship between those ears.

The latest refinement adds a second correction stage to the existing pair. One extra IR per head is convolved with each of that head's existing Stereo Pair corrections. Convolution combines the successive filters into a single response for each source channel, so the plug-in can apply both stages together.

All components in this composition are **48 kHz and 8,640 samples**. Full linear convolution produces **17,279 samples**. The final file retains the original **8,640-sample window, or 180 ms**, with a **40 ms half-cosine fade over its final 1,920 samples**. The fade reaches zero at the end of the retained window.

Keeping the original length lets each refined correction fit the same runtime window. The fade brings the retained tail smoothly to zero. Both operations affect the filter response; use the final WAV when measuring or plotting the combined correction.

The composed WAVs retain their amplitude. Level matching uses fixed gains stored in each head's manifest, calculated from the final responses together with the head-level and ear-balance gains. Equal-and-opposite source gains balance the two source contributions. A common Stereo Pair trim then matches their summed power to the reference: two independent pink-noise input channels, each with unit power.

The measurement uses the centred positions over 80 Hz–16 kHz. The gains remain fixed during playback. When the input channels share material, their rendered outputs can reinforce or cancel at different frequencies, so a recording's level and peaks can differ from the independent-channel reference.

### Follow the gain path

| File or manifest field | Purpose |
| --- | --- |
| `correction.wav` | Global tonal correction used for Mono Point Source |
| `stereo_pair_left_correction.wav` / `stereo_pair_right_correction.wav` | Tonal correction for the two Stereo Pair source channels |
| `levelTrimDb` | Common head-level gain |
| `leftGainDb` / `rightGainDb` | Fixed output-ear balance calibration |
| `monoTrimDb` | Additional Mono Point Source reference-level gain |
| `stereoPairLeftGainDb` / `stereoPairRightGainDb` | Fixed Stereo Pair source-channel balance |
| `stereoPairTrimDb` | Common Stereo Pair reference-level gain |

The common gain in dB is `levelTrimDb + monoTrimDb` for Mono Point Source, or `levelTrimDb + stereoPairTrimDb` for Stereo Pair. Reference-level measurements include this combined gain.

The source-channel and ear-channel gains do different work: source gains balance the two virtual sources, while ear gains balance the rendered left and right outputs. The mode trims also remain active in Raw and Custom Correction, preserving the relative changes from selecting a different correction.

### Where to find the data and measurements

Each head has two records: **manifest.json** lists the files and runtime calibration settings; **provenance.json** records the source measurements and processing steps. The `stereoPairPostCorrection` entry describes the added correction stage, and `currentStereoPairFiles` identifies the final pair. Earlier entries document the preceding stages.

| Head | Current files and calibration | Source and processing record |
| --- | --- | --- |
| IRCAM | [Manifest](../Heads/IRCAM_1050/manifest.json) | [Provenance](../Heads/IRCAM_1050/provenance.json) |
| MIT KEMAR | [Manifest](../Heads/MIT_KEMAR_Normal/manifest.json) | [Provenance](../Heads/MIT_KEMAR_Normal/provenance.json) |
| SADIE D1 | [Manifest](../Heads/KU100_SADIE_D1/manifest.json) | [Provenance](../Heads/KU100_SADIE_D1/provenance.json) |
| FULL2DEG | [Manifest](../Heads/KU100_FULL2DEG/manifest.json) | [Provenance](../Heads/KU100_FULL2DEG/provenance.json) |
| FABIAN | [Manifest](../Heads/FABIAN_HATO0/manifest.json) | [Provenance](../Heads/FABIAN_HATO0/provenance.json) |

Each record includes SHA-256 hashes: file fingerprints that identify the exact data used. To plot a current correction, open a terminal in the SpaceTrace package folder and run:

```text
python scripts/plot_correction.py Heads/FABIAN_HATO0/stereo_pair_left_correction.wav fabian-stereo-left.png
```

The tool produces a response image and a matching text description reporting boost, cut, and broad frequency-band averages. Use the graph to explore the curve and the text to compare its measured values. A correction plot describes the filter itself; the head response and separate runtime gains also contribute to the sound.

The bundled [Stereo Pair plots](Correction_Plots/StereoPair/) show the earlier correction revision. The command above plots the current WAV and records its source hash in the text description.

The retained [Tone Trace models](Correction_Models/) show earlier stages of the correction design. The final WAV includes the combined filters and tail fade.

## Try Papa Pan's historical movement

**Papa Pan** is SpaceTrace's recreation of the horizontal panning approach used by the Papa Engine, the binaural game-audio engine behind Papa Sangre and Audio Defence. It uses the public IRCAM LISTEN 1050 measurements.

Select **Renderer: Papa Pan — historical**, play a repeating sound, and move Azimuth slowly. Papa Pan places the sound in one of 24 directions around you, spaced 15° apart. Movement advances between these positions in steps, with an additional gain contour as the sound crosses the front half of the circle.

Switch to Modern HRTF to compare its interpolated movement, which blends measured responses as the position changes. Papa Pan works with one source on the horizontal plane; Modern HRTF also provides elevation, head selection, and Stereo Pair.

The historical implementation is documented in the source repository's [Papa Pan provenance](https://github.com/dgl1984/spacetrace/blob/main/PAPA_PAN_PROVENANCE.md).

## Updates and project recall

To update, close the host, replace the package files, and reopen it to load the new data.

### What a project remembers

A project stores your control settings, selected head, and any custom correction you loaded. The supplied head models and corrections are read from the installed `Heads/` folder.

For exact recall of an older sound, keep the corresponding head package with your project archive. A new correction or calibration takes effect when that package is updated, including in existing projects. The [package-identity details](#how-projects-identify-head-data) explain what the saved hashes track.

## Troubleshooting

| What you notice | What to try and why |
| --- | --- |
| Front and rear are hard to distinguish | Compare heads at fixed positions, then use a slow movement. The useful cues vary with the head and source material. |
| Height is difficult to hear | Compare several heads with the same sound. Try a vertical movement in front and behind instead of judging only one static position. |
| A stereo recording becomes quieter or loses elements | Mono Point Source averages the input channels as `(L + R) / 2`. Different channels can become quieter when combined, and opposite-polarity content can cancel. Select Stereo Pair to keep two input sources. |
| The pair changes tone when you move Width Offset | Return to Width Offset 0°, centre Azimuth 0°, and Elevation 0° to hear the correction reference. Placement away from those angles uses the same filters. |
| Air Loss seems to do nothing | At 1 m it is neutral. Try a farther distance and compare On/Off while keeping that distance fixed. |
| A head is unexpectedly quiet | Compare at Distance 1 m and Output 0 dB first. If the difference remains after an update, reinstall that head's complete folder and reopen the host so its corrections and gain settings load together. |
| Stereo Pair peaks above the expected level | Shared content in the input channels can reinforce when the rendered sources combine. Lower Output to give the result more headroom; SpaceTrace applies no limiting. |
| A control is unavailable | Check Renderer and Input Mode. Elevation belongs to Modern HRTF; Width Offset belongs to Modern HRTF Stereo Pair. |
| The plug-in cannot find a head or reports a hash mismatch | Restore the complete head folder from the release, including its WAVs and manifest. This supplies a matching set of files. |
| A custom IR will not load | Use a readable mono or identical-stereo WAV of 250 ms or less, and read the status message for the specific error. |
| A typed value is rejected | Enter a number within the range shown in the error message. The previous setting is kept until a valid value is entered. |
| A custom head plays in Raw when Dataset Corrected is selected | Read the status message and confirm that the head package includes its correction file. A package with no correction uses Raw. |

To [report a problem](https://github.com/dgl1984/spacetrace/issues), include the host and version, VST3 or CLAP, operating system, sample rate, and block size. Give the Renderer, Head, Input Mode, Tone, position, Width Offset, and Output settings, along with a short sequence that reproduces it.

For a shutdown problem, say whether you removed the plug-in, deleted its track, closed the project, or exited the host, and whether playback and the editor were active. Those details identify different processing paths.

## Technical reference and sources

### Published applications of the five datasets

These sources describe the original datasets and their use in mixing, rendering, and research. SpaceTrace's preparation and corrections are documented in the [EQ section](#explore-the-eq-and-level-refinements).

**IRCAM LISTEN 1050 — binaural mixing and scene placement.** IRCAM's Panoramix guide shows how the LISTEN collection can be used on binaural mixing buses to monitor spatial scenes over headphones. SpaceTrace selects subject 1050 from that collection. [IRCAM Panoramix quick-start guide, HRTF selection](https://forum.ircam.fr/media/uploads/forumnet-legacy/2016/12/Panoramix-QuickStart2.pdf).

**MIT KEMAR Normal Pinna — 3D audio experiments and research.** Gardner and Martin's MIT archive includes the measurement method, processing resources, and a demonstration 3D audio spatializer. Follow it to see how the measured ear responses were used to build a spatial-audio tool. SpaceTrace uses the normal-pinna dataset from this collection. [MIT Media Lab: KEMAR measurements](https://sound.media.mit.edu/resources/KEMAR.html).

**SADIE II D1 / KU100 — immersive media and binaural monitoring.** The University of York documents how SADIE KU100 measurements were used in Google's YouTube 360/VR pipeline. Its resources cover binaural preview and monitoring spatial content in a DAW. [University of York: Google/SADIE binaural filters](https://www.york.ac.uk/sadie-project/GoogleVRSADIE.html).

**FULL2DEG KU100 — music production and sound-field rendering.** Bernschütz's paper describes the full-sphere measurement compilation for audio production and spherical-acoustics work, with interfaces to sound-field analysis and binaural rendering tools. Its 2° grid contains 16,020 positions. The paper is useful if you want to understand dense spatial sampling and the dataset's own low-frequency and phase preparation before SpaceTrace's corrections. [Bernschütz: A Spherical Far Field HRIR/HRTF Compilation of the Neumann KU100](https://audiogroup.web.th-koeln.de/PUBLIKATIONEN/Bernschuetz_DAGA2013.pdf).

**FABIAN HATO 0 — spatial-audio simulation and measurement research.** TU Berlin's database combines HRTFs, headphone responses, and 3D geometry for the FABIAN head-and-torso simulator. It connects acoustic measurements with physical geometry for work such as auralization: making a simulated acoustic scene audible. SpaceTrace uses its neutral head-above-torso orientation, with the head facing forward. [TU Berlin: FABIAN HRTF database and documentation](https://depositonce.tu-berlin.de/items/3b423df7-a764-4ce1-9065-4e6034bba759).

### Make or refine a custom head

The [custom-head guide](CUSTOM_HEADS.md) covers conversion and initial correction preparation. The [script reference](../scripts/README.md) explains the tools, and [Tone Trace](https://github.com/dgl1984/ToneTrace) provides the response-comparison and correction-design workflow used for the supplied heads.

Begin with a compatible **SOFA file**, a standard container for spatial-acoustic measurements. Keep that original and its licence as your source. Use the Python preparation tools to convert and validate it, then listen in Raw before designing a correction.

For an initial global correction, compare the same pink-noise signal before and after the Raw head at front-centre. For an initial Stereo Pair correction, measure the two source contributions at the centred 90°/270° reference. Finish tone, source/ear balance, and common level calibration as distinct steps.

A **residual correction** addresses what remains after an earlier correction has been applied. Convolve it with the preserved base response, then measure the final WAVs through the complete gain path. The [second-stage recipe above](#how-the-stereo-pair-corrections-were-refined) describes how the current Stereo Pair files were prepared.

A custom head can start with its Raw measurements and gain corrections. Add tonal correction once you have auditioned the spatial response. A head with a global correction can also use that filter in Stereo Pair until dedicated pair corrections are prepared.

### How distance is calculated

At and beyond 1 m, the level gain is `1 / distance`. Inside 1 m, it is `sqrt(1 / distance)`. The softer close-distance curve supplies a controlled level increase using the same directional head measurements.

With Air Loss enabled, a 5 kHz low/high split attenuates the high-frequency component by 0.15 dB per metre beyond 1 m, capped at 3 dB. At 1 m the gain is unity. These are synthetic distance cues; the stage adds neither room reflections nor measured near-field changes to the head response.

### How projects identify head data

Project state stores the selected head's stable identifier and a hash of its native `head.sthrtf` file. On reopening, SpaceTrace locates the head by that identifier and can report a change in the native file's hash.

Correction WAVs are checked against the hashes in the installed manifest. Those checks verify that the installed files belong together. The project-saved native head hash covers the head model; correction files and calibration values come from the installed package. Archiving that package preserves the complete data used for a mix.

### Understand the runtime preparation

SpaceTrace converts SOFA data into **`.sthrtf`**, the format its renderer reads. Conversion carries over the directional measurements it uses, expresses positions in SpaceTrace's coordinate system, and stores runtime samples as 32-bit floating-point values. The original SOFA remains the source for any later conversion.

IRCAM 1050 separates each ear's response into a compact 128-sample minimum-phase filter and an explicit delay. The filter carries its spectral shape; the delays retain the broadband timing difference between the ears. FABIAN provenance records modelled substitutions below 200 Hz and at elevations below −64°, made by the dataset authors.

At a different host sample rate, SpaceTrace resamples the head and corrections before audio processing. Ordinary measured impulse responses use windowed-sinc resampling. Separated minimum-phase responses and correction filters use minimum-phase reconstruction; explicit delays are scaled for the new rate. For a measurement of the complete output, record the plug-in at the host rate you intend to use.

Multiple instances can share the same prepared head data, reducing repeated preparation. Each instance keeps its own audio-processing history. File loading and preparation happen outside the audio callback.

The [source repository](https://github.com/dgl1984/spacetrace) contains the renderer, head loader, preparation scripts, and tests for readers who want to follow those details in code.

### Follow the source and licence records

Each head includes `LICENSE.txt` and `provenance.json`. Start with [third-party notices](../Licenses/THIRD_PARTY_NOTICES.txt) for dataset attribution and redistribution terms, and [the SpaceTrace licence](../LICENSE.txt) for first-party material. The bundled datasets and dependencies have their own terms; use their notices when redistributing or adapting them.

Keep these records with a head package so its source, processing history, and terms of use travel with it.
