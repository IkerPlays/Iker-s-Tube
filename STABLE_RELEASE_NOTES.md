Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.0.2

### Offline
- **Offline videos.** The Save video button saves a video on the SD card (360p, 240p on the old 3DS) next to the old Save audio. Saved videos play without any connection from the Offline list.
- **Save offline from lists.** Long press a video in Favorites, Watch later, a playlist or the queue to save its video or audio. The app loads it in the background.
- **Download queue.** Several downloads can be queued. The Downloads screen shows the progress, how many are waiting and lets you cancel.
- **Downloads screen.** Items saved, space used and free on the SD card, and Play all / Shuffle for everything you saved.
- **Sort and search downloads** by newest, title or size, or by text, and the video page shows when a video is already saved.
- **Clean up.** An optional setting deletes a download after you watch it, and a button deletes all downloads (with confirmation).
- **Resilient downloads.** A Wi-Fi hiccup is retried instead of failing the whole download.
- **Download indicator.** A green dot on Downloads while something downloads.
- **Saved copies.** A DL tag marks the videos that are saved, and without internet opening one of them plays the saved copy.
- **No connection strip.** A red strip says when the console has no internet and reminds that saved downloads still play.

### New look
- **Bottom navigation bar** with icons (Home, Search, Library, Downloads, More) on the main screens.
- **Pill tabs** in the accent color and rounded buttons.
- **Start screen.** A loading screen with a progress bar from the first moment, kept for 3 seconds at the end so the account and data load (the .cia now carries the real Homebrew logo animation instead of the old FourthTube one; the HOME Menu plays it before the app).

### Studio
- **Statistics kept.** The last numbers are saved and shown when they cannot be loaded.
- **Statistics fixed.** Subscribers, total views and total videos come from the public About page of your channel, so they load without the Data API.
- **Own Studio login.** Uploading and editing need the YouTube Data API, which the main login cannot use. Studio has its own sign-in with a Google Cloud client of yours, built into the app by the build secrets or set up in the Studio tab with a QR or by typing (see Documentation/STUDIO_SETUP.md).
- **Upload with more options.** Category and custom thumbnail, a button to watch the uploaded video, and uploads run in their own network session.
- **Video numbers.** Opening one of your uploads shows its views, likes and comments.
- **Resume uploads and tags.** An interrupted upload can be continued later, and the upload form has tags.
- **Edit my uploads.** Change visibility, title, delete, or watch your latest uploads (without the Studio login it lists your public videos). Create playlists and open your channel.

### Comments and player
- **Autoplay countdown.** An optional wait (3, 5 or 10 s) with a countdown before the next video, cancelled with A.
- **Collaboration videos.** Tap the author of a video with several channels to see all of them (icon, handle, subscribers), open any, or long press one to subscribe.
- **Comments.** Sending, replying and liking try several YouTube clients, and a failure now shows the reason on screen. Pinned and creator-hearted comments are marked.
- **SponsorBlock.** An optional setting skips the parts of a video that the community marked as sponsors (or also self promotion, intros and outros).
- **Volume and brightness buttons.** X, Y, L and R can be set to volume +/- and brightness +/-.
- **Own comments.** Delete your own comments, and like or reply on comments of any page (best effort).
- **Chapters** show as ticks on the buffering bar, and the last subtitle language is preselected.

### Library and reliability
- **Continue watching.** The first item of Home is the last video you played.
- **First-use tour and error report.** A short tour on first use, and Create a report to send in Settings (one file with version, state and log).
- **Thumbnail cache on the SD card.** Pictures already seen show at once and use no data.
- **Queue and backups.** The play queue survives a restart, your data is backed up on the SD card once a week, and the app tells you when the last run ended in a crash.
- **Incognito mode.** From the More menu or Settings: no history, searches, resume points or last video are remembered.
- **Search.** More Explore shortcuts (Shorts, Learn something, Funny), long press a recent search to forget it, and optional settings hide Shorts and already watched videos in search, home and the subscription feeds.
- **Fixes from three code reviews.** A failed re-download no longer deletes a saved copy, saved videos with one file can play as audio, the bottom bar no longer piles up screens, the crash notice stays visible, comment details are shared safely between threads, Failed download threads no longer block later downloads, popups no longer leak on every visit, a failed read of an offline file no longer breaks playback, and uploads no longer clash with other network requests.
- **Bell, Not interested, Hide channel.** New video tools when signed in: per-channel notification level, and telling YouTube to stop recommending a video or channel.
- **Scheduled publishing.** Studio upload has Publish: now, 1 hour, 3 hours, 1 day, 3 days or 1 week; the time comes from the internet (the console clock has no reliable time zone).
- **Usage statistics.** The Data tab of Settings shows how many videos you watched and about how many hours.
- **Shorts feed.** The Shorts shortcut in Explore plays the whole list one after another in a shuffled loop.
- **Comments and history through your account.** Posting, replying and deleting comments use the YouTube Data API when Studio is connected. Watched videos are reported to your YouTube history.
- **Liked tab.** The Library shows the videos you liked on YouTube.
- **Live and upcoming in the feed.** The subscriptions feed puts live streams first, then upcoming premieres with the time left, and warns when one starts within 15 minutes.
- **Live chat.** Read the chat of a live stream from the player tools, and write in it with the Studio login.
- **Audio language and links.** Choose the dubbed audio of videos that have several tracks, and open the links of the description through a QR code.
- **Night downloads.** A setting makes the downloads wait for the hours 1:00 to 7:00.
- **Quick sleep timer and most watched channels.** A Sleep timer button in the player tools, and your top three channels in Settings, Data.
- **Queue to YouTube.** Save the play queue as a private playlist of your account.
- **Subscribe from the channel page.** It now also subscribes on your YouTube account.
- **Hide the Studio tab.** A setting removes the Studio tab from the Home screen.
- **More modern look.** Channel cards, rounded and spaced player buttons, accent popup titles, rounded dialogs and buttons.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
