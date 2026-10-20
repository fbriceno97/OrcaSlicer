# Bambu Studio 2.8.4.57 painting comparison

Reference: `bambulab/BambuStudio` tag `v02.08.04.57`, commit
`f977235e6d736c4c0b650520ac5a5b72cbfe9244` (AGPL-3.0).

## Implemented changes

- Color brushes use the reference's `min(radius / 5, 0.2 mm)` subdivision
  limit. Other Orca painters retain their 0.05 mm default. This changes
  recursive mesh subdivision, not just brush appearance.
- Height-range cursors use world coordinates for subdivision and band height,
  including uniformly scaled models, adapted from the reference constructor.
  Non-uniformly scaled camera positions are transformed once.
- Ctrl+wheel recomputes bucket-fill previews using bucket traversal, preserving
  painted-color boundaries, rather than accidentally invoking smart fill.
- Ctrl+wheel leaves Connected Same Color enabled. The negative threshold remains
  a mode sentinel, not a geometry angle.
- Wheel previews use the painter's filtered model-part transforms, avoiding
  indexing modifiers as if they were paintable volumes.
- The remembered geometry threshold starts at the reference's 30 degrees.
- Keyboard tool changes clear stale selection previews and update gap-fill mode.
- Section View uses the reference camera-facing orientation and preserves it while
  changing depth. Reset captures the current camera direction; Alt+wheel changes
  depth. Changing the section invalidates both the picked hit and fill preview.

## Selection semantics

The reference bucket traversal and Orca already share the following behavior:

1. Find the unsplit leaf containing the hit point.
2. Traverse touching leaf neighbors, including propagated neighbors at differing
   subdivision depths.
3. Require the same stored filament state as the starting leaf.
4. Exclude clipped neighbors.
5. With geometry detection enabled, compare adjacent original face normals
   against `cos(angle) - EPSILON`, with the dot product clamped to `[0, 1]`.
6. With a negative threshold, omit the angle filter.

The threshold is **local between adjacent faces**, not an accumulated angle
relative to the first face. A smoothly tessellated cylinder can therefore fill
all the way around at 30 degrees. Geometry mode still respects existing color
boundaries. No angle multiplier or offset has been introduced.

Serialization retains Orca's existing extended filament-state format and
snapshot integration. Bambu's older serializer is not substituted for it.
Networking, authentication, Bambu Connect, printer profiles, slicing, and ZAA
code are not modified by these changes.

## Automated checks

The native `tests/libslic3r/test_triangle_selector.cpp` suite covers flat and
folded faces at 0 and 30 degrees, a sharp corner, color barriers, erase,
serialized snapshot restoration/reopening, clipping, dense curved geometry,
subdivided circle/sphere strokes, and a scaled height band. State restoration
at selector level is not a substitute for GUI undo/redo or full 3MF I/O testing.

The painting Windows workflow builds the installer/portable application and
native tests, then runs the existing Windows unit-test workflow.

An additional dependency-light differential check compiles each repository's
actual bucket traversal body against an unsplit graph adapter:

```sh
git clone --depth 1 --branch v02.08.04.57 https://github.com/bambulab/BambuStudio.git ../bambu
python3 scripts/check_paint_bucket_parity.py --reference ../bambu
```

It checks identical selected triangle IDs for 216 cases, with graph sizes up to
10,000 faces. It does not validate subdivision, adjacency construction,
rendering, GUI interactions, or file I/O.

For a broader isolated source comparison (including actual recursive brush
subdivision and height bands), run:

```sh
python3 scripts/check_paint_selector_parity.py --reference ../bambu --eigen /path/to/eigen3
```

It requires the exact reference commit and real Eigen headers. See
[BAMBU_PAINT_VALIDATION.md](BAMBU_PAINT_VALIDATION.md) for results and limits.

## Side-by-side user test

1. Use Bambu Studio **2.8.4.57** and the candidate Windows build. Import the
   exact same STL/3MF in each. For STEP, export one triangulated STL first and
   import it in both, so import tessellation cannot affect the comparison.
2. Select the same P2S profile and at least two filament slots. Match object
   scale, orientation, clipping, and starting paint assignments. Disable any
   overhang-only restriction. Save an unpainted baseline.
3. Choose Fill → Edge Detection → **30 degrees**. Hover the same face and
   compare the highlighted boundary, then left-click with filament 2. Repeat
   from the baseline at **0 degrees** and around a sharp corner/curved detail.
4. Compare typing 30, dragging to 30, and Ctrl+wheel to 30. The preview and
   committed region must agree. Move the cursor away and back to rule out a
   stale highlight.
5. Paint a small barrier with Circle, then use Connected Same Color on either
   side. It must stop at the stored-color boundary, even across smooth faces.
   Ctrl+wheel must not switch this mode to geometry filtering.
6. Repeat a stroke with Circle, Sphere, Triangle, and Height Range. Include a
   uniformly scaled model and a non-uniformly scaled model. Compare narrow
   details, brush strokes, and erase (Shift+left-click). Test Erase All.
7. Move Section View with the slider or Alt+wheel, paint an exposed region,
   rotate the camera and reset the section orientation, then
   confirm the hidden side was not unintentionally selected. Test keyboard tool
   changes as well as toolbar clicks.
8. Undo/redo each operation. Save a new 3MF, close it, reopen it, and compare
   filament assignments. Slice and inspect toolpaths.
9. Send a small known-good P2S job through the existing Bambu Connect command.
   Confirm the normal cloud-connected printer stays available in Bambu Handy
   and that the existing authentication session is reused.

The user's original approximately-30-degree model must still be checked on
that exact mesh and picked face. Synthetic tests alone cannot establish that
specific reproduction or complete GUI parity. The reference has additional
filled-region outline rendering; its appearance is not claimed identical.
Physical printer submission and Bambu Handy behavior require user testing.
