Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.23

**This is the final version of Iker's Tube.** The app is complete and keeps working as long as YouTube allows it. There will be no more updates, but the source stays open (GPLv3): anyone can fork it and keep it alive. Thank you for every video, every bug report and every idea.

### Changes
- **Farewell.** The first start after the update shows a farewell dialog (with the time you spent watching with the app), More > About has a Farewell section with the full message, the Update tab says it is the final version, and the website has a farewell card and a finished roadmap. The dialog can be shown again from Settings > Advanced ("Show the news again").
- **Sleep timer.** When it ends it also stops the autoplay (the countdown to the next video and a video that is still loading), not only the video that plays, so nothing starts playing during the night.
- **Mini player.** A tap on the mini player controls no longer also reaches the hidden menu icon behind them.
- **Closing the app.** The exit no longer needs every thread to exist (a thread that failed to start crashed the close), and, as in 1.3.23, running network requests stop first.
- **Languages.** The farewell texts are translated to French, German, Italian, Portuguese and Japanese (Spanish and English included).
- **Documentation.** The README and the Spanish guide explain the final version.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
