Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.10

### Changes
- **Playback data.** A video loads even when its page data (/next) fails twice while the player data arrives, formats without a direct address are no longer listed as qualities (they could not play), the compressed (DRC) copy of an audio track no longer replaces the normal one, the audio-language probe no longer runs on unplayable answers (it started up to 15 extra requests), and a failed dislike service keeps the like count YouTube gave.
- **Subtitles.** The format is chosen by the first character, so JSON subtitles that contain "<i>" are no longer read as XML.
- **Comments.** A stale comments token no longer blocks like, reply and delete when the video page path still works.
- **CPU limit.** After a task that raised the CPU limit finished, the app read the first item of an empty list (undefined behaviour), so the 30% base limit could be lost for the rest of the session. It now falls back to the base limit, and a restore without a matching suspend is ignored.
- **Safe mode.** If a second crash loop happens after settings were already set aside once, the new settings file is moved aside too (the old copy is kept as .crashed.old), so safe mode can no longer loop for ever.
- **Sound.** The audio buffer is flushed from the CPU cache before it is queued to the DSP (fewer clicks and repeated fragments), and freeing a texture that failed to allocate no longer crashes.
- **Playback threads.** The decode thread waits instead of spinning and logging when no packet type is available yet, the audio timestamp is stored before its buffer is queued (the clock could read the previous use's stamp right after a seek), and a second failed audio filter build is logged.
- **Live streams.** Seeking inside a live stream restarts the audio clock at the new position (the video used to run ahead of, or freeze behind, the audio after a seek).

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
