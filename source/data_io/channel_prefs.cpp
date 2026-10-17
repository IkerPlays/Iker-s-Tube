#include "data_io/channel_prefs.hpp"
#include "system/libctru_wrapper.hpp"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sys/stat.h>

#define SPEED_PATH "sdmc:/3ds/IkersTube/channel_speed.txt"
#define SPEED_TMP_PATH "sdmc:/3ds/IkersTube/channel_speed.tmp"

static Mutex prefs_lock;
static std::map<std::string, double> speeds;
static bool loaded = false;

static void load_locked() {
	if (loaded) return;
	loaded = true;
	FILE *f = fopen(SPEED_PATH, "r");
	if (!f) return;
	char line[256];
	while (fgets(line, sizeof line, f)) {
		std::string s = line;
		size_t tab = s.find('\t');
		if (tab == std::string::npos) continue;
		speeds[s.substr(0, tab)] = atof(s.c_str() + tab + 1);
	}
	fclose(f);
}

double channel_speed_get(const std::string &channel_id) {
	if (channel_id == "") return 0;
	prefs_lock.lock();
	load_locked();
	auto it = speeds.find(channel_id);
	double res = it == speeds.end() ? 0 : it->second;
	prefs_lock.unlock();
	return res;
}

void channel_speed_set(const std::string &channel_id, double speed) {
	if (channel_id == "") return;
	prefs_lock.lock();
	load_locked();
	if (speed > 0.99 && speed < 1.01) {
		speeds.erase(channel_id);
	} else {
		speeds[channel_id] = speed;
	}
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	FILE *f = fopen(SPEED_TMP_PATH, "w");
	if (f) {
		for (auto &p : speeds) fprintf(f, "%s\t%.2f\n", p.first.c_str(), p.second);
		fclose(f);
		remove(SPEED_PATH);
		rename(SPEED_TMP_PATH, SPEED_PATH);
	}
	prefs_lock.unlock();
}
