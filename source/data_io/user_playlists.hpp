#pragma once
#include <string>
#include <vector>
#include "data_io/favorites.hpp"

// Named playlists created by the user, stored in sdmc:/3ds/IkersTube/playlists.txt
// ("#name" line, then one tab-separated video per line).
struct UserPlaylist {
	std::string name;
	std::vector<FavoriteVideo> videos;
};

std::vector<UserPlaylist> playlists_get();
std::string playlist_clean_name(const std::string &name); // the name as playlist_create stores it (trimmed, cut to 40 bytes): use it to find the list afterwards
bool playlist_create(const std::string &name);                      // false : empty / duplicated name / too many
void playlist_delete(int index);
bool playlist_rename(int index, const std::string &name);           // false : empty / duplicated name
bool playlist_add(int index, const FavoriteVideo &video);           // false : already in the list / list full
void playlist_remove(int index, const std::string &video_id);
