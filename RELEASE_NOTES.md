Devbuild notes

Fixes from a code review of the latest additions:
- Studio: new uploads sent their data to an empty address and always failed (a bug of the resume feature). Fixed.
- SponsorBlock: it now asks again when you turn it on during a video, when the connection comes back, or after a failed request (at most every 20 seconds).
- Thumbnail cache on the SD card: files are written to a temporary name first and renamed, a bad file is deleted when it cannot be decoded, and the disk work no longer happens while the thumbnail list is locked.
