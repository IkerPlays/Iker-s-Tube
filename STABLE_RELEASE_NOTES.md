Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.0.2

### Offline
- **Offline videos.** The Save video button saves a video on the SD card (360p, 240p on the old 3DS) next to the old Save audio. Saved videos play without any connection from the Offline list.
- **Save offline from lists.** Long press a video in Favorites, Watch later, a playlist or the queue to save its video or audio. The app loads it in the background.
- **Download queue.** Several downloads can be queued. The Downloads screen shows the progress, how many are waiting and lets you cancel.
- **Downloads screen.** Items saved, space used and free on the SD card, and Play all / Shuffle for everything you saved.
- **Clean up.** An optional setting deletes a download after you watch it, and a button deletes all downloads (with confirmation).
- **Download indicator.** A green dot on Downloads while something downloads.
- **No connection strip.** A red strip says when the console has no internet and reminds that saved downloads still play.

### New look
- **Bottom navigation bar** with icons (Home, Search, Library, Downloads, More) on the main screens.
- **Pill tabs** in the accent color and rounded buttons.
- **Start screen.** The Homebrew mark and a 3 second loading screen with a progress bar give the account and data time to load.

### Studio
- **Statistics kept.** The last numbers are saved and shown when they cannot be loaded.
- **Statistics fixed.** Subscribers, total views and total videos come from the public About page of your channel, so they load without the Data API.
- **Own Studio login.** Uploading and editing need the YouTube Data API, which the main login cannot use. Studio has its own sign-in with a Google Cloud client of yours (see Documentation/STUDIO_SETUP.md).
- **Upload with more options.** Category and custom thumbnail, a button to watch the uploaded video, and uploads run in their own network session.
- **Edit my uploads.** Change visibility, title, delete, or watch your latest uploads (without the Studio login it lists your public videos). Create playlists and open your channel.

### Comments and player
- **Collaboration videos.** Tap the author of a video with several channels to see all of them (icon, handle, subscribers), open any, or long press one to subscribe.
- **Comments.** Sending, replying and liking try several YouTube clients, and a failure now shows the reason on screen. Pinned and creator-hearted comments are marked.
- **Chapters** show as ticks on the buffering bar, and the last subtitle language is preselected.

### Library and reliability
- **Queue and backups.** The play queue survives a restart, your data is backed up on the SD card once a week, and the app tells you when the last run ended in a crash.
- **Search.** Long press a recent search to forget it.
- **Fixes from a code review.** Failed download threads no longer block later downloads, popups no longer leak on every visit, a failed read of an offline file no longer breaks playback, and uploads no longer clash with other network requests.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
