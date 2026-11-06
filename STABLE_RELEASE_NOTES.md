Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.9

### Changes
- **Updater.** Installing a .3dsx update moves the old file aside as .bak and puts it back if the new one cannot be placed, instead of deleting it first, so a failure or a power cut cannot leave the Homebrew Launcher without the app.
- **Saves.** A failed write of the play queue or of the resume positions (SD card missing or full) is written to the log.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
