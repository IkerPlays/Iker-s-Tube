#pragma once
#include <string>
#include <functional>
#include <3ds.h>

// YouTube Studio : channel statistics and video uploads of the signed-in account. These block on the network, so call
// them from a worker thread.
struct StudioStats {
	std::string subscribers = "-";
	std::string views = "-";
	std::string videos = "-";
	std::string error; // empty when the statistics were loaded
};
StudioStats youtube_studio_stats();

// Uploads the file at `path`. `privacy` is "private", "unlisted" or "public". A short is a vertical video of up to 3
// minutes whose description has #Shorts. `progress(done, total)` is called after every chunk.
// Returns true and fills `video_id` on success, otherwise `error` tells why.
bool youtube_studio_upload(const std::string &path, const std::string &title, const std::string &description,
                           const std::string &privacy, bool is_short, const std::string &category_id,
                           const std::function<void(u64, u64)> &progress, std::string &video_id, std::string &error);
