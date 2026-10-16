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
bool playlist_create(const std::string &name);                      // false : empty / duplicated name / too many
void playlist_delete(int index);
bool playlist_add(int index, const FavoriteVideo &video);           // false : already in the list / list full
void playlist_remove(int index, const std::string &video_id);
