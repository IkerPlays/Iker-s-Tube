Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.18

### Changes
- **Play time.** The daily limit, the break reminder, the play statistics and the badges count only real playback: a video paused, buffering or stopped at its end no longer adds play time (a paused console could trigger "Daily limit reached"), and the limit notice cannot fire on the data of the previous day.
- **Autoplay.** Stopping the video with X + B, or choosing another video during the autoplay countdown, cancels the pending next video (it used to start anyway).
- **Seeking.** D-pad and ZL / ZR seeks stop one second before the end instead of finishing the video as watched.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
