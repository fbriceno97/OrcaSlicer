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

Implementation is in progress. No Windows build, native test suite, installation,
cloud login, Connect handoff or interactive GUI workflow has been verified yet.
The cloud/account restoration still requires a targeted port because Unlayered
deliberately removed that account UI. Source inspection is not GUI verification.
