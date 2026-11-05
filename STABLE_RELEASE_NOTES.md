Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.6

### Changes
- **Player.** A tap in the bottom-right corner while a popup is open goes to the popup instead of opening the hidden menu, a failed seek says so instead of stopping silently, and a failed Previous no longer leaves its mark on the next load.
- **Volume.** The volume bar of the player and the Mute button / sleep timer share one volume, so the sound no longer jumps back after the timer fades or after Mute and Unmute.
- **Live chat.** The chat window is not rebuilt while a finger is on it, so taps on Write, Close and the mode button are no longer lost on a busy chat.
- **Sign-in.** The requests of the sign-in code (token refresh, device code, library and subscription fetches) go one at a time through its network session, so two threads can no longer free each other's connections.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
