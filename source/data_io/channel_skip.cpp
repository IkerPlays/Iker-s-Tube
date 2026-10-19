#include "data_io/channel_skip.hpp"
#include "system/libctru_wrapper.hpp"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sys/stat.h>

#define SKIP_PATH "sdmc:/3ds/IkersTube/channel_skip.txt"
#define SKIP_TMP_PATH "sdmc:/3ds/IkersTube/channel_skip.tmp"

static Mutex prefs_lock;
static std::map<std::string, double> skips;
static bool loaded = false;

static void load_locked() {
	if (loaded) return;
	loaded = true;
	FILE *f = fopen(SKIP_PATH, "r");
	if (!f) return;
	char line[256];
	while (fgets(line, sizeof line, f)) {
		std::string s = line;
		size_t tab = s.find('\t');
		if (tab == std::string::npos) continue;
		skips[s.substr(0, tab)] = atof(s.c_str() + tab + 1);
	}
	fclose(f);
}

double channel_skip_get(const std::string &channel_id) {
	if (channel_id == "") return 0;
	prefs_lock.lock();
	load_locked();
	auto it = skips.find(channel_id);
	double res = it == skips.end() ? 0 : it->second;
	prefs_lock.unlock();
	return res;
}

void channel_skip_set(const std::string &channel_id, double seconds) {
	if (channel_id == "") return;
	prefs_lock.lock();
	load_locked();
	if (seconds < 0.5) {
		skips.erase(channel_id);
	} else {
		skips[channel_id] = seconds;
	}
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	FILE *f = fopen(SKIP_TMP_PATH, "w");
	if (f) {
		for (auto &p : skips) fprintf(f, "%s\t%.0f\n", p.first.c_str(), p.second);
		fclose(f);
		remove(SKIP_PATH);
		rename(SKIP_TMP_PATH, SKIP_PATH);
	}
	prefs_lock.unlock();
}
