#!/usr/bin/env python3
"""Sign a staged SpaceTrace macOS release and build its disk image.

This is release infrastructure, not part of the public scripts bundle.
BUILD_MACOS.sh stages the release with stage_release.py, then calls this script.

The disk image holds "Install SpaceTrace.pkg" beside the manual, custom-head
scripts and licences. The installer only places the three plug-in bundles;
each bundle carries its own heads, so deleting a bundle uninstalls it.

Signing is ad hoc unless a Developer ID is supplied through the environment:

  SPACETRACE_CODESIGN_IDENTITY   "Developer ID Application: ..." (default "-": ad hoc)
  SPACETRACE_INSTALLER_IDENTITY  "Developer ID Installer: ..." (default: unsigned package)
  SPACETRACE_NOTARY_PROFILE      notarytool keychain profile (default: not notarized)

Ad hoc signatures satisfy Apple Silicon but not Gatekeeper. Without a Developer
ID, users approve the installer once in System Settings > Privacy & Security;
the installed plug-ins are not quarantined.
"""
from __future__ import annotations

import argparse
import hashlib
import os
import plistlib
import shutil
import subprocess
import tempfile
from pathlib import Path
from xml.sax.saxutils import escape

from stage_release import MACOS_AU_NAME, validate_public_tree

PLUGIN_BUNDLES = (
    # (bundle, install folder, choice title)
    ("SpaceTrace.vst3", "/Library/Audio/Plug-Ins/VST3", "VST3 plug-in"),
    ("SpaceTrace.clap", "/Library/Audio/Plug-Ins/CLAP", "CLAP plug-in"),
    (MACOS_AU_NAME, "/Library/Audio/Plug-Ins/Components", "Audio Unit plug-in (Logic, GarageBand)"),
)
DISK_IMAGE_ITEMS = ("Docs", "Licenses", "scripts", "README.md", "LICENSE.txt")
INSTALLER_NAME = "Install SpaceTrace.pkg"
IDENTIFIER = "com.lanesaudio.spacetrace"
MINIMUM_MACOS = "12.0"

# Make Logic and other AU hosts rescan the newly installed component.
AU_POSTINSTALL = """#!/bin/sh
/usr/bin/killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
exit 0
"""


def die(message: str) -> "NoReturn":
    raise SystemExit(f"ERROR: {message}")


def run(*args: str) -> None:
    print("+ " + " ".join(args))
    subprocess.run(args, check=True)


def sign_bundle(bundle: Path, identity: str) -> None:
    command = ["codesign", "--force", "--sign", identity]
    if identity != "-":
        command += ["--options", "runtime", "--timestamp"]
    run(*command, str(bundle))
    run("codesign", "--verify", "--strict", "--deep", "--verbose=2", str(bundle))


def component_package(bundle: Path, location: str, identifier: str, scripts: Path | None, out: Path,
                      version: str) -> None:
    with tempfile.TemporaryDirectory(prefix="spacetrace-pkg-") as td:
        root = Path(td) / "root"
        root.mkdir()
        shutil.copytree(bundle, root / bundle.name, symlinks=True)
        # Installer would otherwise "relocate" an upgrade into any other copy
        # of the bundle it finds on disk, such as a build folder.
        components = Path(td) / "components.plist"
        run("pkgbuild", "--analyze", "--root", str(root), str(components))
        with components.open("rb") as f:
            entries = plistlib.load(f)
        for entry in entries:
            entry["BundleIsRelocatable"] = False
            entry["BundleOverwriteAction"] = "upgrade"
        with components.open("wb") as f:
            plistlib.dump(entries, f)
        command = ["pkgbuild", "--root", str(root), "--component-plist", str(components),
                   "--install-location", location, "--identifier", identifier, "--version", version]
        if scripts is not None:
            command += ["--scripts", str(scripts)]
        run(*command, str(out))


