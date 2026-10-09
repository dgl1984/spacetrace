#!/usr/bin/env python3
"""Stage the intentionally small SpaceTrace portable release tree.

This script is release infrastructure, not part of the public scripts bundle.
It centralizes the public-file whitelist so BUILD_WINDOWS.ps1 cannot
accidentally ship development handoffs, audits, source, or build dependencies.
"""
from __future__ import annotations

import argparse
import shutil
from pathlib import Path

PUBLIC_TOP_LEVEL = {
    "SpaceTrace.vst3",
    "SpaceTrace.clap",
    "Heads",
    "Docs",
    "Licenses",
    "scripts",
    "README.md",
    "LICENSE.txt",
}

PUBLIC_DOC_FILES = {
    "MANUAL.md",
    "CUSTOM_HEADS.md",
    "HEAD_CHANNEL_CALIBRATION.txt",
    "HEAD_CORRECTIONS.txt",
}
PUBLIC_DOC_DIRS = {"Correction_Models", "Correction_Plots"}

PUBLIC_SCRIPT_FILES = {
    "README.md",
    "requirements.txt",
    "convert_sofa_head.py",
    "sofa_to_sthrtf.py",
    "tool_common.py",
    "validate_head.py",
    "package_head.py",
    "prepare_stereo_pair.py",
    "prepare_mono_level.py",
    "compose_corrections.py",
    "normalize_correction.py",
    "analyze_head_channels.py",
    "plot_correction.py",
    "head_analysis.py",
    "check_native_package.py",
}

FORBIDDEN_PUBLIC_NAME_FRAGMENTS = (
    "AUDIT",
    "HANDOFF",
    "WIP",
    "GITHUB_PR",
    "INTERNAL_REPORT",
    "WINDOWS_FIRST_TEST",
    "MOVEMENT_SWEEP_REPORT",
    "LICENSING_DECISION",
    "GITHUB_PACKAGE",
)

PRIVATE_FILE_NAMES = {"agents.md", "claude.md", "gemini.md", "conversation.json", "transcript.json"}
PRIVATE_DIR_NAMES = {".codex", ".agents", ".claude", "graphify-out", ".git", "__pycache__"}


def is_private_path(path: Path) -> bool:
    return any(part.casefold() in PRIVATE_DIR_NAMES or part.casefold() in PRIVATE_FILE_NAMES
               or any(fragment in part.upper() for fragment in FORBIDDEN_PUBLIC_NAME_FRAGMENTS)
               for part in path.parts)


def die(message: str) -> "NoReturn":
    raise SystemExit(f"ERROR: {message}\nHOW TO FIX: verify the source checkpoint and release-staging arguments, then run again.")


def copy_path(src: Path, dst: Path) -> None:
    if not src.exists():
        die(f"required release input is missing: {src}")
    if src.is_dir():
        shutil.copytree(src, dst)
    else:
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)


def validate_public_tree(root: Path) -> None:
    actual = {p.name for p in root.iterdir()}
    if actual != PUBLIC_TOP_LEVEL:
        missing = sorted(PUBLIC_TOP_LEVEL - actual)
        extra = sorted(actual - PUBLIC_TOP_LEVEL)
        die(f"public root mismatch; missing={missing}, extra={extra}")

    for p in root.rglob("*"):
        if is_private_path(p.relative_to(root)):
            die(f"internal/development artifact leaked into public package: {p.relative_to(root)}")

    docs = root / "Docs"
    doc_files = {p.name for p in docs.iterdir() if p.is_file()}
    doc_dirs = {p.name for p in docs.iterdir() if p.is_dir()}
    if doc_files != PUBLIC_DOC_FILES or doc_dirs != PUBLIC_DOC_DIRS:
        die(f"Docs whitelist mismatch; files={sorted(doc_files)}, dirs={sorted(doc_dirs)}")

    scripts = root / "scripts"
    script_files = {p.name for p in scripts.iterdir() if p.is_file()}
    script_dirs = {p.name for p in scripts.iterdir() if p.is_dir()}
    if script_files != PUBLIC_SCRIPT_FILES or script_dirs:
        die(f"scripts whitelist mismatch; files={sorted(script_files)}, dirs={sorted(script_dirs)}")

    for required_license in ("THIRD_PARTY_NOTICES.txt", "IRCAM_LISTEN_NOTICE.txt",
                             "MIT_KEMAR_NOTICE.txt", "SADIE_II_LICENSE.txt",
                             "FULL2DEG_CC_BY_SA_3_0.txt", "FABIAN_CC_BY_4_0.txt",
                             "JUCE_NOTICE.txt", "CLAP_LICENSE.txt",
                             "CLAP_HELPERS_LICENSE.txt", "CLAP_JUCE_EXTENSIONS_LICENSE.txt"):
        if not (root / "Licenses" / required_license).is_file():
            die(f"required release license/notice is missing: Licenses/{required_license}")

    head_dirs = {p.name for p in (root / "Heads").iterdir() if p.is_dir()}
    expected_heads = {"IRCAM_1050", "MIT_KEMAR_Normal", "KU100_SADIE_D1", "KU100_FULL2DEG", "FABIAN_HATO0"}
    if head_dirs != expected_heads:
        die(f"shipping head set changed unexpectedly: {sorted(head_dirs)}")

    for head in sorted((root / "Heads").iterdir()):
        if not head.is_dir():
            continue
        for required in ("head.sthrtf", "manifest.json", "provenance.json", "LICENSE.txt"):
            if not (head / required).is_file():
                die(f"head {head.name} is missing public provenance/license asset {required}")


