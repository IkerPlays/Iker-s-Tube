Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.0.7

### Changes
- **Network safety.** The live chat and the loaders can no longer use the same network sessions at once.
- **Download QR.** The About screen has a row that shows a QR code for the latest stable IkersTube.cia.
- **Badges.** My Wrapped lists 13 badges you unlock by watching (streaks, favorite hour, hours playing, ...).
- **Most replayed.** My Wrapped shows the video you replayed most, and a tap plays it.
- **More numbers.** My Wrapped also shows the last 30 days and your longest streak.
- **Info shows your history.** The Info tool tells how many times you watched a video and how many videos of its channel you have watched.
- **Equalizer.** The audio-only screen has a small animated equalizer.
- **Loading placeholders.** Thumbnails pulse softly while they load.
- **Instant feed.** The Feed tab shows your last feed immediately when the app opens, then refreshes it.
- **Memories.** The History shows a video from about a week or a month ago to watch again.
- **Fixes.** Spanish and Portuguese interface languages are kept after restarting the app, and the translation table no longer needs a huge stack frame at startup.
- **Portuguese.** The interface language can be set to Portuguese.
- **Channel history.** The channel page tells how many of its videos you have watched.
- **Ends at.** The Status tool of the player says when the video will be over.
- **Dislikes.** A new player tool shows the estimated likes and dislikes of a video, using the public Return YouTube Dislike API (only the video id is sent).
- **Late-night hint.** After 25 minutes of playing late at night without a sleep timer, a notice suggests the Sleep timer tool.
- **NEW badge.** Feed videos you had not been told about carry a NEW badge.
- **Badge notices.** You get a notice when you unlock a badge.
- **More badges.** 13 badges now, including collector, offline fan and subscriber.
- **Skip intro.** A player tool makes every video of a channel start 5 to 60 seconds later, remembered per channel.
- **Richer progress bar.** Chapter starts are marked on the bar, and the parts SponsorBlock skips show in green.
- **Controls help.** A player tool lists what each button does.
- **Share my numbers.** My Wrapped can show your stats as a QR code.
- **Hardening.** The app recovers list files interrupted while saving and ignores damaged values in its data files.
- **Weekly chart.** My Wrapped shows how long you played on each of the last 7 days.
- **Seasonal shortcut.** Search shows a Halloween, Christmas or New Year shortcut in season.
- **Fade-in.** Changing screens fades the bottom screen in for a moment (part of the rounded style).
- **Lyrics.** With subtitles on, the audio-only screen shows them too (handy for song lyrics).
- **Go to time and Loop chapter.** Two new player tools: jump to a typed time, and repeat the current chapter.
- **Play the new videos.** The Feed tab has a button that plays all the NEW videos in a row.
- **Player tools reachable.** Rows of tools that were wider than the screen wrap into several rows, so every tool can be tapped.
- **Player tools folded.** The long list of extra tools in the player sits behind a More tools button; all tools are reachable now.
- **Account fixes.** A failed or timed-out sign-in can be retried, a network hiccup no longer signs you out, and several threading problems in the account code were fixed.
- **Fewer renewals.** The login token is renewed only when it is about to expire, and an expired one is renewed automatically.
- **Downloads fixes.** More reliable cancelling, a full SD card cannot damage the downloads list, no duplicate downloads, and Delete all downloads is faster.
- **Loader fixes.** Failed channel and video pages are retried when you open them again, searches with quotes work, and one unknown result no longer stops a search.
- **Fix.** Comments or suggestions that finish loading after switching videos no longer replace the new video's page.
- **Setting.** Badge and late-night notices can be turned off.
- **Fixes.** Safer backups (an import is applied at the next start), a crash-risk in the search filter menus removed, and several playback and thumbnail race conditions fixed.
- **Quick volume and brightness.** In the player, SELECT + D-pad up/down changes the volume and left/right the brightness.
- **Feed and loaders.** The feed loads in small groups (steadier with hundreds of subscriptions), hidden channels now hide in the feed, the live chat stops when you leave the video, and several race conditions in the captions and comments loaders were fixed.
- **Saves on exit.** Changes made right before closing the app (settings, history, subscriptions, play time) are no longer lost.
- **Surprise from the feed.** A button in the Feed tab opens a random video of your feed.
- **Fixes.** Playlists keep full video titles, the queue tab acts on the right video, typed searches leave Radio mode, and a full SD card can no longer damage favorites, playlists or the queue.
- **Retry.** A failed search or results page can be retried by tapping the error.
- **Console language.** On the first start the app uses the language of the console when it is supported.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
