#!/usr/bin/env python3
"""Check rendered Markdown links that point at files inside this repository.

The checker deliberately ignores network URLs and anchors. It is dependency-free
so it can run in the existing lightweight CI image.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import re
from typing import Iterable, List, Optional, Tuple
from urllib.parse import unquote, urlsplit


EXCLUDED_DIRECTORIES = {
    ".git",
    ".gradle",
    ".idea",
    "__pycache__",
    "build",
    "node_modules",
}

# This covers normal inline links and image links, including angle-bracket
# destinations with spaces. Reference-style links are intentionally out of
# scope because their definitions are not uniformly local to one line.
MARKDOWN_LINK_RE = re.compile(
    r"!?\[[^\]]*\]\(\s*(?P<target><[^>]*>|(?:\\.|[^)\s])+)(?:\s+[^)]*)?\)"
)


@dataclass(frozen=True)
class BrokenLink:
    source: Path
    line: int
    target: str
    resolved: Optional[Path]
    reason: str


def markdown_files(root: Path) -> List[Path]:
    """Return repository Markdown files, excluding generated/vendor trees."""

    files: List[Path] = []
    for path in root.rglob("*.md"):
        if any(part in EXCLUDED_DIRECTORIES for part in path.relative_to(root).parts):
            continue
        files.append(path)
    return sorted(files)


def _is_external(target: str) -> bool:
    parsed = urlsplit(target)
    return bool(parsed.scheme or parsed.netloc)


def _clean_target(raw_target: str) -> str:
    target = raw_target.strip()
    if target.startswith("<") and target.endswith(">"):
        target = target[1:-1]
    # Markdown permits backslash escapes in destinations. Decode only the
    # punctuation relevant to path resolution; URL percent escapes are handled
    # after urlsplit below.
    return target.replace(r"\(", "(").replace(r"\)", ")").replace(r"\ ", " ")


def _resolve_local_target(source: Path, root: Path, target: str) -> Tuple[Optional[Path], str]:
    target = _clean_target(target)
    parsed = urlsplit(target)
    if target.startswith("#") or _is_external(target):
        return None, "external-or-anchor"

    path_text = unquote(parsed.path)
    if not path_text:
        return None, "empty-path"

    root_resolved = root.resolve()
    candidate = (root_resolved / path_text.lstrip("/")) if path_text.startswith("/") else (
        source.parent.resolve() / path_text
    )
    candidate = candidate.resolve(strict=False)
    try:
        candidate.relative_to(root_resolved)
    except ValueError:
        return candidate, "outside-repository"
    if not candidate.exists():
        return candidate, "missing"
    return candidate, "ok"


def scan_markdown_links(root: Path) -> Tuple[int, int, List[BrokenLink]]:
    """Return ``(file_count, local_link_count, broken_links)`` for ``root``."""

    root = root.resolve()
    files = markdown_files(root)
    local_link_count = 0
    broken: List[BrokenLink] = []

    for source in files:
        in_fence = False
        for line_number, line in enumerate(source.read_text(encoding="utf-8").splitlines(), 1):
            stripped = line.lstrip()
            if stripped.startswith("```") or stripped.startswith("~~~"):
                in_fence = not in_fence
                continue
            if in_fence:
                continue
            for match in MARKDOWN_LINK_RE.finditer(line):
                target = _clean_target(match.group("target"))
                resolved, reason = _resolve_local_target(source, root, target)
                if reason == "external-or-anchor":
                    continue
                local_link_count += 1
                if reason != "ok":
                    broken.append(
                        BrokenLink(
                            source=source,
                            line=line_number,
                            target=target,
                            resolved=resolved,
                            reason=reason,
                        )
                    )

    return len(files), local_link_count, broken


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
    file_count, local_link_count, broken = scan_markdown_links(args.root)
    print(
        f"markdown link check: {file_count} files, "
        f"{local_link_count} local links, {len(broken)} broken"
    )
    for item in broken:
        resolved = str(item.resolved) if item.resolved is not None else "-"
        print(f"{item.source}:{item.line}: {item.reason}: {item.target} -> {resolved}")
    return 1 if broken else 0


if __name__ == "__main__":
    raise SystemExit(main())
