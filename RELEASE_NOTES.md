Devbuild notes

- Comments: with the Studio login, posting, replying and deleting comments now go through the YouTube Data API first (the app login is often refused for them) and fall back to the old way. If it still fails the notice shows why ("data api: ..."). Liking a video also falls back to it.
- History: videos you watch while signed in (and not in incognito) are now reported to YouTube, so they appear in your real history and the recommendations learn from them.
