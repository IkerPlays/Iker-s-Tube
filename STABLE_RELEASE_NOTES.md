Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.11

### Changes
- **Import.** Channels imported from a Takeout CSV stay in your subscriptions after a restart (they were saved without a picture and then hidden as invalid), and a quoted name with "" inside keeps a single quote.
- **Links.** A shared channel link with "?si=..." opens the channel, a pasted address without "https://" (www.youtube.com/@name) opens instead of failing, and %00 in a results link no longer cuts the search.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
