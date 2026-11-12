Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.15

### Changes
- **Streaming.** A stream whose block download fails waits for its retry without stopping the downloader thread (a failing video stream used to freeze the audio stream for up to 12 seconds), a response without a usable Content-Range length marks the stream as failed instead of ready with a wrong size, a redirect URL whose first parameter is the removed one keeps its "?", and the download percentage never goes over 100.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
