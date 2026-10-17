#pragma once
#include <string>
#include <vector>

// Audio saved on the SD card for offline listening : sdmc:/3ds/IkersTube/offline/<id>.m4a plus an index file.
struct OfflineItem {
	std::string id;
	std::string title;
	std::string author;
};

std::vector<OfflineItem> offline_list();
bool offline_get(const std::string &id, OfflineItem &out);
std::string offline_file_path(const std::string &id); // "sdmc:/3ds/IkersTube/offline/<id>.m4a"
void offline_add(const OfflineItem &item);              // registers a finished download
void offline_remove(const std::string &id);             // deletes the file and the entry
