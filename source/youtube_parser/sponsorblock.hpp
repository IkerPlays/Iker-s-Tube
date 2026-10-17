#pragma once
#include <string>
#include <utility>
#include <vector>

// SponsorBlock (https://sponsor.ajay.app) : the parts of a video that the community marked as sponsors, self promotion, intros...
// Only the first 4 characters of the SHA-256 of the video id are sent (the privacy mode of the API), never the id.
// level 1 : sponsors only, level 2 : also self promotion, interaction reminders, intros and outros.
// Blocks on the network : call it from a worker thread. Returns false when the request failed (no segments is still true).
bool sponsorblock_load(const std::string &video_id, int level, std::vector<std::pair<double, double>> &segments);
