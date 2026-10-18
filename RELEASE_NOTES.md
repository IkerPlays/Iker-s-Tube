Devbuild notes

- Collaboration screen: the channels are now three per page with a More button (no scrolling), so the cards no longer draw outside the popup.
- Comments: YouTube refused the Data API for comments (insufficient scopes: the Studio login cannot get that permission), so it is no longer tried after the first refusal. The old way now also works without opening the comments tab, and a failure shows both reasons.
