import tempfile
import unittest
from pathlib import Path

from tools.check_decision_index import audit_decision_index
from tools.check_experiment_index import audit_experiment_index
from tools.check_markdown_links import scan_markdown_links


class MarkdownLinkAuditTest(unittest.TestCase):
    def test_local_links_and_images_are_checked_but_urls_and_anchors_are_ignored(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "docs").mkdir()
            (root / "assets").mkdir()
            (root / "README.md").write_text(
                """# Test

[valid](docs/guide.md) ![image](<assets/test image.png>)
[remote](https://example.com/doc) [anchor](#section)
[missing](docs/missing.md)
""",
                encoding="utf-8",
            )
            (root / "docs/guide.md").write_text("# Guide\n", encoding="utf-8")
            (root / "assets/test image.png").write_bytes(b"png")

            files, local_links, broken = scan_markdown_links(root)

            self.assertEqual(files, 2)
            self.assertEqual(local_links, 3)
            self.assertEqual(len(broken), 1)
            self.assertEqual(broken[0].target, "docs/missing.md")
            self.assertEqual(broken[0].reason, "missing")

    def test_fenced_examples_are_not_treated_as_rendered_links(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "README.md").write_text(
                "```text\n[example](missing.md)\n```\n",
                encoding="utf-8",
            )
            _, local_links, broken = scan_markdown_links(root)
            self.assertEqual(local_links, 0)
            self.assertEqual(broken, [])


class ExperimentIndexAuditTest(unittest.TestCase):
    def test_index_reports_missing_and_stale_entries(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            experiments = root / "experiments"
            experiments.mkdir()
            (experiments / "EXP-001-one.md").write_text("# one\n", encoding="utf-8")
            (experiments / "EXP-002-two.md").write_text("# two\n", encoding="utf-8")
            (experiments / "README.md").write_text(
                "- [one](EXP-001-one.md)\n- [old](EXP-009-old.md)\n",
                encoding="utf-8",
            )

            missing, stale = audit_experiment_index(root)

            self.assertEqual(missing, ["EXP-002-two.md"])
            self.assertEqual(stale, ["EXP-009-old.md"])


class DecisionIndexAuditTest(unittest.TestCase):
    def test_index_reports_missing_and_stale_records(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            decisions = root / "docs" / "decisions"
            decisions.mkdir(parents=True)
            (decisions / "001-one.md").write_text("# one\n", encoding="utf-8")
            (decisions / "002-two.md").write_text("# two\n", encoding="utf-8")
            (decisions / "INDEX.md").write_text(
                "- [one](001-one.md)\n- [old](009-old.md)\n",
                encoding="utf-8",
            )

            missing, stale = audit_decision_index(root)

            self.assertEqual(missing, ["002-two.md"])
            self.assertEqual(stale, ["009-old.md"])


if __name__ == "__main__":
    unittest.main()
