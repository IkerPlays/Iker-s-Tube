## What's new

- **Fix: videos failing with "No valid stream url extracted".** A mistake in the previous stability changes made the player forget the audio and combined stream addresses right before starting the decoder, so every video failed. They are read correctly again, together with the other stream settings, under one lock.
- **Long videos without separate audio.** Videos longer than an hour that YouTube offers only as a single combined stream (no separate audio) failed with "No valid stream url extracted". The combined stream is now kept in that case, and the other player clients are tried first to find a separate audio stream. log.txt also lists the audio formats YouTube offered when none is usable.
- **Better error details.** When a video cannot be played, log.txt now says why (YouTube status and reason, how many streams were found), and the "No valid stream url" message lists what was available.
- **Network retry.** A request that fails because the host name cannot be resolved is tried again up to three times, which covers the first seconds after the Wi-Fi connects.
- **Crash report file.** If the app aborts because of an unexpected error, it now writes sdmc:/3ds/IkersTube/crash.txt (error type, version and a list of code addresses) so the exact place can be found. Send that file together with log.txt after a crash.
- **Freeze fix.** The automatic quality check could wait for the video decoder while holding a lock the decoder needed.
- **Stability.** A full crash review of the player, downloader, thumbnails and parser. Fixed: a freeze when an A-B loop reached its end point; chapters reading uninitialized values; a crash after changing the comment sort; a possible crash when memory runs out while creating thumbnails or textures; unbounded memory growth of the page cache; unsafe access to the stream URLs while a video starts; short reads ending a video early; out-of-range reads in the playlist tab, the JSON parser and the thumbnail list; the mini player and QR popup handling a key twice; and several uninitialized fields.
- **Continue watching.** The Search screen shows the last video you played, and you can make the app open it at start (Settings, Start screen, Last video).
- **First launch.** The very first time the app opens, Settings appear so you can pick language, theme and quality.
- **Library.** Sort favorites and Watch later (newest, title, channel), filter them by text, and give favorites a tag. Videos you already watched get a check mark in lists (can be turned off).
- **New videos alert.** A red dot on the menu icon tells you when your subscriptions have new videos after the feed loads.
- **Custom theme.** A seventh theme with a color you pick, plus a high contrast mode.
- **Sound profiles.** Music, Movie and Speech profiles in the equalizer combine a curve and a volume.
- **Volume per channel.** The mini player volume is remembered for each channel, like the speed.
- **L and R shortcuts.** Choose what L and R do in the player, like X and Y.
- **QR with time.** The QR code of a video you are watching includes the current moment.
