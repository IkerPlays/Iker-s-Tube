Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.0

### Changes
- **Account safety.** Subscribe, like and comment actions no longer report success when YouTube refused them (spaces in the answer, empty or HTML answers), a refresh that fails for a passing reason keeps you signed in, and the sign-in file is saved through a temporary file so a power loss cannot empty it.
- **Updater.** It looks at the 30 newest releases (a stable could fall out of the first 10), a tag with -dev is always a devbuild, and it never installs by itself while a video plays.
- **Autoplay setting saves.** The Autoplay choice is written right away.
- **Readable accent text.** Coloured text keeps a good contrast on every theme, light or dark, long channel names and handles are cut before the Subscribe button, and the switch knob and slider handle are readable on every accent.
- **Faster text measuring.** Measuring a label no longer allocates memory every time.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
