#!/usr/bin/env python3
"""Check that every numbered Decision Record is listed in its index."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
from typing import Iterable, List, Optional, Set, Tuple


DECISION_FILE_RE = re.compile(r"^\d{3}-[^/]+\.md$")
INDEX_LINK_RE = re.compile(r"\((\d{3}-[^)]+\.md)\)")


def audit_decision_index(root: Path) -> Tuple[List[str], List[str]]:
    """Return ``(missing_from_index, stale_index_links)``."""

    decisions_dir = root.resolve() / "docs" / "decisions"
    files = {
        path.name
        for path in decisions_dir.iterdir()
        if path.is_file() and DECISION_FILE_RE.match(path.name)
    }
    index = (decisions_dir / "INDEX.md").read_text(encoding="utf-8")
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
    missing, stale = audit_decision_index(args.root)
    decisions_dir = args.root / "docs" / "decisions"
    decision_count = len(
        [
            path
            for path in decisions_dir.glob("*.md")
            if DECISION_FILE_RE.match(path.name)
        ]
    )
    print(
        f"decision index check: {decision_count} records, "
        f"{decision_count - len(missing)} indexed, "
        f"{len(missing) + len(stale)} issues"
    )
    if missing:
        print("missing from docs/decisions/INDEX.md:")
        for name in missing:
            print(f"  - {name}")
    if stale:
        print("stale links in docs/decisions/INDEX.md:")
        for name in stale:
            print(f"  - {name}")
    return 1 if missing or stale else 0


if __name__ == "__main__":
    raise SystemExit(main())
