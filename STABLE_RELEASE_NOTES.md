Iker's Tube is a YouTube client for the Nintendo 3DS family, built on top of FourthTube and ThirdTube (GPLv3).

## What's new since 1.3.22

### Changes
- **Closing the app.** Network requests that are still running are told to stop at the very start of the exit, and their wait is 0.5 seconds instead of 10, so closing the app while the Wi-Fi is dead or a server is slow no longer freezes the screen for tens of seconds.
- **Startup.** libcurl is initialised once before any thread starts (its implicit first-use initialisation is not thread-safe, and the update check, the feed and the thumbnails all start together).
- **Website.** A release list that only loaded partly (GitHub request limit) is no longer cached as complete, a blocked QR library still shows the address, the latest stable is the highest version (not the newest date), and the release notes headings and long links are styled.

## Installing

Install `IkersTube.cia` with FBI, or run `IkersTube.3dsx` from the Homebrew Launcher.
The DSP firmware dump (`sdmc:/3ds/dspfirm.cdc`) is needed for sound.
