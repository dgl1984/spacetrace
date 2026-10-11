# SpaceTrace Build and Release Notes

SpaceTrace's VST3, CLAP and Audio Unit must be built from the same `SpaceTrace` JUCE target. Format-specific or platform-specific DSP/editor forks are not permitted.

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

## macOS prerequisites

- macOS 12 or later with Xcode (or its command-line tools)
- CMake 3.22 or later
- Python 3.12 (preferred; `h5py` may have no wheel for the newest Python). Set `SPACETRACE_PYTHON` to choose another interpreter.
- Internet access only when pinned JUCE or CLAP wrapper dependencies are not already present

Run:

```text
./BUILD_MACOS.sh
```

The script creates `.venv/` for the head-tool Python dependencies and restores the pinned dependencies with `scripts/fetch_deps.py`. That script uses the same CLAP revisions and archive hashes as `FETCH_CLAP_WINDOWS.ps1`, and the JUCE 8.0.12 source archive for tag commit `29396c22c93392d6738e021b83196283d6e4d850`. It then follows the Windows steps above, with these differences:

1. the build is universal (`arm64;x86_64`) with a macOS 12.0 deployment target;
2. it also builds `SpaceTrace.component` (Audio Unit) from the same target, which Logic Pro and GarageBand require;
3. the CLAP binary test loads the `.clap` bundle with `dlopen`;
4. it does not prepare the KU100 package; a missing or stale package stops the build with instructions;
5. staging copies `Heads/`, `Licenses/` and `LICENSE.txt` into `Contents/Resources` of every bundle instead of the release root, and validates each copy;
6. `scripts/package_macos.py` signs the staged bundles and builds `SpaceTrace_<version>_macOS_Universal.dmg` and `SHA256SUMS.txt` in `build-macos/release/`.

The disk image contains `Install SpaceTrace.pkg`, `Docs/`, `scripts/`, `Licenses/`, `README.md` and `LICENSE.txt`. The installer has one optional package per plug-in format, installed to `/Library/Audio/Plug-Ins/{Components,VST3,CLAP}`, and installs nothing else, so deleting a bundle uninstalls that format. Bundle relocation is disabled so upgrades never land in a build folder. The Audio Unit package restarts `AudioComponentRegistrar` so hosts see the new component.

On macOS the plug-in looks for heads in this order: `SPACETRACE_HEADS_DIR`; `~/Library/Application Support/LanesAudio/SpaceTrace/Heads`, which users create only to replace the supplied heads; the bundle's own `Contents/Resources/Heads`; then the shared search beside the plug-in used by development builds. Because the heads sit inside the signed bundle, the code signature covers them.

### Signing and notarization

Without configuration, bundles are signed ad hoc and the package is unsigned. That runs on Apple silicon, but Gatekeeper blocks the downloaded package until the user chooses Open Anyway in System Settings > Privacy & Security. Apple offers no free signing for open-source projects; a Developer ID requires Apple Developer Program membership. With one, set:

```text
SPACETRACE_CODESIGN_IDENTITY="Developer ID Application: <name> (<team>)"
SPACETRACE_INSTALLER_IDENTITY="Developer ID Installer: <name> (<team>)"
SPACETRACE_NOTARY_PROFILE=<profile created with xcrun notarytool store-credentials>
```

Bundles are then signed with the hardened runtime and a secure timestamp; the installer is signed, notarized and stapled; and the disk image is signed, notarized and stapled.

### macOS release gates

In addition to the shared gates below, test:

- `auval -v aufx SpTr LnAu` passes after installing;
- Audio Unit, VST3 and CLAP instantiate in Logic Pro, REAPER and Ableton Live, on Apple silicon and, where available, an Intel Mac;
- VoiceOver traversal, interaction and exact entry, including the Fn+arrow equivalents of Home/End and Page Up/Page Down;
- the installer's Customize choices, upgrade over an earlier install, deleting a bundle as uninstallation, and the per-user `Heads` replacement folder;
- Retina and non-Retina displays.

## Visual snapshots

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

Release assets consist of the Windows package, the macOS disk image, and a `SHA256SUMS.txt` file covering both.
Source is available from the repository; do not upload a second source ZIP.
The release tag must identify the source and head assets used for that release.
