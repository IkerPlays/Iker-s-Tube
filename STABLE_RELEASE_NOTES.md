Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.17

### Changes
- **Home.** Pinning or unpinning a channel from the long-press menu of Home > Channels no longer freezes the app (the menu action asked for a lock Home already held), hidden channels no longer appear in the account feed, and History sorted by views no longer repeats a day header before almost every row.
- **Decoder.** A first audio frame without a timestamp (or an unset time base) no longer gives a garbage start time that made the video run away from the audio, and the frame discard after a seek starts from safe sizes.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
