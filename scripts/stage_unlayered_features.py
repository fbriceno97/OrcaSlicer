#!/usr/bin/env python3
"""Stage Unlayered v0.1.7 source geometry on an isolated custom Orca branch.

This imports a patch, NOT a supported/print-safe binary. Compile, test, inspect
G-code and validate printer clearances before merging. Never import Unlayered's
Bambu cloud/account removal, branding, or own update infrastructure.
"""
from __future__ import annotations
import os
import pathlib
import subprocess
import sys

SOURCE = "https://github.com/Unlayered3D/Unlayered-Slicer.git"
TAG = "v0.1.7"
BRANCH = "feature/unlayered-wave-nonplanar"
PREFIXES = (
    "src/libslic3r/", "src/libvgcode/",
    "src/slic3r/GUI/Gizmos/", "src/slic3r/GUI/Jobs/",
    "src/slic3r/Utils/Nonplanar", "tests/fff_print/", "tests/libslic3r/",
)
FILES = {
    "src/slic3r/CMakeLists.txt",
    "src/slic3r/GUI/Plater.cpp", "src/slic3r/GUI/Plater.hpp",
    "src/slic3r/GUI/GLCanvas3D.cpp", "src/slic3r/GUI/GLCanvas3D.hpp",
    "src/slic3r/GUI/GCodeViewer.cpp", "src/slic3r/GUI/GCodeViewer.hpp",
    "src/slic3r/GUI/ConfigManipulation.cpp", "src/slic3r/GUI/ConfigManipulation.hpp",
    "src/slic3r/GUI/PresetHints.cpp", "src/slic3r/GUI/PresetHints.hpp",
    "src/slic3r/GUI/IMSlider.cpp", "src/slic3r/GUI/IMSlider.hpp",
    "src/slic3r/GUI/OptionsGroup.cpp", "src/slic3r/GUI/OptionsGroup.hpp",
    "src/slic3r/GUI/3DScene.cpp", "src/slic3r/GUI/3DScene.hpp",
    "src/slic3r/GUI/WebGuideDialog.cpp", "src/slic3r/GUI/WebGuideDialog.hpp",
    "resources/web/tutorial/multiplanar.html",
}
SKIP_PREFIXES = (
    "src/libslic3r/WaveOverhangs/",  # already pinned and imported separately
    "src/libslic3r/AppConfig.", "src/libslic3r/Preset.",
    "src/slic3r/GUI/Gizmos/GLGizmoMmuSegmentation.",
    "src/slic3r/GUI/Gizmos/GLGizmoPainterBase.",
    "src/slic3r/GUI/Gizmos/GLGizmoText.",
    "src/slic3r/GUI/Jobs/Cloud",
)
SKIP_FILES = {
    "src/slic3r/GUI/GUI_App.cpp", "src/slic3r/GUI/GUI_App.hpp",
    "src/slic3r/GUI/MainFrame.cpp", "src/slic3r/GUI/MainFrame.hpp",
}

def cmd(*args, check=True, data=None):
    p = subprocess.run(args, input=data, capture_output=True, check=False)
    if check and p.returncode:
        raise RuntimeError("Command failed: " + " ".join(args) + "\n" +
                           p.stderr.decode(errors="replace")[:3000])
    return p

def report(message):
    print(message, flush=True)
    path = os.environ.get("GITHUB_STEP_SUMMARY")
    if path:
        with open(path, "a", encoding="utf-8") as f:
            f.write(message + "\n")

def main():
    branch = cmd("git", "branch", "--show-current").stdout.decode().strip()
    if branch not in (BRANCH, ""):
        raise RuntimeError("Import prohibited on branch: " + branch)
    if cmd("git", "status", "--porcelain").stdout.strip():
        raise RuntimeError("Import requires clean worktree")
    cmd("git", "remote", "add", "unlayered-stage", SOURCE)
    try:
        cmd("git", "fetch", "--no-tags", "--filter=blob:none",
            "unlayered-stage", "refs/tags/"+TAG+":refs/remotes/unlayered-stage/pinned")
        source = "refs/remotes/unlayered-stage/pinned"
        base = cmd("git", "merge-base", "HEAD", source).stdout.decode().strip()
        if not base:
            raise RuntimeError("No shared OrcaSlicer ancestor; refusing force-import")
        report("### Unlayered source candidate from "+TAG+"\nCommon ancestor: "+base)
        # Stage NEW self-contained feature sources only. The full patch was
        # intentionally tried first and had extensive conflicts with our Orca
        # base; overwriting existing paths would lose Bambu customizations.
        raw = cmd("git", "diff", "--diff-filter=A", "--name-only", "-z",
                  base, source, "--").stdout
        paths = [x.decode() for x in raw.split(b"\0") if x]
        features = ("WaveOverhangs", "CutPlane", "Skeleton", "HotendCollision",
                    "Trunnion", "SegmentSupport", "Nonplanar", "nonplanar",
                    "wave_overhangs", "segment_", "cut_plane",
                    "virtual_tilt", "multiplanar")
        chosen = sorted(p for p in paths if
                        (p.startswith(PREFIXES) or p in FILES)
                        and not p.startswith(SKIP_PREFIXES)
                        and p not in SKIP_FILES
                        and any(word in p for word in features)
                        and pathlib.Path(p).suffix.lower() in
                            (".cpp", ".hpp", ".h", ".py", ".html"))
        imported = []
        skipped = []
        for p in chosen:
            target = pathlib.Path(p)
            if target.exists():
                skipped.append(p)
                continue
            blob = cmd("git", "show", source+":"+p).stdout
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(blob)
            cmd("git", "add", "--", p)
            imported.append(p)
        report("Imported "+str(len(imported))+" previously absent geometry source files; "
               "skipped "+str(len(skipped))+" paths already in our fork.")
        report("The modified Orca core, CMake wiring, and Bambu safety gates "
               "still require a manual conflict-resolution pass. No code is enabled.")
        for filename in ("WaveOverhangs.cpp", "WaveOverhangs.hpp"):
            if not pathlib.Path("src/libslic3r/WaveOverhangs", filename).is_file():
                raise RuntimeError("Missing generator file: "+filename)
        report("**Experimental source staged, NOT built/validated.** "
               "Complete skipped GUI integration hooks, check Bambu Connect and paint, "
               "build and run tests before enabling a release.")
    finally:
        cmd("git", "remote", "remove", "unlayered-stage", check=False)

if __name__ == "__main__":
    main()