def distribution_xml(version: str, packages: list[tuple[str, str, str]]) -> str:
    """packages: (identifier, file name, title)."""
    lines = [
        '<?xml version="1.0" encoding="utf-8"?>',
        '<installer-gui-script minSpecVersion="2">',
        f'    <title>SpaceTrace {escape(version)}</title>',
        '    <license file="LICENSE.txt"/>',
        '    <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>',
        '    <domains enable_localSystem="true"/>',
        f'    <os-version min="{MINIMUM_MACOS}"/>',
        '    <choices-outline>',
    ]
    lines += [f'        <line choice="{identifier}"/>' for identifier, _, _ in packages]
    lines.append('    </choices-outline>')
    for identifier, _, title in packages:
        lines.append(f'    <choice id="{identifier}" title="{escape(title)}" visible="true">')
        lines.append(f'        <pkg-ref id="{identifier}"/>')
        lines.append('    </choice>')
    for identifier, file_name, _ in packages:
        lines.append(f'    <pkg-ref id="{identifier}" version="{escape(version)}">{escape(file_name)}</pkg-ref>')
    lines.append('</installer-gui-script>')
    return "\n".join(lines) + "\n"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser(description="Sign the staged SpaceTrace macOS release and build its disk image.")
    ap.add_argument("--portable", type=Path, required=True, help="Staged release tree from stage_release.py --au.")
    ap.add_argument("--output-dir", type=Path, required=True, help="Directory for the .dmg and SHA256SUMS.txt.")
    ap.add_argument("--version", required=True, help="SpaceTrace version, e.g. 1.0.1.")
    args = ap.parse_args()

    portable = args.portable.resolve()
    validate_public_tree(portable, au=True)

    identity = os.environ.get("SPACETRACE_CODESIGN_IDENTITY", "-")
    installer_identity = os.environ.get("SPACETRACE_INSTALLER_IDENTITY", "")
    notary_profile = os.environ.get("SPACETRACE_NOTARY_PROFILE", "")
    if notary_profile and (identity == "-" or not installer_identity):
        die("notarization needs SPACETRACE_CODESIGN_IDENTITY and SPACETRACE_INSTALLER_IDENTITY Developer IDs.")

    for bundle, _, _ in PLUGIN_BUNDLES:
        sign_bundle(portable / bundle, identity)

    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    image = output_dir / f"SpaceTrace_{args.version}_macOS_Universal.dmg"

    with tempfile.TemporaryDirectory(prefix="spacetrace-product-") as td:
        work = Path(td)
        packages: list[tuple[str, str, str]] = []
        au_scripts = work / "au-scripts"
        au_scripts.mkdir()
        (au_scripts / "postinstall").write_text(AU_POSTINSTALL, encoding="ascii")
        (au_scripts / "postinstall").chmod(0o755)
        for bundle, location, title in PLUGIN_BUNDLES:
            suffix = Path(bundle).suffix.lstrip(".")
            identifier = f"{IDENTIFIER}.{suffix}"
            file_name = f"SpaceTrace-{suffix}.pkg"
            scripts = au_scripts if bundle == MACOS_AU_NAME else None
            component_package(portable / bundle, location, identifier, scripts, work / file_name, args.version)
            packages.append((identifier, file_name, title))

        resources = work / "resources"
        resources.mkdir()
        shutil.copy2(portable / "LICENSE.txt", resources / "LICENSE.txt")
        distribution = work / "distribution.xml"
        distribution.write_text(distribution_xml(args.version, packages), encoding="utf-8")

        volume = work / f"SpaceTrace {args.version}"
        volume.mkdir()
        installer = volume / INSTALLER_NAME
        command = ["productbuild", "--distribution", str(distribution), "--package-path", str(work),
                   "--resources", str(resources)]
        if installer_identity:
            command += ["--sign", installer_identity, "--timestamp"]
        run(*command, str(installer))
        if notary_profile:
            run("xcrun", "notarytool", "submit", str(installer), "--keychain-profile", notary_profile, "--wait")
            run("xcrun", "stapler", "staple", str(installer))

        for name in DISK_IMAGE_ITEMS:
            source = portable / name
            if source.is_dir():
                shutil.copytree(source, volume / name)
            else:
                shutil.copy2(source, volume / name)

        if image.exists():
            image.unlink()
        run("hdiutil", "create", "-volname", volume.name, "-srcfolder", str(volume),
            "-format", "UDZO", "-imagekey", "zlib-level=9", "-ov", str(image))

    if identity != "-":
        run("codesign", "--force", "--sign", identity, "--timestamp", str(image))
    if notary_profile:
        run("xcrun", "notarytool", "submit", str(image), "--keychain-profile", notary_profile, "--wait")
        run("xcrun", "stapler", "staple", str(image))
    run("hdiutil", "verify", str(image))

    sums = output_dir / "SHA256SUMS.txt"
    sums.write_text(f"{sha256(image)}  {image.name}\n", encoding="ascii")
    signing = "Developer ID" if identity != "-" else "ad hoc (not notarized)"
    print(f"SpaceTrace macOS disk image: PASS: {image} [{signing}]")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
