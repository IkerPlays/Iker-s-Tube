Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.16

### Changes
- **What's new dialog.** After an update the dialog is shown once: it used to come back every 1.5 seconds on Home after you closed it, until the next start.
- **Sign-in.** An answer without tokens (a captive portal) is no longer taken as a sign-in or allowed to wipe your tokens, a revoked grant (invalid_grant) clears the dead tokens instead of leaving them to fail at every launch, and your own channel is found again after a restart (the "my channel" button and own-comment detection failed until you signed out and in).
- **Notices and QR.** A notice no longer flashes a white box in the last millisecond of its fade, and the address under a QR code is cut to fit.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
