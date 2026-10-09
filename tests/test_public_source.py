#!/usr/bin/env python3
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from check_public_source import check_files


class PublicSourceTests(unittest.TestCase):
    def test_private_records_are_rejected_even_when_nested(self):
        paths = ["Docs/DEVELOPMENT_HANDOFF.md", "Docs/LICENSING_DECISION.md",
                 "Docs/GITHUB_PR_DRAFT.md", "WINDOWS_FIRST_TEST.md",
                 "Docs/notes/.codex/session.json", "Docs/AGENTS.md",
                 "Docs/notes/conversation.json"]
        self.assertEqual(len(check_files(Path("."), paths)), len(paths))

    def test_renamed_handoff_is_rejected_by_heading(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "notes.md").write_text("# SpaceTrace development handoff\n", encoding="utf-8")
            self.assertEqual(len(check_files(root, ["notes.md"])), 1)

    def test_public_technical_documentation_is_allowed(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "MANUAL.md").write_text("# SpaceTrace manual\nAudition the corrected head.\n", encoding="utf-8")
            self.assertEqual(check_files(root, ["MANUAL.md"]), [])


if __name__ == "__main__":
    unittest.main()
