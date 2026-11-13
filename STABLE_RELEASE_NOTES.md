Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.19

### Changes
- **Studio upload.** The saved upload address is only accepted when it is a YouTube upload address (it receives your login token) and is written through a temporary file, a finished upload whose answer was lost no longer leaves a phantom "unfinished upload", a dead session or changed file (400 / 404 / 410) fails at once and forgets the pending upload instead of retrying, and CRLF descriptions keep their line breaks without stray spaces.
- **Studio edits.** Changing the title or description of a video whose tags contain "]" (such as "top [10]") keeps all its tags (the list was cut at the first "]" and the request failed or lost tags).

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
