#pragma once
#include <string>
#include <vector>

// The channels you opened most recently (newest first, up to 8), kept in sdmc:/3ds/IkersTube/recent_channels.txt
struct RecentChannel {
	std::string name, url;
};
void recent_channel_add(const std::string &name, const std::string &url);
std::vector<RecentChannel> recent_channels_get();
