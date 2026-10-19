#pragma once
#include <string>

// Channels pinned on top of the local subscribed channels list (ids, kept in sdmc:/3ds/IkersTube/pinned_channels.txt)
bool pinned_channel_is(const std::string &channel_id);
void pinned_channel_set(const std::string &channel_id, bool pinned);
