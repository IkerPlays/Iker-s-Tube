#pragma once
#include <string>

// true if a video of a list (search results, home feed) has to be left out : Shorts and watched videos, as the settings say,
// and the videos of the channels you chose to hide (by channel name)
bool result_video_hidden(const std::string &url, const std::string &author = "");

// channels hidden from the lists (kept in sdmc:/3ds/IkersTube/hidden_channels.txt, one name per line)
bool hidden_channel_is(const std::string &name);
void hidden_channel_set(const std::string &name, bool hidden);
int hidden_channel_count();
void hidden_channels_clear();
