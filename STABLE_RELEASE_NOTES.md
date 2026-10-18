Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.0.3

### Account and YouTube
- **History.** Videos you watch while signed in (not in incognito) are reported to your YouTube history.
- **Liked tab.** The Library shows the videos you liked on YouTube.
- **Queue and lists to YouTube.** Save the play queue or one of your local lists as a private playlist of your account.
- **Subscribe from the channel page.** It now also subscribes on your YouTube account.
- **Live and upcoming in the feed.** The subscriptions feed puts live streams first, then upcoming premieres with the time left, and warns when one starts within 15 minutes.
- **Comments.** Without a comments token (comments not opened) writing and replying now work, and when a comment or chat message fails a screen shows the whole reason. With the Studio login the Data API is tried first (YouTube may refuse it for lack of scopes, then the old way is used). Liking a video falls back to the Data API too.

### Player
- **Live chat.** Read the chat of a live stream from the player tools (Super Chats included, Top chat or All chat), and write in it with the Studio login.
- **Audio language and links.** Choose the dubbed audio of videos that have several tracks, and open the links of the description through a QR code.
- **Quick sleep timer.** A Sleep timer button in the player tools.
- **Collaboration screen.** Channels are cards, three per page with a More button, so nothing is drawn outside the screen, and a Subscribe all button follows every channel of the video.
- **Live chat fits.** It shows the newest messages that fit above its buttons.

### Library, settings and look
- **Podcasts, Courses and Releases.** Channels that have those tabs show them as categories of the Playlists tab.
- **Shorts feed.** The Shorts shortcut in Explore plays the whole list one after another in a shuffled loop.
- **Night downloads.** A setting makes the downloads wait for the hours 1:00 to 7:00.
- **Usage statistics.** Settings, Data shows how many videos you watched, about how many hours and your top three channels.
- **Hide the Studio tab.** A setting removes the Studio tab from the Home screen.
- **More modern look.** Rounded and spaced player buttons, accent popup titles, rounded dialogs and buttons, channel cards and the six Library tabs across the whole width.
- **Studio login.** It now shows the address and the code on separate lines so the code is never cut off.
- **Channel QR.** The Info tab of a channel has a QR code button.
- **Your YouTube lists in the Library.** The Liked tab cycles through your liked videos, your YouTube history and each playlist of your account (best effort).
- **Comment diagnostics.** When comments cannot be sent, the failure screen lists what YouTube answered, and new comments and replies have a last-resort way to be sent.
- **Bulk actions in the Library.** Play all, Shuffle and Save audio on Favorites, Watch later and Liked; Shuffle queue and Save all audio on the queue.
- **Play or queue a whole search or channel.** Play all and Queue all at the top of the results and of the Videos tab of a channel.
- **Watch later cleanup and player buttons.** An option removes a video from Watch later when you finish it; the player has Next video, -1 min and +1 min.
- **Free room.** Delete the downloads you already watched, and clear the picture cache.
- **Video actions menu everywhere.** Long press a video in any list to play it next, queue it, save it to Watch later or Favorites, download it, or say Not interested.
- **Default speed.** A setting for the playback speed of channels without their own.
- **Save a whole playlist.** A player tool downloads the audio of the playlist the video is in.
- **History as a playlist.** Play all, Shuffle and a text search on the History screen, and the full video menu on long press.
- **Channel menu.** Long press a channel in the search results to subscribe or set the bell.
- **Upload to a playlist.** The Studio upload form can add the new video to one of your playlists.
- **Edit the description** of your uploads from Studio.
- **More search filters.** Live now, Playlists, Channels, Last hour, This year, sort by date or views, and HD.
- **Progress bars and day headers.** Started videos show a red progress bar on their thumbnail, and the History is grouped by day.
- **Explore shortcuts and queue preview.** Eight more Explore topics, and the player shows the next video of the queue.
- **Auto chat.** A setting opens the live chat by itself when a live stream plays; the player also has a Restart tool, and the channel Videos row can save audio.
- **Saved video quality.** A setting chooses 144p, 240p or 360p for the videos you save, and the video menu can mark videos as watched.
- **Reset the settings** from Settings, Data (lists, history and downloads are kept).
- **Autoplay skips watched.** A setting makes autoplay avoid the videos already in your history.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
