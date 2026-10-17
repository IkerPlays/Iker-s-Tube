Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.0.1

### Your YouTube account (OAuth login needed)
- **Like or dislike a video**, **save it to your YouTube playlists**, and **subscribe or unsubscribe** on your account (the subscribe button now also updates your account).
- **Comments.** Write a comment, and long press a comment to like it, reply to it or translate it.
- **Your account in Search.** Recommended videos and your notifications inbox, and the Later tab also downloads your YouTube Watch later playlist.

### Player
- **Mini player** with big controls on the bottom screen, swipe gestures, volume remembered per channel and a left-handed mode.
- **Tools on the video page.** Jump 10 s back or forward, skip 30 s, A-B loop, tappable chapters, bookmarks with names, next chapter, quick speed (50% to 200%, remembered per channel), stop after this video and a premiere countdown.
- **Seek preview** thumbnails while dragging the progress bar, equalizer presets and sound profiles (music, movie, speech).
- **Shorts play one after another** when opened from a channel.
- **Buttons.** Choose what X, Y, L and R do in the player.
- **Starts at the beginning.** A new video no longer starts at the minute of the previous one. A Settings option makes every video start from the beginning.
- **Screensaver.** The logo bounces on a black top screen after a minute of pause.

### Studio
- **Studio tab** next to Home, Subscribed and New: your subscribers, total views and total videos, and upload a video or a Short from any folder of the SD card with title, description and visibility. Needs the OAuth login.

### Library and search
- **Library.** Queue, favorites with tags, playlists, Watch later and the new Offline tab. Sort and filter favorites and Watch later, play next from any list and a check mark on videos you already watched.
- **Downloads screen.** Items saved, space used and free on the SD card, progress of the running download and a way to cancel it.
- **Offline videos and audio.** Save a video (360p, 240p on the old 3DS) or only its audio to the SD card and play it without a connection from the Offline list (Downloads in the bottom bar).
- **Search.** Long press a recent search to forget it, keyboard suggestions, Explore and Discover shortcuts, continue watching the last video, QR scanning with the camera and a red dot when your subscriptions have new videos.
- **Grid of thumbnails** in search, channels and the home feed (optional).
- **Translate** a video title and description, or a comment, into the language of the app.
- **QR codes** of any video (including the current moment), also from the queue, favorites and playlists.

### Look and settings
- **Bottom bar and pill tabs.** A navigation bar with icons (Home, Search, Library, Downloads, More) and rounded pill tabs.
- **Themes.** Six themes plus a custom color, a high contrast mode, text size, large thumbnails and an optional background picture from the SD card.
- **Settings in sections** with a start screen, automatic theme by time of day, data saver, a data limit warning, statistics, backup export and import, and the first launch opens the settings.
- **Languages.** All the new texts are available in English, Spanish, French, German, Italian, Portuguese and Japanese.

### Reliability
- **Queue and backups.** The play queue survives a restart, your data is backed up on the SD card once a week, and the app tells you when the last run ended in a crash.
- **Fixes.** Videos over an hour with a single combined stream play again, the A-B loop no longer freezes, comment sorting no longer crashes, and many cases that could close the app were fixed after a full code review.
- **Network.** Failed host name lookups right after the Wi-Fi connects are retried and the app asks the console to reconnect if the Wi-Fi drops during playback.
- **Diagnostics.** Warnings and errors are saved to sdmc:/3ds/IkersTube/log.txt, and an unexpected abort writes sdmc:/3ds/IkersTube/crash.txt.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
