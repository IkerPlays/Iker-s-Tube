Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.2.1

### Changes
- **Video quality fixed.** If the wanted quality does not exist the closest one is used (the New 3DS used to fall to 144p), the muxed 360p no longer replaces the real 360p stream, only qualities that can play are listed (the old 3DS family only the low ones), a quality picked by hand is not lowered by the data saver for that video, and a quick second change keeps the position.
- **Quality shortcuts.** A Quality tool in the General tab of the player cycles the quality with one tap, Settings has a Video quality row, and the quality row of the player is taller.
- **Audio tracks.** The audio language search asks YouTube like the video page does and says plainly when YouTube did not answer.
- **Touch fixes.** Selector rows react on every button, sliders keep their drag instead of scrolling the page, a finger that slides out of a list cancels the press, the info icons and tab bars react where they are drawn, and a tap that moves a few pixels is not taken as a scroll.
- **Back button.** B closes a popup in Library and History without also leaving the screen.
- **Tidier labels.** Tabs, selector buttons and the bottom bar use a smaller font when a label is long (Spanish, German, ...), and the settings buttons are full-width cards like the rest.
- **Translations.** 80 more texts in French, German, Italian, Portuguese and Japanese.
- **Data safety.** The history keeps its durations after a restart, full-card writes no longer replace good lists (watch later, bookmarks, resume points, searches), the backup keeps the previous copy as *.prev, empty setting values use their default, and safe mode needs two unfinished starts in a row.
- **Playback.** A stream that dies long before the end no longer counts as watched (the resume point and watch later stay), pausing while a video is still opening no longer desyncs, every new video has its own retries, a failed decoder start can no longer crash, and a lock at the live edge of live streams is balanced.
- **Home, channels and downloads.** Home retry keeps its place and reloads the first page, the account feed shows its errors and a tap tries again, a channel line with an error retries when tapped, a feed refresh no longer re-subscribes channels you just left, cancelling downloads also drops the queued saves, and a full SD card is noticed when a download ends.
- **Thumbnails.** Busy-server answers are retried and bad cached pictures are dropped.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
