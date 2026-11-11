Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.13

### Changes
- **Sound.** If the app has no linear memory left for an audio buffer for about 3 seconds, that audio frame is dropped and the video keeps playing (it used to retry for ever and the playback froze with no message).

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
