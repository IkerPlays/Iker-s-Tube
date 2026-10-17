Devbuild notes

- Comments: sending, replying and liking now try several YouTube clients (and ask for the video page itself if the comment page is refused). If it still fails, the notice now shows the reason (endpoint, client, error) and the log keeps the server answer.
- Studio statistics: subscribers, total views and total videos now come from the public About page of your channel, so they load without the Data API (that was the 403).
- Studio uploads and edits need the YouTube Data API, which the main login cannot use. Studio has its own sign-in with a Google Cloud client of yours: see Documentation/STUDIO_SETUP.md (put studio_client.txt in 3ds/IkersTube, then Sign in for Studio in the Studio tab). Without it, Edit my uploads lists your public videos (read only).
- Start screen: the Homebrew mark replaces the app logo, and a 3 second loading screen with a progress bar gives the account and data time to load before the first screen.
- Studio: after an upload finishes, a button opens the uploaded video.
