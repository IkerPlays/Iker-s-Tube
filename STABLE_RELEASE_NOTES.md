Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.7

### Changes
- **History and tabs.** Opening History again no longer leaks a dialog each time, and a tab view given a selected tab out of range clamps it instead of reading past its list.
- **Search.** An error answer (400 / 403) to Load more keeps the continue token so asking again works, and the "N results" count stays after Load more.
- **Home.** One failed Load more no longer blocks every later one, and the New videos filter reads only the first number of a date ("3 days ago") and ignores absolute years.
- **Numbers from YouTube.** A number that YouTube sends as text (such as a playlist's video count) is read, instead of showing 0.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
