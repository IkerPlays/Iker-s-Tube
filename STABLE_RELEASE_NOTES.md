Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.3

### Changes
- **Channel page.** It keeps only the 8 most recent channels in memory instead of every channel opened, which saves memory on long sessions.
- **Account actions.** When Like, Subscribe, Comment or a playlist change fails on your YouTube account, the notice gives a short reason (sign in again, too many requests, YouTube busy, check the connection) instead of raw technical text.
- **Library.** The long-press menu shows "Remove from queue" for a video that is already queued, like the other menus do, instead of an "Add to queue" that silently removed it.
- **Favorites and Watch later.** "Remove the watched ones" (both lists) and "Clear Watch later" save the list once instead of once per video, so they no longer freeze on a long list.
- **Comments.** Reply checks that the comments are loaded before it opens the keyboard, so a reply you typed is not lost to an "Open the comments first" failure.
- **Library.** The confirm buttons of "Clear all favorites" and "Clear Watch later" show how many videos will go (the whole list is cleared even when a filter is on).
- **Channel page.** Tapping the error line of a channel no longer freezes the console, a failed Streams or Shorts tab can be retried with a tap, B on the QR popup only closes it, and a community post image without a size no longer crashes.
- **Settings.** B on a popup (install, remove history, sign-in code) only closes the popup instead of also leaving Settings.
- **Saving.** A save that cannot be flushed to the SD card (full card) is reported as failed, so the previous copy of history, subscriptions or settings is kept instead of being replaced by a cut file.
- **History.** It keeps the latest 3000 videos, and the "hide watched" marks no longer miss a video you just watched.
- **Text.** Titles and chat messages with emoji, escaped quotes or accents are decoded correctly in Watch later pulls, Studio answers, live chat and collaborations.
- **Video info.** The dislike count no longer repeats the like count.
- **Studio login.** The refresh token and the client are saved through a temporary file, so a full card or a power loss while signing in no longer signs you out.
- **Live chat.** Closing and reopening the chat of the same video no longer restarts a healthy chat because an old thread ended late.
- **Small fixes.** Counts near a million or a billion print as 1.0M / 1.0B instead of 1000.0k, a pasted playlist link with strange characters no longer breaks the request, and History > Shuffle gives a different order after each start.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
