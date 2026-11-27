#!/usr/bin/env python3

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# Other people's trees, and what builds write.
FOREIGN = ("adsbee", "firmware/components/daidalus", "firmware/managed_components", "firmware/third_party",
           "firmware/build", "firmware/cmake-build-debug", "firmware/utility/build", "firmware/tzdb/build",
           "firmware/tzdb/venv", "hardware/sim/.venv", ".git", ".idea", ".claude")
ENDINGS = (".md", ".adoc", ".py", ".sh", ".c", ".cpp", ".h", ".hpp", ".json", ".xml", ".cmake", ".txt", ".html", ".yml")
BUILT = ("build/", "netmap/", ".venv/", "pdf2text/", "data/", "~/")
MARKDOWN_LINK = re.compile(r"\]\(([^)\s#]+)(?:#[^)]*)?\)")
ASCIIDOC_LINK = re.compile(r"\b(?:link|xref):([^\s\[#]+)(?:#[^\s\[]*)?\[")
QUOTED = re.compile(r"`([^`\s]+)`")
PLACEHOLDER = re.compile(r"[<>*$|{}]|^-|^[A-Z_]+=|^https?:|^\.\.\.")


def documents():
    for name in ("README.md", "README.adoc", "AGENTS.md"):
        for document in ROOT.rglob(name):
            relative = document.relative_to(ROOT).as_posix()
            if not relative.startswith(FOREIGN) and not document.is_symlink():
                yield document


def named(text):
    """The paths a text names: every link target, and what stands in backticks and looks like a path."""
    for pattern in (MARKDOWN_LINK, ASCIIDOC_LINK):
        yield from (target for target in pattern.findall(text) if not PLACEHOLDER.search(target))
    for quoted in QUOTED.findall(text):
        path = quoted.rstrip(".,:;")
        if "/" in path and path.endswith((*ENDINGS, "/")) and not PLACEHOLDER.search(path):
            yield path


def exists(path, document):
    path = path.replace("%20", " ")
    return any((base / path).exists() for base in (document.parent, ROOT, ROOT / "firmware"))


def main():
    findings = 0
    for document in sorted(documents()):
        text = document.read_text()
        for path in sorted(set(named(text))):
            if not any(part in path for part in BUILT) and not exists(path, document):
                print(f"FAIL docs     {document.relative_to(ROOT)}  names {path}, which is not there")
                findings += 1
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
