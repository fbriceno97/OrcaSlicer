#!/usr/bin/env python3
"""Expose bounded compiler diagnostics in GitHub's check annotations."""
from pathlib import Path
import re
import sys

from report_ctest_failures import escape_annotation


def diagnostics(text):
    lines = text.splitlines()
    starts = [i for i, line in enumerate(lines) if re.search(
        r"\berror(?:\s+[A-Z]+\d+)?\s*:|fatal error|CMake Error|FAILED:", line, re.IGNORECASE)]
    # Include context for template instantiation and linker failures. Limit the
    # report to GitHub's annotation quota, leaving space for the step exit code.
    if not starts:
        return ["\n".join(lines[-35:])[-6000:] or "Build failed without output."]
    return ["\n".join(lines[max(0, i - 2):i + 12])[:6000] for i in starts[:8]]


def main():
    text = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
    for message in diagnostics(text):
        print("::error title=Windows build failure::" + escape_annotation(message))


if __name__ == "__main__":
    main()
