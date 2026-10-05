#!/usr/bin/env python3
"""Shared helpers for SpaceTrace asset-preparation command-line tools."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import re
import sys


class ToolError(RuntimeError):
    """Expected, user-actionable command-line error."""

    def __init__(self, message: str, *, hint: str | None = None, code: int = 2):
        super().__init__(message)
        self.message = message
        self.hint = hint
        self.code = code


def die(exc: Exception) -> int:
    if isinstance(exc, ToolError):
        print(f"ERROR: {exc.message}", file=sys.stderr)
        if exc.hint:
            print(f"HOW TO FIX: {exc.hint}", file=sys.stderr)
        return exc.code
    print(f"ERROR: Unexpected failure: {exc}", file=sys.stderr)
    print("HOW TO FIX: Re-run with --help and verify the input files. If this persists, keep the full message for a bug report.", file=sys.stderr)
    return 99


def require_file(path: Path, label: str) -> Path:
    if not path.exists():
        raise ToolError(
            f"{label} was not found: {path}",
            hint="Check the filename and path. Paths containing spaces are fine when quoted by your shell.",
            code=3,
        )
    if not path.is_file():
        raise ToolError(f"{label} is not a file: {path}", hint="Choose the actual file, not its containing folder.", code=3)
    return path


def ensure_parent_writable(path: Path) -> None:
    parent = path.parent
    probe = parent
    while not probe.exists() and probe != probe.parent:
        probe = probe.parent
    if probe.exists() and not probe.is_dir():
        raise ToolError(f"Output path crosses a location that is not a directory: {probe}", hint="Choose an output path inside a real directory.", code=4)
    if parent.exists() and not parent.is_dir():
        raise ToolError(f"Output parent is not a directory: {parent}", hint="Choose an output path inside a real directory.", code=4)
    try:
        parent.mkdir(parents=True, exist_ok=True)
    except OSError as exc:
        raise ToolError(f"Cannot create output directory {parent}: {exc}", hint="Choose a writable location.", code=4) from exc
    if not os.access(parent, os.W_OK):
        raise ToolError(f"Output directory is not writable: {parent}", hint="Choose a writable folder or adjust its permissions.", code=4)


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for block in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def load_json(path: Path) -> dict:
    require_file(path, "JSON file")
    try:
        return json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ToolError(f"Could not read JSON from {path}: {exc}", hint="Repair or replace the JSON file, then try again.", code=5) from exc


def write_json(path: Path, data: dict) -> None:
    ensure_parent_writable(path)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


_STABLE_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{2,127}$")


def validate_stable_id(stable_id: str) -> str:
    if not _STABLE_ID_RE.fullmatch(stable_id):
        raise ToolError(
            f"Invalid stable ID: {stable_id!r}",
            hint="Use 3-128 characters containing only letters, numbers, '.', '_' or '-'. Example: mylab.head1.48000",
            code=6,
        )
    return stable_id


def check_duplicate_stable_id(heads_root: Path, stable_id: str, output_dir: Path | None = None) -> None:
    if not heads_root.exists():
        return
    for manifest in heads_root.glob("*/manifest.json"):
        if output_dir is not None and manifest.parent.resolve() == output_dir.resolve():
            continue
        try:
            data = json.loads(manifest.read_text(encoding="utf-8-sig"))
        except Exception:
            continue
        if data.get("stableId") == stable_id:
            raise ToolError(
                f"Stable ID {stable_id!r} is already used by {manifest.parent}",
                hint="Choose a new stable ID. Stable IDs are project-state identity and must remain unique.",
                code=7,
            )
