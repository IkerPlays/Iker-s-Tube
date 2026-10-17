#pragma once
#include <string>
#include <vector>

struct Bookmark {
	double time;
	std::string name;
};

// Named moments of a video, stored in sdmc:/3ds/IkersTube/bookmarks.txt (video id, seconds, name per line)
std::vector<Bookmark> bookmarks_get(const std::string &video_id);
void bookmark_add(const std::string &video_id, double time, const std::string &name);
void bookmark_remove(const std::string &video_id, size_t index); // index inside bookmarks_get(video_id)
