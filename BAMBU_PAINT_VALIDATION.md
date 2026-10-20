# Painting candidate validation

Implementation commit: `0723e597e6d4637403f127d033da1686bd067ed8`.
Reference: Bambu Studio `v02.08.04.57`, commit
`f977235e6d736c4c0b650520ac5a5b72cbfe9244`.

[Windows build and native tests](https://github.com/fbriceno97/OrcaSlicer/actions/runs/39347165531)

## Downloads

- [Windows installer ZIP](https://github.com/fbriceno97/OrcaSlicer/actions/runs/39347165531/artifacts/12233342453)
- [Windows portable ZIP](https://github.com/fbriceno97/OrcaSlicer/actions/runs/39347165531/artifacts/12233147799)

The installer archive contains
`OrcaSlicer_BambuConnect_Windows_Installer_V2.5.0-dev_x64.exe`.
Both artifacts identify the implementation commit above in GitHub metadata.

## Results

| Check | Result | Evidence / limit |
|---|---|---|
| Windows x64 compilation | PASS | Build job `118382509784` |
| Installer creation | PASS | NSIS packaging and uploaded installer artifact |
| Native Windows unit tests | PASS | 2,320 tests passed; zero failed; job `118396013154`; 235.10 seconds |
| Installer archive integrity | PASS | ZIP CRC check; GitHub digest matches downloaded bytes |
| Portable archive integrity | PASS | ZIP CRC check; 4,045 entries; digest matches downloaded bytes |
| Application architecture | PASS | `orca-slicer.exe` and `OrcaSlicer.dll` PE machine `0x8664` (AMD64) |
| Unsplit differential traversal | PASS | 216 cases; identical selected triangle IDs |
| Subdivision differential comparison | PASS | Identical triangle geometry and states for the controlled cases below |
| Address/undefined behavior sanitizers | PASS, isolated checks | Core selector harness; leak detection unavailable under the sandbox, disabled |
| Original user model at approximately 30 degrees | UNVERIFIED | Exact mesh/picked face must be compared in both GUIs |
| GUI undo/redo and complete 3MF round trip | UNVERIFIED | Core serialized-state restoration passed; GUI/project integration needs manual testing |
| P2S print submission / Bambu Handy | UNVERIFIED physically | Bambu Connect, authentication, profiles and networking implementation unchanged |

All seven added painting cases pass in the production Windows suite: local
0/30-degree geometry thresholds, painted-color barriers, connected fill/erase
and state restoration, clipping, dense curved geometry at 30 degrees,
subdivided Circle/Sphere color boundaries, and scaled height bands.

[Test-result XML artifact](https://github.com/fbriceno97/OrcaSlicer/actions/runs/39347165531/artifacts/12233656449)

### Differential tests

`python3 scripts/check_paint_bucket_parity.py --reference ../bambu`
compiles each checkout's actual bucket traversal body against an unsplit graph
adapter. Cases cover 0/30-degree thresholds, disabled geometry filtering,
0/5/20/30/40/90-degree neighbor turns, colors, clipping, and graph sizes of
3, 100, and 10,000 faces. All 216 selected triangle sets match exactly.

`python3 scripts/check_paint_selector_parity.py --reference ../bambu --eigen /path/to/eigen3`
compiles the actual selector painting/subdivision methods with real Eigen and a
small dependency facade. It checks source operations beyond the unsplit graph:

- Circle and Sphere: 69 painted leaves each for the small-stroke fixture.
- Connected fill after brush subdivision: respects the painted component;
  restoration of the serialized painting also passes.
- Uniformly scaled height band: 256 painted leaves in both implementations.
- Dense sphere at 30 degrees: all 3,968 faces selected in both implementations.
- Larger Circle/Sphere brushes exercising the 0.2 mm subdivision cap: 192 painted
  faces each in both implementations.

The sorted painted triangle coordinates and states match exactly in a
5,953-line output. SHA-256:
`8672972f299eaab52cc915e8929b8d52f89275124459b5a78fbe31b7f7080eef`.

This facade replaces mesh storage, adjacency/normal construction, logging, and
TBB scheduling; it is not the full application/native test suite. It excludes
GUI rendering, production neighbor construction, concurrency and printer I/O.
The native tests separately exercise production mesh/selector integration.

The isolated height-band check fails on the original `8333c3c` selector (no
painted faces) and passes on the reference and patched selectors. The
non-uniform scale camera position is transformed once in the adapted code,
where the reference constructor would transform it twice.

### Archive SHA-256

| Artifact | SHA-256 |
|---|---|
| Installer ZIP | `deca74d28f0a1dbd26dfbaa8e11fe49e2c90005d100de2bd2593aaa56ff2feb8` |
| Portable ZIP | `660ac02124c9160c0e58ff3d51a4489da093d35e394c13c3b6f98701335c32cb` |

The NSIS installer bootstrap is x86, as expected for NSIS; the installed
application payload and portable application are x64. These were inspected as
PE binaries, not launched in a Windows GUI during this session.

## Scope and remaining validation

The existing neighbor-angle kernel and same-color graph traversal already
match the Bambu reference. No angle scaling/offset was introduced. The changes
fix the wrong wheel-selection call path, preserve the same-color mode sentinel,
port reference brush subdivision and world-space height-band behavior, restore
the reference remembered threshold, and adapt camera-oriented section controls
and selection-cache invalidation.

Undo/redo and paint serialization retain Orca's existing integration and extended
filament format. Bambu's filled-region outline renderer is not transplanted;
Orca's highlight/contour appearance remains different. Irregular/non-manifold
meshes, the original user model, GUI interaction, complete project I/O, and
physical printer operation are not certified by the isolated comparisons.

See [comparison and regression procedure](BAMBU_PAINT_TESTING.md) for the exact
side-by-side test steps. This report does not certify complete GUI or
physical-printer parity.
