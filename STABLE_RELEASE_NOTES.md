Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.10

### Changes
- **Playback data.** A video loads even when its page data (/next) fails twice while the player data arrives, formats without a direct address are no longer listed as qualities (they could not play), the compressed (DRC) copy of an audio track no longer replaces the normal one, the audio-language probe no longer runs on unplayable answers (it started up to 15 extra requests), and a failed dislike service keeps the like count YouTube gave.
- **Subtitles.** The format is chosen by the first character, so JSON subtitles that contain "<i>" are no longer read as XML.
- **Comments.** A stale comments token no longer blocks like, reply and delete when the video page path still works.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
