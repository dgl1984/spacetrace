#!/bin/bash
# Build, test, stage and package SpaceTrace for macOS (universal AU, VST3 and CLAP).
# Signing is ad hoc unless Developer ID variables are set; see scripts/package_macos.py.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="$ROOT/build-macos"
LOG="$ROOT/BUILD_MACOS_LOG.txt"
JUCE="${JUCE_DIR:-$ROOT/deps/JUCE}"
CLAP_EXT="${CLAP_JUCE_EXTENSIONS_DIR:-$ROOT/deps/clap-juce-extensions}"
VENV="$ROOT/.venv"
JOBS="$(sysctl -n hw.ncpu)"

exec > >(tee "$LOG") 2>&1
trap 'echo "=== FAIL ==="; echo "Build log: $LOG"' ERR

fail() { echo "ERROR: $*"; exit 1; }

VERSION="$(sed -nE 's/^project\(SpaceTrace VERSION ([0-9.]+).*/\1/p' "$ROOT/CMakeLists.txt")"
[ -n "$VERSION" ] || fail "Could not read the SpaceTrace version from CMakeLists.txt."
echo "=== SpaceTrace $VERSION - macOS AU + VST3 + CLAP Validation ==="
echo "Source: $ROOT"

command -v cmake >/dev/null || fail "CMake is required and was not found on PATH."
command -v xcrun >/dev/null || fail "Xcode command-line tools are required: xcode-select --install"

# h5py publishes wheels for established Python releases, so prefer 3.12.
if [ ! -x "$VENV/bin/python" ]; then
    PYTHON="${SPACETRACE_PYTHON:-$(command -v python3.12 || command -v python3 || true)}"
    [ -n "$PYTHON" ] || fail "Python 3 is required and was not found on PATH."
    echo "=== Creating Python environment with $PYTHON ==="
    "$PYTHON" -m venv "$VENV"
fi
PY="$VENV/bin/python"
if ! "$PY" -c "import numpy, h5py, matplotlib" 2>/dev/null; then
    echo "=== Installing head-tool Python dependencies ==="
    "$PY" -m pip install --quiet -r "$ROOT/scripts/requirements.txt"
fi

if [ -n "${JUCE_DIR:-}" ] && [ ! -f "$JUCE/CMakeLists.txt" ]; then
    fail "JUCE_DIR does not contain CMakeLists.txt: $JUCE"
fi
if [ -n "${CLAP_JUCE_EXTENSIONS_DIR:-}" ] && [ ! -f "$CLAP_EXT/CMakeLists.txt" ]; then
    fail "CLAP_JUCE_EXTENSIONS_DIR does not contain CMakeLists.txt: $CLAP_EXT"
fi
if [ -z "${JUCE_DIR:-}" ] || [ -z "${CLAP_JUCE_EXTENSIONS_DIR:-}" ]; then
    echo "=== Verifying pinned JUCE 8.0.12 and CLAP dependencies ==="
    "$PY" "$ROOT/scripts/fetch_deps.py"
fi

echo "=== Checking existing SADIE II D1 / KU100 native package ==="
"$PY" "$ROOT/scripts/check_native_package.py" "$ROOT/resources/datasets/sadie2_d1_ku100_44100.sthrtf" \
    --name 'SADIE II D1 / Neumann KU100' \
    --measurement-count 8802 \
    --ir-length 256 \
    --require-compensation \
    --compensation-version 'tonetrace-raw-v1' ||
    fail "The KU100 package is missing or stale. Prepare it on Windows with scripts/Prepare-SADIE-D1.ps1, or with PowerShell 7 (pwsh)."

echo "=== Source contract ==="
"$PY" "$ROOT/tests/source_contract.py"

echo "=== External head package validation ==="
"$PY" "$ROOT/tests/validate_heads.py" --require-all

# Keep the CMake cache and compiled dependencies for incremental builds when present.
echo "Build directory: $BUILD (incremental if already present)"

echo "=== Configure universal Release build ==="
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release \
    -DSPACETRACE_BUILD_PLUGIN=ON \
    -DSPACETRACE_BUILD_TESTS=ON \
    -DSPACETRACE_BUILD_TOOLS=OFF \
    -DSPACETRACE_BUILD_CLAP=ON \
    "-DJUCE_DIR=$JUCE" \
    "-DCLAP_JUCE_EXTENSIONS_DIR=$CLAP_EXT" \
    "-DPython3_EXECUTABLE=$PY"

echo "=== Build tests and processor integration ==="
cmake --build "$BUILD" --parallel "$JOBS" --target \
    spacetrace_core_tests spacetrace_dataset_tests spacetrace_plugin_tests spacetrace_mono_level_tests spacetrace_clap_tests

echo "=== Run tests ==="
ctest --test-dir "$BUILD" --output-on-failure

echo "=== Build AU, VST3 and CLAP ==="
cmake --build "$BUILD" --parallel "$JOBS" --target SpaceTrace_AU SpaceTrace_VST3 SpaceTrace_CLAP

ARTEFACTS="$BUILD/SpaceTrace_artefacts/Release"
for bundle in AU/SpaceTrace.component VST3/SpaceTrace.vst3 CLAP/SpaceTrace.clap; do
    [ -d "$ARTEFACTS/$bundle" ] || fail "Build passed but $bundle could not be located."
done

echo "=== Stage release tree ==="
# Keep public staging whitelisted and testable, exactly as on Windows.
PORTABLE="$BUILD/portable/SpaceTrace"
"$PY" "$ROOT/scripts/stage_release.py" --source-root "$ROOT" \
    --vst3 "$ARTEFACTS/VST3/SpaceTrace.vst3" \
    --clap "$ARTEFACTS/CLAP/SpaceTrace.clap" \
    --au "$ARTEFACTS/AU/SpaceTrace.component" \
    --output "$PORTABLE" --allowed-output-root "$BUILD"

echo "=== Sign, package installer and disk image ==="
"$PY" "$ROOT/scripts/package_macos.py" --portable "$PORTABLE" --output-dir "$BUILD/release" --version "$VERSION"

echo "=== PASS ==="
echo "Disk image: $BUILD/release/SpaceTrace_${VERSION}_macOS_Universal.dmg"
echo "Staged release tree: $PORTABLE"
echo "Build log: $LOG"
echo "Nothing was installed automatically."
