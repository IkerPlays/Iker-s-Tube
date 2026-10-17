#pragma once
#include <string>

// The last video that was played, stored in sdmc:/3ds/IkersTube/last_played.txt (id, title)
void last_played_set(const std::string &id, const std::string &title);
bool last_played_get(std::string &id, std::string &title);
