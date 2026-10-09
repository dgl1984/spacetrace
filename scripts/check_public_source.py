#!/usr/bin/env python3
"""Check tracked source files for private development records before publication."""
from pathlib import Path
import re
import subprocess

from stage_release import is_private_path

PRIVATE_HEADING = re.compile(
    r"^#{1,6}\s+.*(?:development handoff|internal (?:audit|report)|PR description draft)",
    re.IGNORECASE | re.MULTILINE,
)


def check_files(root: Path, paths: list[str]) -> list[str]:
    problems = []
    for name in paths:
        path = Path(name)
        if is_private_path(path):
            problems.append(f"private development path: {name}")
        elif path.suffix.lower() in {".md", ".txt"}:
            text = (root / path).read_text(encoding="utf-8-sig")
            if PRIVATE_HEADING.search(text):
                problems.append(f"private development heading: {name}")
    return problems


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    tracked = subprocess.check_output(["git", "ls-files", "-z"], cwd=root)
    paths = tracked.decode("utf-8").rstrip("\0").split("\0") if tracked else []
    problems = check_files(root, paths)
    if problems:
        print("Publication check failed:\n" + "\n".join(problems))
        return 1
    print(f"Public source check: PASS ({len(paths)} tracked files checked)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
