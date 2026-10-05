# Third-party notices

SpaceTrace and the HRTF datasets it can load are separate works with separate terms. Keep the notices that belong to any head you redistribute.

## IRCAM LISTEN 1050

The default/reference head is derived from the official SOFA file `IRC_1050_R_44100.sofa`. The source license permits educational, research, and commercial use subject to its notice requirements and requests acknowledgment. The license text carried with the SpaceTrace head is in `Heads/IRCAM_1050/LICENSE.txt`; the source-tree copy is `LICENSE_IRCAM_LISTEN.txt`.

SpaceTrace does not use HRTF bytes extracted from Papa Sangre or Audio Defence binaries. Those projects are historical behaviour references for Papa Pan only.

## MIT KEMAR Normal Pinna

This head is derived from the MIT Media Lab KEMAR dataset. The source notice states that the data may be used freely provided Bill Gardner and Keith Martin are cited when it is used in research or commercial applications. See `Heads/MIT_KEMAR_Normal/LICENSE.txt` and `LICENSE_MIT_KEMAR.txt`.

## SADIE II D1 / Neumann KU100

This head is generated reproducibly from subject D1 of the SADIE II database, measured with the Neumann KU100 dummy head. SADIE II is distributed by the University of York under Apache License 2.0 with attribution/reference requirements described in `LICENSE_SADIE_II.txt`. The generated SpaceTrace package keeps the raw directional HRIR data separate from its fixed Tone Trace tonal correction.

## TH Köln / Bernschütz FULL2DEG KU100

This head is derived from Benjamin Bernschütz / TH Köln's `HRIR_FULL2DEG.sofa`, a dense Neumann KU100 dataset with 16,020 measurements. The source SOFA reports the license as **CC 3.0 BY-SA**. The source SHA-256, source URL, conversion method, and source metadata are preserved in `Heads/KU100_FULL2DEG/provenance.json`; the accompanying license notice is in `Heads/KU100_FULL2DEG/LICENSE.txt`.

## FABIAN HATO 0

This head is derived from the FABIAN HRTF database at HATO 0°. The source metadata reports **Creative Commons Attribution 4.0 International (CC BY 4.0)**. See `Heads/FABIAN_HATO0/LICENSE.txt`.

## JUCE

JUCE 8.0.12 is a build dependency. SpaceTrace's Windows build fetches the pinned version when it is not already present. JUCE retains its own licensing terms; consult the JUCE source distribution used for the build.

## CLAP build support

SpaceTrace CLAP builds use pinned revisions of the MIT-licensed `free-audio/clap-juce-extensions` wrapper and its `free-audio/clap` and `free-audio/clap-helpers` dependencies. These are wrapper/build dependencies only: VST3 and CLAP are built from the same SpaceTrace JUCE processor/editor/core implementation.

See:

- `LICENSE_CLAP_JUCE_EXTENSIONS.txt`
- `LICENSE_CLAP.txt`
- `LICENSE_CLAP_HELPERS.txt`

The exact revisions used by the build are pinned in `FETCH_CLAP_WINDOWS.ps1` and recorded into the fetched dependency tree.
