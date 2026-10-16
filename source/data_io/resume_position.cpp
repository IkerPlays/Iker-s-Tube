#include "data_io/resume_position.hpp"
#include "system/libctru_wrapper.hpp"
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <utility>
#include <vector>

#define RESUME_PATH "sdmc:/3ds/IkersTube/resume.txt"
#define RESUME_TMP_PATH "sdmc:/3ds/IkersTube/resume.tmp"
#define RESUME_MAX_ENTRIES 150

static Mutex resume_lock;
static std::vector<std::pair<std::string, double>> entries; // most recent first
static bool loaded = false;

static void ensure_loaded_locked() {
	if (loaded) {
		return;
	}
	loaded = true;
	FILE *f = fopen(RESUME_PATH, "r");
	if (!f) {
		return;
	}
	char line[128];
	while (fgets(line, sizeof line, f)) {
		std::string s = line;
		size_t tab = s.find('\t');
		if (tab != std::string::npos && tab > 0) {
			double sec = atof(s.c_str() + tab + 1);
			if (sec > 0) {
				entries.push_back({s.substr(0, tab), sec});
			}
		}
	}
	fclose(f);
}

double resume_get(const std::string &video_id) {
	double res = -1;
	resume_lock.lock();
	ensure_loaded_locked();
	for (auto &e : entries) {
		if (e.first == video_id) {
			res = e.second;
			break;
		}
	}
	resume_lock.unlock();
	return res;
}

void resume_set(const std::string &video_id, double seconds, double duration) {
	if (video_id.empty() || video_id.find('\t') != std::string::npos || video_id.find('\n') != std::string::npos) {
		return;
	}
	resume_lock.lock();
	ensure_loaded_locked();
	for (size_t i = 0; i < entries.size(); i++) {
		if (entries[i].first == video_id) {
			entries.erase(entries.begin() + i);
			break;
		}
	}
	bool keep = seconds >= 15 && (duration <= 0 || duration - seconds >= 15);
	if (keep) {
		entries.insert(entries.begin(), {video_id, seconds});
		if (entries.size() > RESUME_MAX_ENTRIES) {
			entries.resize(RESUME_MAX_ENTRIES);
		}
	}
	resume_lock.unlock();
}

void resume_save() {
	resume_lock.lock();
	ensure_loaded_locked();
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	FILE *f = fopen(RESUME_TMP_PATH, "w");
	if (f) {
		for (auto &e : entries) {
			fprintf(f, "%s\t%.1f\n", e.first.c_str(), e.second);
		}
		fclose(f);
		remove(RESUME_PATH);
		rename(RESUME_TMP_PATH, RESUME_PATH);
	}
	resume_lock.unlock();
}
