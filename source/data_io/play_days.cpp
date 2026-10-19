#include "data_io/play_days.hpp"
#include "data_io/settings.hpp"
#include "system/libctru_wrapper.hpp"
#include "util/file_recover.hpp"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sys/stat.h>

#define PLAY_DAYS_PATH "sdmc:/3ds/IkersTube/play_days.txt"
#define PLAY_DAYS_TMP_PATH "sdmc:/3ds/IkersTube/play_days.tmp"
#define PLAY_DAYS_KEEP 60

static Mutex days_lock;
static std::map<int, int> days;
static bool loaded = false;

static void load_locked() {
	if (loaded) return;
	loaded = true;
	FILE *f = fopen_or_recover(PLAY_DAYS_PATH, PLAY_DAYS_TMP_PATH, "r");
	if (!f) return;
	char line[64];
	while (fgets(line, sizeof line, f)) {
		int day = 0, seconds = 0;
		if (sscanf(line, "%d\t%d", &day, &seconds) == 2 && day > 0 && seconds > 0 && seconds <= 86400) days[day] = seconds;
	}
	fclose(f);
}

void play_days_record(int day, int seconds) {
	if (day <= 0 || seconds <= 0) return;
	if (seconds > 86400) seconds = 86400;
	days_lock.lock();
	load_locked();
	auto it = days.find(day);
	if (it == days.end() || it->second < seconds) {
		days[day] = seconds;
		while ((int)days.size() > PLAY_DAYS_KEEP) days.erase(days.begin());
		mkdir("sdmc:/3ds", 0777);
		mkdir("sdmc:/3ds/IkersTube", 0777);
		FILE *f = fopen(PLAY_DAYS_TMP_PATH, "w");
		if (f) {
			for (auto &p : days) fprintf(f, "%d\t%d\n", p.first, p.second);
			bool good = !ferror(f);
			if (fclose(f) != 0) good = false;
			if (good) {
				remove(PLAY_DAYS_PATH);
				rename(PLAY_DAYS_TMP_PATH, PLAY_DAYS_PATH);
			} else {
				remove(PLAY_DAYS_TMP_PATH);
			}
		}
	}
	days_lock.unlock();
}

std::vector<std::pair<int, int>> play_days_last(int count) {
	std::vector<std::pair<int, int>> result;
	int today = local_day_number();
	days_lock.lock();
	load_locked();
	for (int day = today - count + 1; day <= today; day++) {
		auto it = days.find(day);
		result.push_back({day, it == days.end() ? 0 : it->second});
	}
	days_lock.unlock();
	return result;
}
