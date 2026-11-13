Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.20

### Changes
- **Saved videos.** An entry whose file was deleted by hand now says "saved file missing" instead of failing later with a network message, re-saving or deleting a saved video shows at once (its info was cached), and "Delete from the SD card" refuses to delete the video that is playing (the playback broke at the next block).
- **Languages.** The file explorer shows the Japanese "parent directory" label (it tested "jp", the code is "ja"), the Japanese text of "Every video of this channel starts ... s later" has no stray spaces, and five long French and German buttons are shorter so they fit the screen.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
