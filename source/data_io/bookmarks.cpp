#include "util/file_recover.hpp"
#include "data_io/bookmarks.hpp"
#include "system/libctru_wrapper.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>

#define BOOKMARKS_PATH "sdmc:/3ds/IkersTube/bookmarks.txt"
#define BOOKMARKS_TMP_PATH "sdmc:/3ds/IkersTube/bookmarks.tmp"

struct Entry {
	std::string id;
	double time;
	std::string name;
};

static Mutex bookmarks_lock;
static std::vector<Entry> entries;
static bool loaded = false;

static void load_locked() {
	if (loaded) {
		return;
	}
	loaded = true;
	FILE *f = fopen_or_recover(BOOKMARKS_PATH, BOOKMARKS_TMP_PATH, "r");
	if (!f) {
		return;
	}
	char line[512];
	while (fgets(line, sizeof line, f)) {
		std::string s = line;
		while (s.size() && (s.back() == '\n' || s.back() == '\r')) {
			s.pop_back();
		}
		size_t t1 = s.find('\t');
		size_t t2 = t1 == std::string::npos ? t1 : s.find('\t', t1 + 1);
		if (t2 == std::string::npos) {
			continue;
		}
		double at = atof(s.substr(t1 + 1, t2 - t1 - 1).c_str());
		if (!(at >= 0 && at < 1e7)) continue; // nan / inf / negative: a damaged line (nan would break the sort)
		entries.push_back({s.substr(0, t1), at, s.substr(t2 + 1)});
	}
	fclose(f);
}

static void save_locked() {
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	FILE *f = fopen(BOOKMARKS_TMP_PATH, "w");
	if (!f) {
		return;
	}
	for (auto &e : entries) {
		std::string name = e.name;
		for (auto &c : name) {
			if (c == '\t' || c == '\n' || c == '\r') c = ' ';
		}
		fprintf(f, "%s\t%.1f\t%s\n", e.id.c_str(), e.time, name.c_str());
	}
	fclose(f);
	remove(BOOKMARKS_PATH);
	rename(BOOKMARKS_TMP_PATH, BOOKMARKS_PATH);
}

std::vector<Bookmark> bookmarks_get(const std::string &video_id) {
	std::vector<Bookmark> res;
	bookmarks_lock.lock();
	load_locked();
	for (auto &e : entries) {
		if (e.id == video_id) {
			res.push_back({e.time, e.name});
		}
	}
	bookmarks_lock.unlock();
	std::sort(res.begin(), res.end(), [](const Bookmark &a, const Bookmark &b) { return a.time < b.time; });
	return res;
}

void bookmark_add(const std::string &video_id, double time, const std::string &name) {
	bookmarks_lock.lock();
	load_locked();
	entries.push_back({video_id, time, name});
	save_locked();
	bookmarks_lock.unlock();
}

void bookmark_remove(const std::string &video_id, size_t index) {
	auto sorted = bookmarks_get(video_id);
	if (index >= sorted.size()) {
		return;
	}
	bookmarks_lock.lock();
	for (size_t i = 0; i < entries.size(); i++) {
		if (entries[i].id == video_id && entries[i].time == sorted[index].time && entries[i].name == sorted[index].name) {
			entries.erase(entries.begin() + i);
			break;
		}
	}
	save_locked();
	bookmarks_lock.unlock();
}
