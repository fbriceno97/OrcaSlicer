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

## Feature provenance

The Beta 5 custom commit range was compared against its pre-custom ancestor
`1ee0b730a1`. The pinned Beta 5 tree, not a moving branch, supplied the port.

| Feature | Source and treatment |
| --- | --- |
| Painting controls, world-space height bands, subdivision, overlapping-shell picking | Beta 5 commits `57f92c3676`, `350078a0c9`, `051d30f468`, `0723e597e6`, `11fec226a9`; ported with native Unlayered tilt/highlight behavior retained. |
| Bambu Connect slice-to-import and cancellation | Beta 5 commits `be2df00a45` through `3788f7c8c8`; adapted to native Unlayered print-button dispatch. |
| Orca/Bambu accounts and cloud pane | Restored from the pinned Beta 5 adapter, account markup, provider commands and endpoint configuration; retained newer base session helpers and native device-tab behavior. |
| Branding, installer identity, executable metadata and associations | Beta 5 identity fixes including `605c252ec5`, `d49b135f8a`, `e5a6dd33de`, `634f6c9a90`; adapted to a third, separate identity. |
| Z anti-aliasing | Already implemented by the approved Unlayered base; retained, including its native fill regression. |
| Segments, build directions, nonplanar slicing, preview, G-code and collision checks | Approved Unlayered v0.1.9 implementation retained. Core implementation files compare byte-for-byte with that source base. |
| Settings import | Native additive Other Slicer Import retained; the real importer regression additionally verifies source-file byte preservation. |

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

The complete Windows x64 workflow succeeded at
`c8883d7da1ae9f401dc1b3f7aedaadb0e7335e78`:
https://github.com/fbriceno97/OrcaSlicer/actions/runs/41347476594.
Compilation, the exception checker's nine regression cases and actual core-PDB
inspection, native tests, preset-cache generation, application CLI smoke checks,
installer creation and synthetic install/uninstall coexistence checks passed.
All software gates run before application artifact uploads. MSIX packaging is
opt-in and disabled for these custom installer/portable builds.

Native results: 3,286 cases; 3,284 passed, zero failed, two skipped. The standard
suite excludes NotWorking and RequiresApp. All ten re-enabled cloud mock/loopback
cases are included. The legacy filename-protection failure is fixed, and raft
centroid coverage explicitly separates isolated from overlapping rafts without
changing native support generation or G-code.

### Windows x64 test artifacts

These are Actions test artifacts, not a published installer release. Download the
installer artifact and extract its .exe, or extract the portable artifact to its
own folder and run unlayered-slicer.exe. They were built from the tested commit
above; later documentation-only commits do not change their application source.

| Artifact | Download | Actions archive SHA256 |
| --- | --- | --- |
| Installer | [Actions artifact](https://github.com/fbriceno97/OrcaSlicer/actions/runs/41347476594/artifacts/13100452556) | `eae27ae14400623fd05daec2c48d203cebea61eac90622ad4d467dd777a579cb` |
| Portable | [Actions artifact](https://github.com/fbriceno97/OrcaSlicer/actions/runs/41347476594/artifacts/13100397955) | `90fb93e0576304ff93d6861143646554bd11e11c5f1d56e8816c6c6ad1b8a81d` |
| Native JUnit report | [Actions artifact](https://github.com/fbriceno97/OrcaSlicer/actions/runs/41347476594/artifacts/13100073476) | `287aec6ed6c82759f25d363f94772eeef0770a25b8338bdfcf76f816f0adec00` |

Digests identify the Actions ZIP archives, not the installer executable inside.


The synthetic installer test creates disposable Beta 5 install/profile/registry
sentinels, checks product identity and a separate custom uninstall entry, then
uninstalls and checks that the custom executable/entry/shortcut are removed while
Beta 5 sentinels survive. It found an orphaned custom desktop shortcut, fixed by
matching unconditional cleanup. It does not verify a real existing Beta 5 install.

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
stored credentials are not read or changed. On the Home page, Orca Cloud's Login
control appears by default outside Stealth Mode. In Preferences, enable **Enable
Bambu Cloud** to show the Bambu account row; install the optional Bambu network
plug-in to use that account. Sign in again in this application: importing presets
does not transfer the other application's credentials. Cloud authentication and
synchronization still require runtime verification. Source inspection is not GUI
verification.

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
