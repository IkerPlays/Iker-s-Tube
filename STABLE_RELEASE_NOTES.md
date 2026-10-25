Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.3

### Changes
- **Channel page.** It keeps only the 8 most recent channels in memory instead of every channel opened, which saves memory on long sessions.
- **Account actions.** When Like, Subscribe, Comment or a playlist change fails on your YouTube account, the notice gives a short reason (sign in again, too many requests, YouTube busy, check the connection) instead of raw technical text.
- **Library.** The long-press menu shows "Remove from queue" for a video that is already queued, like the other menus do, instead of an "Add to queue" that silently removed it.
- **Favorites and Watch later.** "Remove the watched ones" (both lists) and "Clear Watch later" save the list once instead of once per video, so they no longer freeze on a long list.
- **Comments.** Reply checks that the comments are loaded before it opens the keyboard, so a reply you typed is not lost to an "Open the comments first" failure.
- **Library.** The confirm buttons of "Clear all favorites" and "Clear Watch later" show how many videos will go (the whole list is cleared even when a filter is on).

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
