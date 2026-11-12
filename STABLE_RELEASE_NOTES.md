Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.14

### Changes
- **Per-channel settings.** A speed or volume of 100% chosen for a channel is remembered even when your default in Settings is different (it used to be erased, and the default came back), and Slower / Faster also remember the speed for the channel.
- **Player bars.** The volume bar respects the Settings volume cap, the speed bar goes up to 200% (it showed 150% while 200% played and dropped the speed when released), and the sleep timer bar is translated.
- **Auto quality.** The first step down after a stall at 720p or 1080p is 480p, not 360p.
- **Audio language and data saver.** The Audio language tool and the data-saver exemption of a chosen quality act on the video that plays, not on the page you are looking at.
- **Queue.** "Shuffle queue" keeps the repeat list and saves once (it cleared the queue, which also turned repeat off).
- **Text.** Chapter and audio track names in notices are cut on a letter boundary.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
