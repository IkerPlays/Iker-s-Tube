#pragma once
#include <string>
#include <vector>
#include "data_io/favorites.hpp"


// "Favorites / watch later" list, stored in sdmc:/3ds/IkersTube/favorites.txt (one tab-separated video per line)
void watch_later_load();
bool watch_later_contains(const std::string &video_id);
bool watch_later_toggle(const FavoriteVideo &video); // returns true if it was added
void watch_later_remove(const std::string &video_id);
std::vector<FavoriteVideo> watch_later_get();

// Downloads the account's YouTube Watch later playlist and adds the videos that are missing locally (OAuth only).
// Call from a worker thread. Returns the number of videos added, or -1 if the request failed / no login.
int watch_later_pull_remote();
// Adds videos to the end of the local list without sending anything to YouTube; returns how many were new.
int watch_later_merge(const std::vector<FavoriteVideo> &videos);

// Adds or removes the video in the YouTube "Watch later" playlist (needs the OAuth login; no-op otherwise). Call from a worker thread.
void watch_later_sync_remote(const std::string &video_id, bool add);

// The videos of one of the account's YouTube playlists ("VLLL" liked videos, "VLWL" watch later). Worker thread only.
bool account_playlist_videos(const std::string &browse_id, std::vector<FavoriteVideo> &videos);
