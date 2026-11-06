Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.8

### Changes
- **Load more.** An error answer (400 / 403) to Load more on a channel's Videos, Streams or Shorts, or on the comments, keeps the continue token, so the list no longer stops silently and asking again works.
- **Downloads.** If the last step of a multi-part download fails (SD card glitch), the parts already in place are removed instead of staying as orphan files.
- **Start screen.** The Start screen setting (Library, Search, Continue last video) and the first-run welcome Settings now take effect; the request was cleared before it was used, so the app always opened on Home.
- **Bottom bar.** Tapping the tab of the screen you are already on no longer suspends that screen without resuming it (Home and Search kept their loader paused).
- **Settings.** Long toast messages are cut to fit the screen, "Mini player" and "Normal" are translated, a damaged automatic quality cap value is ignored, and a write that accepts nothing no longer hangs the update install.
- **Search history.** A very long query is cut on a letter boundary so it is not read back as two entries.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
