#!/usr/bin/env python3
"""Check that every numbered experiment Markdown file is indexed."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
from typing import Iterable, List, Optional, Set, Tuple


EXPERIMENT_FILE_RE = re.compile(r"^EXP-\d+-[^/]+\.md$")
INDEX_LINK_RE = re.compile(r"\((EXP-\d+-[^)]+\.md)\)")


def audit_experiment_index(root: Path) -> Tuple[List[str], List[str]]:
    """Return ``(missing_from_index, stale_index_links)``."""

    experiments_dir = root.resolve() / "experiments"
    files = {
        path.name
        for path in experiments_dir.iterdir()
        if path.is_file() and EXPERIMENT_FILE_RE.match(path.name)
    }
    index = (experiments_dir / "README.md").read_text(encoding="utf-8")
    indexed: Set[str] = set(INDEX_LINK_RE.findall(index))
    return sorted(files - indexed), sorted(indexed - files)


def main(argv: Optional[Iterable[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "root",
        nargs="?",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="repository root (default: directory containing tools/)",
    )
    args = parser.parse_args(list(argv) if argv is not None else None)
    missing, stale = audit_experiment_index(args.root)
    experiment_count = len(
        [
            path
            for path in (args.root / "experiments").glob("EXP-*.md")
            if EXPERIMENT_FILE_RE.match(path.name)
        ]
    )
    print(
        f"experiment index check: {experiment_count} files, "
        f"{experiment_count - len(missing)} indexed, "
        f"{len(missing) + len(stale)} issues"
    )
    if missing:
        print("missing from experiments/README.md:")
        for name in missing:
            print(f"  - {name}")
    if stale:
        print("stale links in experiments/README.md:")
        for name in stale:
            print(f"  - {name}")
    return 1 if missing or stale else 0


if __name__ == "__main__":
    raise SystemExit(main())
