#pragma once
#include <string>

// Volume (preamp factor) remembered per channel, stored in sdmc:/3ds/IkersTube/channel_volume.txt
double channel_volume_get(const std::string &channel_id); // 0 if nothing is stored
void channel_volume_set(const std::string &channel_id, double volume);
