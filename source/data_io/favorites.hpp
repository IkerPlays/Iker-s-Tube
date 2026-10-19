#pragma once
#include <string>
#include <vector>

struct FavoriteVideo {
	std::string id;
	std::string title;
	std::string author;
	std::string length_text;
	std::string tag; // free label chosen by the user (favorites only), "" if none
};

// "Favorites / watch later" list, stored in sdmc:/3ds/IkersTube/favorites.txt (one tab-separated video per line)
void favorites_load();
bool favorites_contains(const std::string &video_id);
bool favorites_toggle(const FavoriteVideo &video); // returns true if it was added
void favorites_remove(const std::string &video_id);
void favorites_clear(); // everything at once (one save)
std::vector<FavoriteVideo> favorites_get();
void favorites_set_tag(const std::string &video_id, const std::string &tag);
