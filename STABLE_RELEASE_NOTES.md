Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.5

### Changes
- **Player.** A video that cannot be played (private, removed, no connection) opened by autoplay, the queue or Next now says so instead of stopping silently, Play after the retries ran out gets new retries, a stream cut before its end resumes from where it stopped (not from 0:00) when you press Play or A, and changing the comment order on a bad connection keeps the comments you already see.
- **Decoder.** A failed stream probe is reported, a damaged H.264 packet or missing SPS/PPS can no longer read or write outside its buffer, and a packet is no longer leaked when the stream restart fails.
- **Exit.** Closing the app while a sign-in check is running no longer frees the sign-in network session twice.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
