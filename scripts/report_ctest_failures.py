#!/usr/bin/env python3
"""Expose CTest JUnit failures in GitHub check annotations and the job summary."""
import os
from pathlib import Path
import sys
import xml.etree.ElementTree as ET


def escape_annotation(text):
    return text.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")


def main():
    report = Path(sys.argv[1])
    if not report.is_file():
        print("No JUnit report was produced; inspect the test setup or runner failure.")
        return
    try:
        cases = list(ET.parse(report).getroot().iter("testcase"))
    except ET.ParseError as error:
        details = report.read_text(encoding="utf-8", errors="replace")[-5000:]
        message = f"{report.name}: incomplete test report ({error}). The test process may have terminated.\n{details}"
        print("::error title=Incomplete test report::" + escape_annotation(message))
        return
    failed = []
    skipped = 0
    for case in cases:
        failure = case.find("failure")
        if failure is None:
            failure = case.find("error")
        if failure is not None:
            name = case.get("name", "Unnamed test")
            details = "\n".join(part for part in (
                failure.get("message", ""), failure.text or "", case.findtext("system-out", "")
            ) if part.strip()) or "Test failed without a message"
            # Bound annotation size while retaining the end of a long test's diagnostics.
            message = f"{name}\n{details[-6000:]}"
            print(f"::error title=CTest failure::{escape_annotation(message)}")
            failed.append(name)
        elif case.find("skipped") is not None:
            skipped += 1
    summary = f"Tests: {len(cases)}, failed: {len(failed)}, skipped: {skipped}.\n"
    print(summary, end="")
    summary_path = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary_path:
        with open(summary_path, "a", encoding="utf-8") as output:
            output.write(summary)
            for name in failed:
                output.write("\n- Failed: " + " ".join(name.split()) + "\n")


if __name__ == "__main__":
    main()
