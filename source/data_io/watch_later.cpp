#include "data_io/watch_later.hpp"
#include "oauth/oauth.hpp"
#include "util/async_task.hpp"
#include "system/libctru_wrapper.hpp"
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#define WATCH_LATER_PATH "sdmc:/3ds/IkersTube/watch_later.txt"
#define WATCH_LATER_TMP_PATH "sdmc:/3ds/IkersTube/watch_later.tmp"

static Mutex watch_later_lock;

struct SyncArg {
	std::string id;
	bool add;
};
static void sync_task(void *arg) {
	SyncArg *p = (SyncArg *)arg;
	watch_later_sync_remote(p->id, p->add);
	delete p;
}
static void sync_remote(const std::string &id, bool add) {
	if (OAuth::is_authenticated()) {
		queue_async_task(sync_task, new SyncArg{id, add});
	}
}
static std::vector<FavoriteVideo> watch_later_list;

static std::string clean_field(std::string s) {
	for (auto &c : s) {
		if (c == '\t' || c == '\n' || c == '\r') {
			c = ' ';
		}
	}
	return s;
}

static void save_locked() {
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	FILE *f = fopen(WATCH_LATER_TMP_PATH, "w");
	if (!f) {
		return;
	}
	for (auto &v : watch_later_list) {
		fprintf(f, "%s\t%s\t%s\t%s\n", clean_field(v.id).c_str(), clean_field(v.title).c_str(),
		        clean_field(v.author).c_str(), clean_field(v.length_text).c_str());
	}
	fclose(f);
	remove(WATCH_LATER_PATH);
	rename(WATCH_LATER_TMP_PATH, WATCH_LATER_PATH);
}

void watch_later_load() {
	watch_later_lock.lock();
	watch_later_list.clear();
	FILE *f = fopen(WATCH_LATER_PATH, "r");
	if (f) {
		char line[1024];
		while (fgets(line, sizeof line, f)) {
			std::string s = line;
			while (s.size() && (s.back() == '\n' || s.back() == '\r')) {
				s.pop_back();
			}
			std::vector<std::string> parts;
			size_t start = 0;
			while (true) {
				size_t tab = s.find('\t', start);
				parts.push_back(s.substr(start, tab == std::string::npos ? std::string::npos : tab - start));
				if (tab == std::string::npos) {
					break;
				}
				start = tab + 1;
			}
			if (parts.size() >= 2 && parts[0].size()) {
				FavoriteVideo v;
				v.id = parts[0];
				v.title = parts[1];
				v.author = parts.size() > 2 ? parts[2] : "";
				v.length_text = parts.size() > 3 ? parts[3] : "";
				watch_later_list.push_back(v);
			}
		}
		fclose(f);
	}
	watch_later_lock.unlock();
}

bool watch_later_contains(const std::string &video_id) {
	bool res = false;
	watch_later_lock.lock();
	for (auto &v : watch_later_list) {
		if (v.id == video_id) {
			res = true;
			break;
		}
	}
	watch_later_lock.unlock();
	return res;
}

bool watch_later_toggle(const FavoriteVideo &video) {
	bool added = true;
	watch_later_lock.lock();
	for (size_t i = 0; i < watch_later_list.size(); i++) {
		if (watch_later_list[i].id == video.id) {
			watch_later_list.erase(watch_later_list.begin() + i);
			added = false;
			break;
		}
	}
	if (added) {
		watch_later_list.insert(watch_later_list.begin(), video); // newest first
	}
	save_locked();
	watch_later_lock.unlock();
	sync_remote(video.id, added);
	return added;
}

void watch_later_remove(const std::string &video_id) {
	watch_later_lock.lock();
	for (size_t i = 0; i < watch_later_list.size(); i++) {
		if (watch_later_list[i].id == video_id) {
			watch_later_list.erase(watch_later_list.begin() + i);
			break;
		}
	}
	save_locked();
	watch_later_lock.unlock();
	sync_remote(video_id, false);
}

std::vector<FavoriteVideo> watch_later_get() {
	watch_later_lock.lock();
	auto res = watch_later_list;
	watch_later_lock.unlock();
	return res;
}
