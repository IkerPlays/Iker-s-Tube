Devbuild notes

- New Studio tab next to Home, Subscribed and New. It shows your subscribers, total views and total videos, and lets you upload a video or a Short from any folder of the SD card (title, description and visibility are asked before sending). It needs the OAuth login and uses the YouTube Data API: if YouTube refuses it for this login, the reason is shown in the tab and saved in log.txt.
- Comments: writing, replying and liking now use the same web client that loads the comments, which fixes "Could not send the comment". Failures are written with the server answer in log.txt.
- Network: request bodies can now be binary (needed for uploads).
- Studio: uploads use their own network session (no more clash with Reload), a lost answer or a 308 without Range is handled, retries wait a little, and the totals reload after an upload. A refused upload explains that YouTube does not allow it with this login.
- Comments: like and reply send their request with the same web client that produced their tokens.
