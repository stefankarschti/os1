#!/usr/bin/env python3
"""Validate live Markdown links, the documentation index, and plan metadata."""

from __future__ import annotations

from pathlib import Path, PurePosixPath
from urllib.parse import unquote, urlsplit
import argparse
import re
import sys


REPO_ROOT = Path(__file__).resolve().parents[1]
LIVE_DOCUMENTS = (
    "AGENTS.md",
    "README.md",
    "GOALS.md",
    "doc/README.md",
    "doc/ARCHITECTURE.md",
    "doc/REFERENCES.md",
    "doc/latest-review.md",
)
INLINE_LINK_RE = re.compile(r"!?\[[^\]]*\]\((?P<target>[^)]+)\)")
REFERENCE_LINK_RE = re.compile(r"^\s*\[[^\]]+\]:\s*(?P<target>\S+)", re.MULTILINE)
PLAN_STATUS_RE = re.compile(r"^> Status: (?P<status>active|implemented|superseded)$", re.MULTILINE)
PLAN_OWNER_RE = re.compile(r"^> Owner: (?P<owner>.+?)\s*$", re.MULTILINE)
PLAN_VERIFIED_RE = re.compile(
    r"^> Last verified: (?P<date>\d{4}-\d{2}-\d{2}) at `(?P<commit>[0-9a-f]{7,40})`\s*$",
    re.MULTILINE,
)
MAX_LINK_TARGET_LENGTH = 4096


def _contains_control_character(value: str) -> bool:
    return any(ord(character) < 32 or ord(character) == 127 for character in value)


def _extract_target(raw_target: str) -> str:
    target = raw_target.strip()
    if target.startswith("<"):
        end = target.find(">")
        return target[1:end] if end >= 0 else target
    return target.split(maxsplit=1)[0] if target else ""


def extract_links(contents: str) -> list[str]:
    links = [_extract_target(match.group("target")) for match in INLINE_LINK_RE.finditer(contents)]
    links.extend(_extract_target(match.group("target")) for match in REFERENCE_LINK_RE.finditer(contents))
    return [link for link in links if link]


def _resolve_local_link(document: Path, target: str, repo_root: Path) -> tuple[Path | None, str | None]:
    if len(target) > MAX_LINK_TARGET_LENGTH:
        return None, "link target is too long"
    if _contains_control_character(target):
        return None, "link target contains a control character"
    try:
        parsed = urlsplit(target)
    except ValueError:
        return None, "link target has invalid URL syntax"
    if parsed.scheme or parsed.netloc or target.startswith("#"):
        return None, None
    decoded_path = unquote(parsed.path)
    if _contains_control_character(decoded_path):
        return None, "link target contains a control character"
    if not decoded_path:
        return None, None
    target_path = Path(decoded_path)
    if target_path.is_absolute():
        return None, "absolute local links are not portable"
    resolved = (document.parent / target_path).resolve()
    try:
        resolved.relative_to(repo_root)
    except ValueError:
        return None, "link escapes the repository"
    return resolved, None


def _all_markdown(repo_root: Path) -> list[Path]:
    documents = [repo_root / name for name in ("AGENTS.md", "README.md", "GOALS.md")]
    documents.extend(sorted((repo_root / "doc").rglob("*.md")))
    return documents


def _check_links(document: Path, repo_root: Path, check_all_local: bool) -> list[str]:
    errors: list[str] = []
    contents = document.read_text(encoding="utf-8")
    for target in extract_links(contents):
        resolved, error = _resolve_local_link(document, target, repo_root)
        if error is not None:
            errors.append(f"{document.relative_to(repo_root)}: '{target}': {error}")
            continue
        if resolved is None:
            continue
        if not check_all_local and resolved.suffix.lower() != ".md":
            continue
        if not resolved.exists():
            errors.append(f"{document.relative_to(repo_root)}: missing link target '{target}'")
    return errors


def _check_index(repo_root: Path, documents: list[Path]) -> list[str]:
    index_path = repo_root / "doc" / "README.md"
    contents = index_path.read_text(encoding="utf-8")
    indexed: set[Path] = set()
    for target in extract_links(contents):
        resolved, error = _resolve_local_link(index_path, target, repo_root)
        if error is None and resolved is not None and resolved.suffix.lower() == ".md":
            indexed.add(resolved)

    errors: list[str] = []
    for document in documents:
        if document == index_path or document.parent == repo_root:
            continue
        if document not in indexed:
            errors.append(f"doc/README.md: unindexed document '{document.relative_to(repo_root)}'")
    return errors


def _check_plan_metadata(repo_root: Path) -> list[str]:
    errors: list[str] = []
    plan_root = repo_root / "doc" / "exec-plans"
    for directory, allowed_statuses in (
        (plan_root / "active", {"active"}),
        (plan_root / "completed", {"implemented", "superseded"}),
    ):
        for plan in sorted(directory.glob("*.md")):
            contents = plan.read_text(encoding="utf-8")
            status = PLAN_STATUS_RE.search(contents)
            owner = PLAN_OWNER_RE.search(contents)
            verified = PLAN_VERIFIED_RE.search(contents)
            relative = plan.relative_to(repo_root)
            if status is None or status.group("status") not in allowed_statuses:
                errors.append(f"{relative}: invalid status for {directory.name} plan")
            if owner is None or not owner.group("owner").strip() or "<" in owner.group("owner"):
                errors.append(f"{relative}: missing concrete owner")
            if verified is None:
                errors.append(f"{relative}: invalid last-verified date/commit")
    return errors


def check_repository(repo_root: Path) -> tuple[list[str], int]:
    repo_root = repo_root.resolve()
    errors: list[str] = []
    if not (repo_root / "doc").is_dir():
        return [f"missing documentation directory: {repo_root / 'doc'}"], 0

    documents = _all_markdown(repo_root)
    missing_live = [name for name in LIVE_DOCUMENTS if not (repo_root / name).is_file()]
    errors.extend(f"missing live document '{name}'" for name in missing_live)

    live_paths = {(repo_root / name).resolve() for name in LIVE_DOCUMENTS}
    for document in documents:
        if not document.is_file():
            errors.append(f"missing expected document '{document.relative_to(repo_root)}'")
            continue
        errors.extend(_check_links(document, repo_root, document.resolve() in live_paths))
    if (repo_root / "doc" / "README.md").is_file():
        errors.extend(_check_index(repo_root, documents))
    errors.extend(_check_plan_metadata(repo_root))
    return sorted(set(errors)), len(documents)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    parse_args(sys.argv[1:] if argv is None else argv)
    try:
        errors, count = check_repository(REPO_ROOT)
    except (OSError, UnicodeError, ValueError) as exc:
        print(f"documentation check could not run: {exc}", file=sys.stderr)
        return 2
    if errors:
        print("documentation check failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1
    print(f"Documentation check passed: {count} Markdown files are indexed and valid.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
