#pragma once
#include <string>
#include <functional>
#include <vector>
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
// the current UTC time from the internet (the Date header of a Google answer : the console clock has no reliable time zone)
bool youtube_studio_utc_now(long long &epoch);
// publish_in_seconds > 0 : the video is uploaded private and YouTube publishes it that much later
bool youtube_studio_upload(const std::string &path, const std::string &title, const std::string &description,
                           const std::string &privacy, bool is_short, const std::string &category_id, const std::string &tags, int publish_in_seconds,
                           const std::string &resume_url, const std::function<void(u64, u64)> &progress, std::string &video_id,
                           std::string &error);
// An upload that did not finish (the app was closed, no connection...) can be continued : this tells which one is pending.
bool youtube_studio_pending_upload(std::string &path, std::string &title, std::string &upload_url);
void youtube_studio_clear_pending_upload();

// ---- your uploaded videos (YouTube Data API, same login) ----
struct StudioVideo {
	std::string id, title, privacy; // privacy : "public", "unlisted" or "private"
};
bool youtube_studio_list_videos(std::vector<StudioVideo> &out, std::string &error); // the latest 25 uploads
bool youtube_studio_set_privacy(const std::string &video_id, const std::string &privacy, std::string &error);
bool youtube_studio_set_title(const std::string &video_id, const std::string &title, std::string &error);
// views, likes and comments of one of your videos ("1.2K", "35", "4"), "-" when a number is hidden
bool youtube_studio_video_stats(const std::string &video_id, std::string &views, std::string &likes, std::string &comments);
bool youtube_studio_delete(const std::string &video_id, std::string &error);
// custom thumbnail from a .jpg / .png file on the SD card (max 2 MB). The account must be verified by YouTube.
bool youtube_studio_set_thumbnail(const std::string &video_id, const std::string &path, std::string &error);