def validate_output_location(source_root: Path, output: Path, allowed_output_root: Path | None = None) -> None:
    source_root = source_root.resolve()
    output = output.resolve()

    if output == source_root:
        die(f"release output must not replace the source tree: {output}")
    if output in source_root.parents:
        die(f"release output must not contain the source tree: {output}")

    if source_root not in output.parents:
        return

    if allowed_output_root is None:
        die(f"release output must not be inside the source tree without an explicit allowed output root: {output}")

    allowed_output_root = allowed_output_root.resolve()
    if allowed_output_root == source_root or source_root not in allowed_output_root.parents:
        die(f"allowed output root must itself be a dedicated subdirectory of the source tree: {allowed_output_root}")

    if output != allowed_output_root and allowed_output_root not in output.parents:
        die(f"release output is outside the explicitly allowed output root: {output}")


def stage(source_root: Path, vst3: Path, clap: Path, output: Path, allowed_output_root: Path | None = None) -> None:
    source_root = source_root.resolve()
    output = output.resolve()
    validate_output_location(source_root, output, allowed_output_root)

    if output.exists():
        if output.is_symlink():
            die(f"refusing to clear symlinked output directory: {output}")
        shutil.rmtree(output)
    output.mkdir(parents=True)

    copy_path(vst3, output / "SpaceTrace.vst3")
    copy_path(clap, output / "SpaceTrace.clap")
    copy_path(source_root / "Heads", output / "Heads")
    copy_path(source_root / "Licenses", output / "Licenses")
    copy_path(source_root / "README.md", output / "README.md")
    copy_path(source_root / "LICENSE.txt", output / "LICENSE.txt")

    docs_out = output / "Docs"; docs_out.mkdir()
    for name in sorted(PUBLIC_DOC_FILES):
        copy_path(source_root / "Docs" / name, docs_out / name)
    for name in sorted(PUBLIC_DOC_DIRS):
        copy_path(source_root / "Docs" / name, docs_out / name)

    scripts_out = output / "scripts"; scripts_out.mkdir()
    for name in sorted(PUBLIC_SCRIPT_FILES):
        copy_path(source_root / "scripts" / name, scripts_out / name)

    validate_public_tree(output)


def main() -> int:
    ap = argparse.ArgumentParser(description="Stage and validate the clean SpaceTrace portable release package.")
    ap.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[1], help="SpaceTrace source checkpoint root (default: parent of scripts/).")
    ap.add_argument("--vst3", type=Path, required=True, help="Built SpaceTrace.vst3 file/bundle/directory.")
    ap.add_argument("--clap", type=Path, required=True, help="Built SpaceTrace.clap binary.")
    ap.add_argument("--output", type=Path, required=True, help="Destination directory for the portable SpaceTrace package.")
    ap.add_argument("--allowed-output-root", type=Path, help="Explicit safe subtree allowed when staging inside the source tree, e.g. the build directory.")
    args = ap.parse_args()
    stage(args.source_root, args.vst3, args.clap, args.output, args.allowed_output_root)
    print(f"SpaceTrace release staging: PASS: {args.output}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
