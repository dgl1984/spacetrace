# SpaceTrace Build and Release Notes

SpaceTrace's VST3 and CLAP must be built from the same `SpaceTrace` JUCE target. Format-specific DSP/editor forks are not permitted.

## Windows prerequisites

- CMake
- Visual Studio 2022 Build Tools with Desktop development with C++
- Windows SDK
- Python for validation/head preparation
- Internet access only when pinned JUCE or CLAP wrapper dependencies are not already present

Run:

```text
BUILD_WINDOWS.bat
```

The build fetches JUCE 8.0.12 and the pinned `clap-juce-extensions` dependency only when necessary. Both are source/build dependencies; neither becomes a separate SpaceTrace implementation.

The build must:

1. prepare/validate any generated head resources;
2. run source-contract and head-package checks;
3. compile the shared processor/core integration tests;
4. run all registered tests;
5. build `SpaceTrace.vst3`;
6. build `SpaceTrace.clap` from the same JUCE target;
7. stage both plug-ins at the root of the portable `SpaceTrace/` folder;
8. stage only the explicit public whitelist (`Heads/`, user/reproducibility `Docs/`, `Licenses/`, user custom-head `scripts/`, README and first-party LICENSE);
9. validate that no internal audit/handoff/build/source artifacts leaked into the portable tree;
10. install nothing automatically.

For the developer visual validation pass, run separately:

```text
BUILD_VISUAL_SNAPSHOTS.bat
```

That target must instantiate and render the real SpaceTrace editor, reject invalid/uniform/black images, and write its PNG manifest/report outside the portable release. Its 125/150/175/200 percent outputs are JUCE component-render scale checks, not simulated Windows host DPI; validate real host DPI separately.

## CLAP dependency pins

`FETCH_CLAP_WINDOWS.ps1` restores exact revisions of:

- free-audio/clap-juce-extensions
- free-audio/clap
- free-audio/clap-helpers

The revisions are written into `deps/clap-juce-extensions/SPACETRACE_PINNED_REVISIONS.txt` after fetching. Do not silently replace them with whatever happens to be on `main` during a release build.

## Release gates

Before publishing a release, test at minimum:

- VST3 and CLAP instantiate in REAPER;
- both expose the same controls and state;
- NVDA/Narrator traversal and exact entry;
- Home/End on sliders and combo boxes, and Page Up/Page Down on Azimuth/Elevation;
- save/restore independently in both formats;
- five heads, Raw/Corrected/Custom Correction IR;
- Input Mode default Mono Point Source and Modern-only Stereo Pair mapping/state;
- eight host parameters, including Width Offset range/default/state and schema-4 migration to zero;
- Width Offset -45/0/+45 geometry, shared DSP/display positions, and no effect in Mono or Papa Pan;
- Head Model first in normal Tab/Shift+Tab navigation;
- Air Loss On/Off, including exact 1.00 m spectral neutrality;
- Mono Point Source ↔ Stereo Pair switching without stale FIR history;
- Papa Pan remains mono-only even when Stereo Pair is stored;
- Modern/Papa switching;
- developer visual snapshot harness plus real Windows DPI/host visual checks;
- missing-head behavior;
- multi-instance startup;
- offline rendering;
- remove plug-in during playback, editor open and closed;
- delete whole track during playback, editor open and closed;
- close project during playback;
- repeated create/destroy cycles;
- correction/head calibration tests;
- Python custom-head happy and failure paths;
- portable layout contains no build caches/debug artifacts.

## Incremental builds and dependency cache

The Windows build reuses `build-windows/`, including compiled JUCE objects. It does not delete this directory before configuring. CLAP archives remain in `deps/archives/` and are checked against pinned SHA-256 hashes. The fetch script compares the installed dependency files with the verified archives and stops if local content differs, preserving local edits. It does not trust the revision record alone. Requesting CLAP without its dependency is a CMake error.

## Licensing release gate

The repository carries SpaceTrace's first-party licence and separate third-party notices. For the JUCE dependency's licensing requirements, see [the JUCE notice](../Licenses/JUCE_NOTICE.txt).

## Publication checks

Run `python scripts/check_public_source.py` before pushing source, and run
`python tests/test_release_staging.py` before publishing a Windows package.
After changing correction IRs or retained Tone Trace models, run
`python scripts/refresh_correction_plots.py` and commit the regenerated images,
descriptions, and plot index. `python scripts/refresh_correction_plots.py --check`
checks their source and output hashes without requiring plotting dependencies.
Review new documentation for private discussion or development notes; automated
checks catch known filenames and handoff headings but cannot replace that review.

Release assets consist of the Windows package and its `SHA256SUMS.txt` file.
Source is available from the repository; do not upload a second source ZIP.
The release tag must identify the source and head assets used for that release.
