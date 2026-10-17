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

- **Do not install this development build over your existing stable installation until tested.**
- The action waits for the current plate's slice to be ready; if the plate changes or 180 seconds pass without a ready slice, it cancels rather than uploading a stale file.
- This is not fully unattended one-click printing. Bambu Connect may require confirmations and mapping selections.
- This does not spoof Bambu Studio's network identity, replace any networking plugin, change printer firmware, or require LAN Only Mode.
- Temporary sliced files are currently retained in the OS temporary directory. Automated clean-up and full print handoff are follow-up tasks.
- A Windows build and printer tests on A1/P2S are required; source-only commits are not proof that the application works.

## Origin

The export URL and .gcode.3mf transfer method build upon upstream OrcaSlicer proposal [#13811](https://github.com/OrcaSlicer/OrcaSlicer/pull/13811) (unmerged as of the development work). The slice-on-demand flow and print-menu entry are implemented in this fork.

## Testing checklist

- [ ] Windows x64 build succeeds
- [ ] New menu item appears only for Bambu Lab printer profiles
- [ ] Unsliced A1 model automatically slices and opens Bambu Connect
- [ ] Already sliced A1 model opens Bambu Connect without unnecessary re-slicing
- [ ] Changing plates during slicing cancels handoff
- [ ] Bambu Connect recognizes imported `.gcode.3mf` and can start a print
- [ ] P2S import and print succeeds
- [ ] Bambu Handy still monitors both devices through cloud mode
