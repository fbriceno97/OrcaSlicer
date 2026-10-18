# OrcaSlicer → Bambu Connect: Slice & Send (experimental)

This fork adds **Slice & send via Bambu Connect** to the print-action dropdown for Bambu Lab printer profiles.

## Requirements

- Install Bambu Connect on the same computer and sign in to the account that owns the printer.
- Bambu Connect must be registered as the `bambu-connect://` URL protocol handler.
- Keep Bambu printers in **normal cloud mode**, not LAN Only Mode, if Bambu Handy remote monitoring is desired.
- This feature only initiates **import into Bambu Connect**. You must still review the destination printer, filament/AMS choices, and confirm print submission within Bambu Connect.

## How to use

1. Select a Bambu Lab printer profile (e.g. A1 or P2S).
2. Open the dropdown next to **Print plate**.
3. Select **Slice & send via Bambu Connect**.
4. Click the main print-action button.
5. If the current plate is not sliced, OrcaSlicer will slice it automatically. If a valid slice already exists, it will reuse it.
6. OrcaSlicer writes a unique temporary `.gcode.3mf` and opens `bambu-connect://import-file?...` using the system URL handler.
7. Inspect and start the print inside Bambu Connect; continue using Bambu Handy normally.

## Safety and limitations

- **Do not use the old v0.1.0-beta1 NSIS installer alongside official OrcaSlicer.** It uses the upstream identity and prompts to uninstall the existing application. Cancel that prompt. A separately identified replacement installer is being developed and is not yet verified.
- The action waits for the current plate's slice to be ready; if the plate changes or 180 seconds pass without a ready slice, it cancels rather than uploading a stale file.
- This is not fully unattended one-click printing. Bambu Connect may require confirmations and mapping selections.
- This does not spoof Bambu Studio's network identity, replace any networking plugin, change printer firmware, or require LAN Only Mode.
- Temporary sliced files are currently retained in the OS temporary directory. Automated clean-up and full print handoff are follow-up tasks.
- Windows x64 portable launch, slicing, and Bambu Connect import were manually tested. Physical printing and continued Bambu Handy/cloud monitoring have not yet been confirmed.

## Origin

The export URL and .gcode.3mf transfer method build upon upstream OrcaSlicer proposal [#13811](https://github.com/OrcaSlicer/OrcaSlicer/pull/13811) (unmerged as of the development work). The slice-on-demand flow and print-menu entry are implemented in this fork.

## Windows x64 prerelease status

The tested build comes from commit `3788f7c8c8e4af162fc7842910933195c00c3a26`, GitHub Actions run `39080439005`. The user confirmed that the Windows x64 portable app launches, slices, hands off to Bambu Connect, and reaches the final confirmation screen one click before printing. **No successful physical print is claimed.**

For the next release, package only Windows x64 binaries. The previous beta 1 installer is unsuitable for side-by-side installation. Verify the new NSIS install/uninstall behavior before releasing. The recommended install path is the portable zip, extracted alongside rather than over an existing OrcaSlicer installation. Existing stable OrcaSlicer user profiles/settings may still be shared; preserve backups where appropriate.

## Testing checklist

- [x] Windows x64 build artifacts produced (GitHub Actions run 39080439005)
- [ ] Verify menu item is limited to Bambu Lab profiles
- [ ] Unsliced A1 model automatically slices and opens Bambu Connect
- [ ] Already sliced A1 model opens Bambu Connect without unnecessary re-slicing
- [ ] Changing plates during slicing cancels handoff
- [ ] Bambu Connect recognizes imported `.gcode.3mf` and can start a print
- [x] User confirmed Bambu Connect opened with a sliced file and reached the final print confirmation (printer profile not independently verified)
- [ ] Complete actual physical print from Bambu Connect on P2S
- [ ] Bambu Handy still monitors both devices through cloud mode

## Side-by-side installer acceptance tests (not yet complete)

- [ ] Existing official OrcaSlicer remains installed and usable
- [ ] Fork NSIS installer does NOT request uninstall of stable OrcaSlicer
- [ ] Fork installs into its own Program Files directory and has a separate Apps & Features entry
- [ ] Fork Start Menu shortcuts do not replace official OrcaSlicer shortcuts
- [ ] Fork and official OrcaSlicer settings/data directories do not conflict, or shared configuration is explicitly documented and tested
- [ ] Uninstalling the fork leaves official OrcaSlicer untouched
- [ ] Bambu Connect handoff still works with a build from the new installer
