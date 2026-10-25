Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.2

### Changes
- **Dialogs and menus.** A dialog or menu swallows every button while it is open (B that dismisses "Exit?" no longer also leaves the screen, the d-pad no longer seeks under the video menu), a tap outside the More menu only closes it, the More menu always fits on the screen, and a second notice waits until the first has been readable for a second.
- **Channel menu.** Subscribing or unsubscribing from the long-press channel menu is saved right away.
- **New videos badge.** The feed no longer marks everything as new after a first run with an empty feed, and its list is saved safely.
- **Badges and My Wrapped.** Night owl and Early bird need at least 20 videos and a real favorite hour, and the streaks in My Wrapped count local days like the badges do.
- **Privacy and saves.** QR links are not written to the shared links file in incognito, and the last played video is saved through a temporary file.
- **Trailer.** The website has the new trailer in the Look section, and tools/make_trailer.py builds it (English and Spanish).

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
