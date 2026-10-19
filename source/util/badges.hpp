#pragma once
#include <string>
#include <vector>

// Badges unlocked by watching (shown in My Wrapped). The ones already earned are kept in sdmc:/3ds/IkersTube/badges.txt
struct Badge {
	int id;
	bool done;
	std::string name;
};
std::vector<Badge> badges_compute(); // reads the watch history and the playing time
void badges_check_new();             // notice for each badge earned since the last check (the first run only records them)
