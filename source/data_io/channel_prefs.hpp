#pragma once
#include <string>

// Playback speed remembered per channel, stored in sdmc:/3ds/IkersTube/channel_speed.txt (channel id, speed per line)
double channel_speed_get(const std::string &channel_id); // 0 if nothing is stored
void channel_speed_set(const std::string &channel_id, double speed);
