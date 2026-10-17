#pragma once
#include <string>
#include <vector>

// Ids of the subscription videos the user has already been told about (sdmc:/3ds/IkersTube/feed_seen.txt)
// Returns how many of `ids` are new. The very first run only records them (returns 0).
int feed_seen_count_new(const std::vector<std::string> &ids);
void feed_seen_mark(const std::vector<std::string> &ids);
