Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.21

### Changes
- **Text.** Emoji in comments, posts and descriptions are no longer dropped (a run that is only an emoji came out blank), and a very long run of digits followed by a colon in a description is not taken as a timestamp.
- **Links.** Addresses with capital letters in the scheme or host ("HTTPS://WWW.YouTube.com/...") are recognised as YouTube addresses, and a "://" inside the query string of an address without a scheme is no longer taken as its scheme.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
