Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since the last stable release (1.0.1)

- **Backup and statistics.** Export and import your data (favorites, watch later, playlists, bookmarks, history, subscriptions, settings) and see your total playing time and most watched channel.
- **Data saver.** 240p videos without thumbnails, plus a per-session data limit warning.
- **Grid of thumbnails everywhere.** Search, channel pages and the home feed.
- **Watch later sync (download).** The Later tab also pulls your YouTube Watch later playlist when you are signed in.
- **Personal controls.** Choose what X and Y do in the player, left-handed mini player, speed remembered per channel, skip 30 s and stop after this video.
- **Custom idle background** from a picture on the SD card.
- **Long videos fixed.** Videos over an hour that only have a combined audio and video stream now play instead of failing with "No valid stream url extracted".
- **Network resilience and diagnostics.** Retries when the host name cannot be resolved right after the Wi-Fi connects, and clearer details in the log when a video cannot be played.
- **Starts at the beginning.** Fixed a bug where a new video could start at the minute of the previous video, and added an option to always start from the beginning.
- **Account actions.** Like and dislike, save to your YouTube playlists, subscribe on your account, write, like and reply to comments (OAuth login needed).
- **Translate** titles, descriptions and comments into the language of the app.
- **Offline audio.** Save audio to the SD card and play it without a connection from the Offline tab.
- **Queue and shorts.** Play next from library lists and shorts that play one after another.
- **Premiere countdown** for upcoming videos.
- **Crash report file.** An unexpected abort now leaves sdmc:/3ds/IkersTube/crash.txt on the SD card so it can be diagnosed.
- **Stability fixes.** A freeze with the A-B loop, a crash risk with bookmarks and several unchecked cases that could close the app.
- **Continue watching and first launch.** Resume the last video from Search or at start, and the settings open on the first launch.
- **Library tools.** Sort, filter and tag favorites and Watch later, check marks on watched videos, and a red dot for new subscription videos.
- **More personal.** A custom theme color, high contrast mode, sound profiles, volume per channel and L / R shortcuts.
- **QR with time.** QR codes of the current video include the moment you are at.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
