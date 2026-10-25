Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.1

### Changes
- **Updater.** A release tagged -dev is never taken for a stable one, even if GitHub's prerelease flag is missing.
- **Sign-in.** Token renewals run one at a time, a failed renewal is not repeated for 30 seconds, the profile is not downloaded again at every renewal, the saved token file is checked for its full size, and an empty answer from YouTube counts as done for account actions.
- **Lists.** Favorites and Watch later drop duplicated videos when read and cut very long titles, and an empty filtered list says "Nothing matches the filter" instead of "you have none".
- **Saved videos.** Deleting a saved video that is in use says so instead of pretending it worked.
- **Fixes.** All yes / no settings are clamped when read, and file sizes over 2 GB in the file browser no longer turn negative.
- **Search.** A pasted video address opens the video, a filter change keeps the Shorts or Radio feed a feed, and a search text with %0 or %1 is no longer altered.
- **Live chat.** Switching between Top and All chat shows messages again, and the chat stops polling when the stream or its chat is over.
- **SponsorBlock and translation.** SponsorBlock never seeks past the end of a video and its marks are hidden when it is off, and long text is cut on letter boundaries before translating.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
