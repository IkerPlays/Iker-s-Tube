Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.6

### Changes
- **Player.** A tap in the bottom-right corner while a popup is open goes to the popup instead of opening the hidden menu, a failed seek says so instead of stopping silently, and a failed Previous no longer leaves its mark on the next load.
- **Volume.** The volume bar of the player and the Mute button / sleep timer share one volume, so the sound no longer jumps back after the timer fades or after Mute and Unmute.
- **Live chat.** The chat window is not rebuilt while a finger is on it, so taps on Write, Close and the mode button are no longer lost on a busy chat.
- **Sign-in.** The requests of the sign-in code (token refresh, device code, library and subscription fetches) go one at a time through its network session, so two threads can no longer free each other's connections.
- **Live streams.** A live segment that fails to open, or is interrupted, frees its ffmpeg contexts, so a long outage no longer eats memory with every retry.
- **Decoder and icons.** A seek whose audio filter cannot be rebuilt now fails cleanly instead of crashing on the next audio frame, and a channel icon that cannot get its transparency copy (low memory) is drawn as it is instead of reading past its buffer.
- **Theme.** Choosing the dark theme by hand while "Automatic theme by time" is on turns the automatic theme off (it used to flip the choice back at once), and both settings show the real state when you come back to Settings.
- **Thumbnails.** A picture whose texture upload fails is asked again after a few seconds instead of being decoded again at every loop.
- **Home.** Pull to refresh on Account channels no longer freezes Home while it waits for the Wi-Fi and a failed refresh keeps the list, Tap to try again on the account feed reloads it when the first page failed, and refreshing a channel no longer leaves a moment where it is missing from your subscriptions.
- **Library.** L / R do not change tabs under an open QR popup, Delete all downloads shows how many downloads go, and the downloads search row stays visible while a filter is set.
- **Playlists.** Creating a list from the add-to-list dialog, Save the queue and Duplicate find the new list under the name it was stored with (a name with a trailing space or accents used to leave an empty list), and adding a video to a list where it already is says so instead of doing nothing.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
