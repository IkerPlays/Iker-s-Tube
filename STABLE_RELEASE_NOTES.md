Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.0.6

### Fixes (important)
- **Crash at startup.** Some consoles crashed every time the app opened after the update to 1.0.6. The background fetch of the keyboard suggestions no longer uses its own thread or network sessions (it runs in the async worker, once Search is opened), which removes a race that could crash the app while the home feed was loading.
- **Safe mode.** If a start does not finish (a crash), the next start renames the settings and the optional files to *.crashed (nothing is deleted, existing copies are never overwritten), skips the dialogs and the background extras and says so with a notice. The start counts as good after 30 seconds of running or on a normal exit.
- If 1.0.6 crashes on your console and the app cannot update itself: delete or rename 3ds/IkersTube/settings.txt on the SD card, or install this version with FBI.

### Changes
- **Rounded buttons.** Buttons have rounded corners (a setting turns it off).
- **My Wrapped.** A summary of what you watch: top channel, favorite hour, streak and more.
- **Sleep timer fade.** The volume goes down smoothly during the last 30 seconds of the sleep timer.
- **Next episode.** New player tool finds the next episode of a series by number.
- **Radio.** Eight mood mixes (Chill, Focus, Workout, Sleep...) that keep playing by themselves.
- **Notices.** The notices are rounded, fit their text, slide in and fade out.
- **New videos notice.** Once per session the app tells how many new videos your channels have.
- **Bookmarks.** Long press a bookmark for its QR code or to delete it.
- **Data saver tool.** Turn the data saver on or off from the player.
- **Greeting.** The start page of Search greets you by the time of the day.
- **Rounded badges.** The badges on the thumbnails are rounded.
- **Daily limit.** A setting warns when you reach 30, 60, 120 or 240 minutes of playing in a day.
- **Rounded selectors.** The selectors have a soft track and a rounded pill.
- **Safe mode.** If the app crashes while starting, the next start sets the settings aside and starts without extras instead of crashing again.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
