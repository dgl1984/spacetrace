#!/usr/bin/env python3
from pathlib import Path
import importlib.util
import tempfile
import contextlib
import io

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("stage_release", ROOT / "scripts" / "stage_release.py")
MOD = importlib.util.module_from_spec(SPEC); SPEC.loader.exec_module(MOD)

with tempfile.TemporaryDirectory(prefix="spacetrace-release-test-") as td:
    td = Path(td)
    vst3 = td / "fake.vst3"; vst3.mkdir(); (vst3 / "dummy.bin").write_bytes(b"vst3")
    clap = td / "fake.clap"; clap.write_bytes(b"clap")
    out = td / "portable" / "SpaceTrace"
    MOD.stage(ROOT, vst3, clap, out)
    MOD.validate_public_tree(out)
    assert not (out / "third_party").exists()
    assert not (out / "Docs" / "AUDIT_1.0_RC1_INTERNAL.md").exists()
    assert not (out / "Docs" / "BUILD_AND_RELEASE.md").exists()
    assert not (out / "scripts" / "Prepare-Datasets.ps1").exists()
    assert (out / "Licenses" / "THIRD_PARTY_NOTICES.txt").is_file()
    assert (out / "LICENSE.txt").is_file()
    for head in (out / "Heads").iterdir():
        assert (head / "provenance.json").is_file()
        assert (head / "LICENSE.txt").is_file()

# Path-safety regression checks for the Windows portable staging location.
with tempfile.TemporaryDirectory(prefix="spacetrace-release-path-test-") as td:
    td = Path(td)
    fake_source = td / "SpaceTrace"
    fake_source.mkdir()
    build_root = fake_source / "build-windows"
    portable = build_root / "portable" / "SpaceTrace"

    # The build subtree is allowed only when explicitly named.
    MOD.validate_output_location(fake_source, portable, build_root)

    # The same in-tree output must be rejected without that explicit boundary.
    try:
        with contextlib.redirect_stderr(io.StringIO()):
            MOD.validate_output_location(fake_source, portable)
    except SystemExit:
        pass
    else:
        raise AssertionError("in-source staging unexpectedly allowed without --allowed-output-root")

    # An explicit build root must not authorize some other source subtree.
    try:
        with contextlib.redirect_stderr(io.StringIO()):
            MOD.validate_output_location(fake_source, fake_source / "Docs" / "release", build_root)
    except SystemExit:
        pass
    else:
        raise AssertionError("allowed output root escaped its intended build subtree")

    # An output outside the source tree remains valid without a special allowance.
    MOD.validate_output_location(fake_source, td / "public-release")

    # A parent output would erase the source on re-staging. Reject it first.
    sentinel = fake_source / "keep.txt"
    sentinel.write_text("preserve")
    for invalid in (fake_source, td):
        try:
            MOD.stage(fake_source, vst3, clap, invalid)
        except SystemExit:
            pass
        else:
            raise AssertionError("destructive source/ancestor output accepted")
        assert sentinel.read_text() == "preserve"

print("SpaceTrace release staging contract: PASS")
