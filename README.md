# SpaceTrace — Put a sound somewhere

Move a voice around the listener, lift an effect above them, or turn the angle of a stereo field recording within a soundscape. SpaceTrace is a binaural spatializer for headphone listening, available as a Windows x64 VST3 and CLAP plug-in.

It uses measurements of how sound reaches the ears from different directions to give each placement its timing, level, and tonal cues. Five heads offer different sets of those cues. Their prepared EQ and level calibration make it easier to compare the placement and movement you hear.

[**Download SpaceTrace 1.0.1**](https://github.com/dgl1984/spacetrace/releases/latest) · [**Read the manual**](Docs/MANUAL.md)

## Install

1. Download **SpaceTrace_1.0.1_Windows_x64.zip** from the release page and extract it.
2. Place the complete `SpaceTrace` folder in a plug-in location scanned by your DAW. Its plug-in settings or installation guide will show which locations it uses. Keep the accompanying folders together: `Heads/` contains the head models, corrections, and calibration settings.
3. Scan for new plug-ins, then insert **SpaceTrace** on an audio track. Choose VST3 or CLAP according to your DAW's support; both provide the same controls and sound.

When updating, replace the accompanying folders along with the plug-ins. The manual explains [installation](Docs/MANUAL.md#install-spacetrace) and [keeping the head data used by an earlier project](Docs/MANUAL.md#updates-and-project-recall).

## Try a moving sound

Put on headphones and play a voice, percussion loop, or another familiar recording. Start with **Modern HRTF**, **Mono Point Source**, **Dataset Corrected**, and **Distance 1 m**.

Move **Azimuth** from 0° toward 90° to take the sound from the front toward your left. Continue through 180° behind you and 270° to your right. To begin by moving right, start at 360° and move downward. Try **Elevation** for height, then **Distance** for changes in level and high-frequency detail.

For a stereo recording, choose **Stereo Pair**. It places the input channels as two linked sources that rotate together. Try turning a crowd recording to one side of a scene, or moving rain around a stationary voice. **Width Offset** changes the placement of the two sources toward the front or rear of the pair. The ambience already in the recording moves with it.

Mono Point Source combines the input channels inside the plug-in. Both modes produce a two-channel binaural output.

The [manual's listening exercises](Docs/MANUAL.md#hear-your-first-moving-sound) explain the controls alongside things to try and listen for. It also covers [exact value entry and automation](Docs/MANUAL.md#set-precise-values-and-automate).

## Compare the five heads

1. IRCAM LISTEN 1050
2. MIT KEMAR Normal Pinna
3. SADIE II D1 / Neumann KU100
4. TH Köln / Bernschütz FULL2DEG KU100
5. FABIAN HATO 0

Switch heads while repeating the same movement. Listen for how clearly you can follow the sound, distinguish front from rear, and hear changes in height. The [head comparison](Docs/MANUAL.md#find-a-head-that-works-for-you) describes their differences and links to published applications of the source datasets.

Each head includes correction EQ for Mono Point Source and separate corrections for the two Stereo Pair sources. The manual follows [how those filters and levels were refined](Docs/MANUAL.md#explore-the-eq-and-level-refinements), with links to the data and instructions for plotting the current responses.

SpaceTrace also includes **Papa Pan**, a recreation of the horizontal panning approach from the game-audio engine behind Papa Sangre and Audio Defence. Try its 24 stepped directions alongside Modern HRTF's interpolated movement. [Explore Papa Pan](Docs/MANUAL.md#try-papa-pans-historical-movement).

## Prepare your own head or correction

Load a correction WAV to shape the tone, or prepare a custom head from compatible SOFA spatial-measurement data. The [custom-head guide](Docs/CUSTOM_HEADS.md) and [script reference](scripts/README.md) cover conversion, measurement, correction, and packaging.

[Tone Trace](https://github.com/dgl1984/ToneTrace) is the separate frequency-response comparison and correction-design tool used to prepare the supplied heads.

## Licence

SpaceTrace's first-party material uses Apache License 2.0 with the Commons Clause License Condition v1.0; see [LICENSE.txt](LICENSE.txt). The bundled datasets and dependencies carry their own terms in the [third-party notices](Licenses/THIRD_PARTY_NOTICES.txt). Each head also includes its source, processing history, and licence records.
