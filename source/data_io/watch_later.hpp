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

// Adds or removes the video in the YouTube "Watch later" playlist (needs the OAuth login; no-op otherwise). Call from a worker thread.
void watch_later_sync_remote(const std::string &video_id, bool add);
