#pragma once
#include <string>

// Seconds skipped at the start (intro) remembered per channel, stored in sdmc:/3ds/IkersTube/channel_skip.txt
double channel_skip_get(const std::string &channel_id); // 0 if nothing is stored
void channel_skip_set(const std::string &channel_id, double seconds);
