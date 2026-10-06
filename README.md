# SpaceTrace 1.0.1 — Five-Head Spatializer, VST3 + CLAP

SpaceTrace is a binaural HRTF spatializer built to keep spatial audio practical whether you work with a mouse, keyboard, screen reader, host automation, or the host's generic parameter view.

SpaceTrace 1.0.1 ships five heads:

1. IRCAM LISTEN 1050
2. MIT KEMAR Normal Pinna
3. SADIE II D1 / Neumann KU100
4. TH Köln / Bernschütz FULL2DEG KU100
5. FABIAN HATO 0

Stereo Pair uses separate direct left/right correction IRs for all five heads. Head numbers follow the plug-in menu: 1 IRCAM, 2 MIT KEMAR, 3 SADIE, 4 FULL2DEG, 5 FABIAN. Current correction response plots and calibration metadata accompany the head packages.

## Start here

Mono Point Source has a fixed level calibration for each head: with identical left/right input (or a true mono input), the default front-center position is matched to the input's 80 Hz–16 kHz pink-weighted level. It preserves the spatial and tonal response. Stereo-to-mono averaging can still reduce differing or opposite-polarity material; there is no automatic gain control. Stereo Pair retains its separate calibration.

Read `Docs/MANUAL.md` for the practical workflow and the engineering details behind it. The shortest possible start is:

1. Insert SpaceTrace and leave **Renderer** on Modern HRTF and **Input Mode** on Mono Point Source.
2. Pick a **Head**.
3. Leave **Tone** on Dataset Corrected.
4. Move **Azimuth**, **Elevation**, and **Distance**. **Air Loss** defaults on and affects only the subtle synthetic HF absorption beyond 1 m.
5. Remember that 0° and 360° are both straight ahead: increasing from 0° starts left; to move right from front-center, begin at 360° and move downward.

## Accessibility

SpaceTrace follows one hard rule: **one parameter, one user-facing control**. The graphical position display is output-only; it does not duplicate Azimuth, Elevation, or Distance as another editable surface.

There is no private OneCore/MSAPI/NVDA speech layer. The normal controls expose normal accessibility information.

Keyboard additions include Home/End on sliders and combo boxes, 10° Page Up/Page Down steps on Azimuth/Elevation, and Enter/F2 exact entry on sliders. These commands exist because JUCE's stock controls do not supply all of them by default.

## VST3 and CLAP are the same implementation

Both formats are wrappers around the same JUCE processor/editor/state/core target. There is no separate CLAP DSP or UI implementation.

SpaceTrace exposes one true Bypass parameter to the host; the wrapper bypass points at that same parameter.

Modern HRTF also offers **Stereo Pair**: input Left and Right are rendered as a linked binaural pair around the selected center Azimuth, then summed through the normal shared Distance/Air Loss/Output stages. **Width Offset** (Stereo Pair only) ranges from -45° to +45° and bows the two sources symmetrically around the canonical 90°/270° reference geometry. Zero is the calibrated reference. Papa Pan remains mono-only.

## Portable release layout

The public Windows package is deliberately small:

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

Internal audits, development handoffs, source code, build scripts, dependency trees and RC engineering reports are not staged into this public folder.

## Custom heads

SOFA is the archival/source format. `.sthrtf` is SpaceTrace's prepared runtime format. The public Python tools in `scripts/` let you convert compatible SOFA files, validate a head, measure front-center balance, normalize a Tone Trace correction, prepare optional 90°/270° Stereo Pair correction/loudness calibration, generate correction plots/text descriptions, and package the result.

See `Docs/CUSTOM_HEADS.md` and `scripts/README.md`.

Tone Trace is optional for normal SpaceTrace use. If you are creating a custom dataset correction, the official project/releases are linked from the manual and custom-head guide.

## Licensing

`LICENSE.txt` covers first-party Lanes Audio SpaceTrace material under Apache License 2.0 plus the Commons Clause License Condition v1.0, matching the first-party licensing model used by Tone Trace.

The bundled HRTF datasets and third-party build/framework components retain their own terms. They are **not** blanket-licensed by SpaceTrace's first-party license. Start with `Licenses/THIRD_PARTY_NOTICES.txt`, and keep the `LICENSE.txt`/`provenance.json` that travels with each head package.
