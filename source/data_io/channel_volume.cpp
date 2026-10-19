#include "util/file_recover.hpp"
#include "data_io/channel_volume.hpp"
#include "system/libctru_wrapper.hpp"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sys/stat.h>

#define VOLUME_PATH "sdmc:/3ds/IkersTube/channel_volume.txt"
#define VOLUME_TMP_PATH "sdmc:/3ds/IkersTube/channel_volume.tmp"

static Mutex prefs_lock;
static std::map<std::string, double> volumes;
static bool loaded = false;

static void load_locked() {
	if (loaded) return;
	loaded = true;
	FILE *f = fopen_or_recover(VOLUME_PATH, VOLUME_TMP_PATH, "r");
	if (!f) return;
	char line[256];
	while (fgets(line, sizeof line, f)) {
		std::string s = line;
		size_t tab = s.find('\t');
		if (tab == std::string::npos) continue;
		volumes[s.substr(0, tab)] = atof(s.c_str() + tab + 1);
	}
	fclose(f);
}

double channel_volume_get(const std::string &channel_id) {
	if (channel_id == "") return 0;
	prefs_lock.lock();
	load_locked();
	auto it = volumes.find(channel_id);
	double res = it == volumes.end() ? 0 : it->second;
	prefs_lock.unlock();
	return res;
}

void channel_volume_set(const std::string &channel_id, double volume) {
	if (channel_id == "") return;
	prefs_lock.lock();
	load_locked();
	if (volume > 0.99 && volume < 1.01) {
		volumes.erase(channel_id);
	} else {
		volumes[channel_id] = volume;
	}
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	FILE *f = fopen(VOLUME_TMP_PATH, "w");
	if (f) {
		for (auto &p : volumes) fprintf(f, "%s\t%.2f\n", p.first.c_str(), p.second);
		bool good = !ferror(f);
		if (fclose(f) != 0) good = false;
		if (good) {
			remove(VOLUME_PATH);
			rename(VOLUME_TMP_PATH, VOLUME_PATH);
		} else {
			remove(VOLUME_TMP_PATH);
		}
	}
	prefs_lock.unlock();
}
