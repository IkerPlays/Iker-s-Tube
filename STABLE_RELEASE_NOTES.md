Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.0.5

### Player
- **Subtitle tools.** CC size and CC color change the subtitles on the spot. The Info tool also lists the subtitle languages and chapters.
- **More tools.** Video QR is joined by QR at this time, Autoplay (off, list, all), Slower and Faster (5% steps), Hashtags (searches the tags of a video), +List (adds the video to a local playlist), Shuffle queue, Save list to YouTube, Screens off (listen with the screens dark; SELECT + START brings them back) and Status (clock and battery).
- **Chapters.** The chapter tools show the name of the chapter they jump to, and the chapter list highlights the one that is playing.
- **Seek step.** A setting chooses how many seconds the D-pad left and right jump (5, 10, 15 or 30).
- **Time left.** A setting makes the player bar show the time left instead of the length.
- **Videos in a row and break reminder.** Settings stop the queue and autoplay after 3, 5 or 10 videos (good to fall asleep) and remind you to take a break after 30, 60 or 120 minutes.
- **Battery.** The player warns at 15% and 5% battery.
- **Reload comments.** A Reload button in the Comments tab shows the comment you just sent. The comment keyboard offers quick phrases (your own in comment_phrases.txt).
- **Suggestions.** The Suggestions tab respects hidden channels and the hide settings.

### Search and menus
- **Combinable search filters.** The Filter button opens menus (sort, upload date, type, length, features) that can be combined. Medium (4-20 min) is new and the length filters now use the right YouTube values.
- **Related searches and tips.** The results show four related searches to tap. The start page shows a tip of the day and a Surprise me (random topic) button, plus seven more Explore shortcuts.
- **Menus.** The long press menus use pages (More options, Previous options) so all the options fit on the screen. A video can be liked on YouTube or added to a local playlist from its menu, and Forget where I stopped is there too.

### Library, history and channels
- **Library.** It opens on the tab you used last. Queue: Remove duplicates. Favorites and Watch later can be cleared (it asks first). Liked: remove your like. Downloads: sort by channel and a QR code option.
- **History.** Top channels lists the channels you watch most; long press a day header to delete that whole day.
- **Pinned channels.** Pin your favorite subscribed channels on top of the channels list.
- **Channel page.** Shuffle and Surprise me buttons for the videos of a channel.
- **Hidden channels.** A list shows them again one by one or all at once.

### Data, Studio and system
- **Import and export.** Export lists also writes your subscriptions as a CSV (local and account channels) and every local playlist as a text file. Import subscriptions reads import_subscriptions.csv.
- **Resume positions.** A Data button forgets where you stopped in the videos.
- **Studio.** The upload picker has a shortcut to the 3DS camera videos (DCIM) and upload_description.txt and upload_tags.txt fill the form of every new upload.
- **About and updates.** Tap the GitHub address to see its QR code. The update check uses the current repository address directly. The report has more facts to help with problems.

### Subtitles, audio tracks, Shorts and the account (carried over from the devbuilds)
- **Subtitles.** The CC tool turns on YouTube subtitles, cycles their languages and has a last step that translates them to the language of the app. A setting turns on the language you used last automatically.
- **Audio tracks.** Dubbed tracks are found by parsing the YouTube answer properly and trying two clients; report.txt says what the probe saw.
- **Shorts.** The Shorts shortcut opens the first Short at once and tries YouTube's real Shorts feed (experimental). D-pad Down and Up in the player go to the next and the previous video.
- **Account.** Rename or delete your own YouTube playlists in the Library, save a playlist you watch to your YouTube library, and long press a community post to like it or vote in its poll (experimental). A Bell button on the channel page changes its notification level.
- **Mark watched.** New player tool adds or removes the video in the history by hand.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
