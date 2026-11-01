# Unlayered Custom Beta

The base is the official Unlayered Slicer v0.1.9 source commit
89e3033bf81cd892a8f2af819634aa2aa235e1cc. The requested binary build identifier
3d20b5957 cannot be resolved in the public source repository; no equivalence is claimed.
The user approved the official tag as the source base.

Custom feature source: release/custom-beta5 at
426e50e8c190857633fe3be3b73fdc54fd9270ef. The retired experimental branch is
not a source for this application.

Beta 5 contributes the Bambu Studio painting controls and mesh picking/subdivision
fixes, their existing native tests and parity tools, and Bambu Connect slice-to-import
handoff. Native Unlayered segment authoring, build directions, nonplanar slicing,
preview, collision checks and G-code export remain the base implementation.
Z anti-aliasing is already present in this base and must be regression-tested
together with native nonplanar output.

The application is identified as Unlayered Custom Beta. Its settings key is
UnlayeredCustomBeta, distinct from Beta 5's OrcaSlicer key and stock Unlayered.
Its installer registry key, install directory, shortcuts, file ProgID, URL protocol
and Windows IPC names are separate. File registration adds an Open with candidate
and does not replace another application's default. Stock upstream automatic
installer execution is disabled: test builds must not install stock Unlayered updates.

Use File > Import > Import Settings from Other Slicers and choose OrcaSlicer to
copy Beta 5 presets. This uses upstream's additive importer; it does not delete
source settings and does not copy credentials. Keep the source application closed
during import. Existing target presets are preserved by the upstream importer.

Upstream LICENSE.txt and attribution are retained. This is a derivative of
Unlayered Slicer, OrcaSlicer, Bambu Studio and PrusaSlicer; it is not an official
Unlayered or Bambu product.

The Windows x64 workflow runs software checks before uploading portable and installer
test artifacts. It has no release publication step. Passing software checks does
not validate physical clearance or printing on Bambu A1/P2S.

## Validation status

Implementation is in progress. Windows x64 compilation, the native regression suite,
preset-cache generation and application CLI smoke checks passed at
`d0697368a5ee0aa7b4f9e2d8249d3e83c91876f9` in
https://github.com/fbriceno97/OrcaSlicer/actions/runs/41341708733.
Native results: 3,286 cases; 3,284 passed, zero failed, two skipped. The standard
suite excludes NotWorking and RequiresApp. All ten re-enabled cloud mock/loopback
cases are included. The legacy filename-protection failure is fixed, and raft
centroid coverage now explicitly separates isolated from overlapping rafts without
changing native support generation or G-code.

Installer creation passed, but synthetic install/uninstall coexistence verification
failed. Application artifact uploads correctly stayed blocked. Diagnostic rerun:
https://github.com/fbriceno97/OrcaSlicer/actions/runs/41343276089.
Live cloud login/synchronization, Connect handoff and interactive Windows segment
selection, direction assignment, preview and export remain unverified. No Windows
desktop session is available in this environment. Physical clearance/printing checks
are separate and have not been performed.

The real homepage DOM passes automated Chromium tests for Orca/Bambu login
commands, account menus, plugin gating, cloud navigation and hidden-panel state.
Local checks also passed with CSS light and dark color schemes.
The native app bridge and account responses are mocked; this is not live sign-in
or interactive Windows GUI verification.
Beta 5's Bambu account adapter, login controls and privacy-policy dialog are restored;
Orca Cloud transport is re-enabled with its Beta 5 endpoints and public client key.
The credential-store service is UnlayeredCustomBeta/Auth, so existing applications'
stored credentials are not read or changed. Cloud authentication and synchronization
still require runtime verification. Source inspection is not GUI verification.

Local isolated bucket traversal matches pinned Beta 5 in all 216 synthetic cases.
This check does not exercise native mesh adjacency/subdivision, painting controls,
undo, project round-tripping, preview, cloud access or physical printing.

The extracted selector fixture comparison also matches pinned Beta 5: split brushes,
scaled height bands, dense sphere and large-brush geometry/state results. Output
SHA256: 8672972f299eaab52cc915e8929b8d52f89275124459b5a78fbe31b7f7080eef.
It uses Eigen 5.0.1 verified against the base dependency's SHA256. This is an
isolated dependency facade, not the native Catch2 suite or interactive GUI.

New draft PR: https://github.com/fbriceno97/OrcaSlicer/pull/3.

## Windows interactive acceptance procedure (not yet executed)

1. Keep Beta 5 installed, back up its settings, and install the custom candidate.
   Confirm separate installation and Apps & Features entries. Start both applications
   and confirm their profile changes do not affect each other.
2. In the custom app, import OrcaSlicer presets through Import Settings from Other
   Slicers. Verify original files remain unchanged, imported values match, and
   importing again preserves existing target presets without duplication.
3. Load a model, enable non-planar printing for a disposable test printer preset,
   select Virtual for a 3-axis configuration, and open Cut Planes. Add a cut plane,
   enable Color cut chunks, select each resulting segment and assign distinct
   Build direction values. Accept By group sequencing when prompted.
4. Check segment highlighting and direction changes in the viewport, including
   undo/redo. Save and reopen the project and confirm planes, directions and
   painting are retained.
5. Slice, open Preview, traverse the segments/layers and check visible toolpath
   directions against the authored values. Inspect the native collision warnings.
   Export G-code and a sliced 3MF and verify both are nonempty and reload correctly.
6. Exercise connected-color bucket fill, angle-limited fill, wheel changes,
   camera-oriented Section View and scaled height-band painting, including
   overlapping shells. Check undo/redo and save/reload of the painted project.
7. With an authorized account, exercise Orca/Bambu login, preset synchronization
   and the cloud device workflow. Test the Bambu Connect action on an unsliced
   plate and an already sliced plate, cancel during slicing and change plates
   while waiting. Confirm successful handoff reaches Connect's review screen.
   This procedure stops before submitting a print.
8. Uninstall only the custom candidate. Confirm Beta 5 still starts and its
   installation, profiles, associations and stored login remain intact.

Passing these interactive checks is separate from the synthetic installer test,
extracted selector checks and native Catch2 tests. No physical A1/P2S clearance or
printing validation is included.
