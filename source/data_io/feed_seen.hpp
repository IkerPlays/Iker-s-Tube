#pragma once
#include <string>
#include <vector>
#include <set>

// Ids of the subscription videos the user has already been told about (sdmc:/3ds/IkersTube/feed_seen.txt)
// Returns how many of `ids` are new. The very first run only records them (returns 0).
int feed_seen_count_new(const std::vector<std::string> &ids);
void feed_seen_mark(const std::vector<std::string> &ids);
// Which of `ids` are not in the seen list (same rule as feed_seen_count_new, but it records nothing). Call it BEFORE feed_seen_mark.
std::set<std::string> feed_seen_new_set(const std::vector<std::string> &ids);
