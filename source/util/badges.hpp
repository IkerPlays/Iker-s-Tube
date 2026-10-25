#pragma once
#include <ctime>
#include <string>
#include <vector>

// Badges unlocked by watching (shown in My Wrapped). The ones already earned are kept in sdmc:/3ds/IkersTube/badges.txt
struct Badge {
	int id;
	bool done;
	std::string name;
};
long long local_day_of(time_t t); // the number of the day in the time zone of the console (the same count for the badges and for My Wrapped)
std::vector<Badge> badges_compute(); // reads the watch history and the playing time
void badges_check_new();             // notice for each badge earned since the last check (the first run only records them)
