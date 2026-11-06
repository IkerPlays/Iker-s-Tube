Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.8

### Changes
- **Load more.** An error answer (400 / 403) to Load more on a channel's Videos, Streams or Shorts, or on the comments, keeps the continue token, so the list no longer stops silently and asking again works.
- **Downloads.** If the last step of a multi-part download fails (SD card glitch), the parts already in place are removed instead of staying as orphan files.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
