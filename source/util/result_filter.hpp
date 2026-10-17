#pragma once
#include <string>

// true if a video of a list (search results, home feed) has to be left out : Shorts and watched videos, as the settings say
bool result_video_hidden(const std::string &url);
