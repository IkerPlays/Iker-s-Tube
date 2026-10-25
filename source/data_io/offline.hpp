#pragma once
#include <string>
#include <vector>

// Audio saved on the SD card for offline listening : sdmc:/3ds/IkersTube/offline/<id>.m4a plus an index file.
struct OfflineItem {
	std::string id;
	std::string title;
	std::string author;
	int quality = 0;       // 0 : audio only, otherwise the height of the saved video (240, 360)
	bool combined = false; // the video file already has the sound (one stream), otherwise the sound is the .m4a file
};

std::vector<OfflineItem> offline_list();
bool offline_get(const std::string &id, OfflineItem &out);
std::string offline_file_path(const std::string &id); // "sdmc:/3ds/IkersTube/offline/<id>.m4a"
std::string offline_video_path(const std::string &id); // "sdmc:/3ds/IkersTube/offline/<id>.mp4"
bool offline_add(const OfflineItem &item);              // registers a finished download (false: the index could not be written)
bool offline_has_id(const std::string &id);     // cheap (cached) : is this video saved ?
bool offline_remove(const std::string &id);             // deletes the file and the entry (false when a file could not be deleted : the entry stays)
void offline_remove_all();                              // everything at once (the index is written once)
