Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.12

### Changes
- **Links.** A pasted or scanned address is handed to the main thread instead of being written into the scene request from the worker thread, so it can no longer be lost or race with the frame that clears it.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
