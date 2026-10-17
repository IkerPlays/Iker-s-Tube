#include "data_io/offline.hpp"
#include "system/libctru_wrapper.hpp"
#include <cstdio>
#include <set>
#include <cstdlib>
#include <sys/stat.h>

#define OFFLINE_DIR "sdmc:/3ds/IkersTube/offline"
#define OFFLINE_INDEX OFFLINE_DIR "/index.txt"
#define OFFLINE_INDEX_TMP OFFLINE_DIR "/index.tmp"

static Mutex offline_lock;
static std::set<std::string> saved_ids; // cache for offline_has_id()
static bool saved_ids_valid = false;

static std::string clean(std::string s) {
	for (auto &c : s) {
		if (c == '\t' || c == '\n' || c == '\r') c = ' ';
	}
	return s;
}

std::string offline_file_path(const std::string &id) {
	return std::string(OFFLINE_DIR) + "/" + id + ".m4a";
}

std::string offline_video_path(const std::string &id) {
	return std::string(OFFLINE_DIR) + "/" + id + ".mp4";
}

static std::vector<OfflineItem> read_index() {
	std::vector<OfflineItem> items;
	FILE *f = fopen(OFFLINE_INDEX, "r");
	if (!f) return items;
	char line[1024];
	while (fgets(line, sizeof line, f)) {
		std::string s = line;
		while (s.size() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
		size_t t1 = s.find('\t');
		size_t t2 = t1 == std::string::npos ? t1 : s.find('\t', t1 + 1);
		if (t1 == std::string::npos || t2 == std::string::npos) continue;
		OfflineItem item;
		item.id = s.substr(0, t1);
		item.title = s.substr(t1 + 1, t2 - t1 - 1);
		size_t t3 = s.find('\t', t2 + 1);
		if (t3 == std::string::npos) {
			item.author = s.substr(t2 + 1);
		} else {
			item.author = s.substr(t2 + 1, t3 - t2 - 1);
			size_t t4 = s.find('\t', t3 + 1);
			item.quality = atoi(s.substr(t3 + 1, t4 == std::string::npos ? std::string::npos : t4 - t3 - 1).c_str());
			item.combined = t4 != std::string::npos && s.compare(t4 + 1, 1, "1") == 0;
		}
		items.push_back(item);
	}
	fclose(f);
	return items;
}

static void write_index(const std::vector<OfflineItem> &items) {
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	mkdir(OFFLINE_DIR, 0777);
	FILE *f = fopen(OFFLINE_INDEX_TMP, "w");
	if (!f) return;
	for (auto &i : items)
		fprintf(f, "%s\t%s\t%s\t%d\t%d\n", clean(i.id).c_str(), clean(i.title).c_str(), clean(i.author).c_str(), i.quality,
		        i.combined ? 1 : 0);
	fclose(f);
	remove(OFFLINE_INDEX);
	rename(OFFLINE_INDEX_TMP, OFFLINE_INDEX);
	saved_ids_valid = false;
}

std::vector<OfflineItem> offline_list() {
	offline_lock.lock();
	auto items = read_index();
	offline_lock.unlock();
	return items;
}

bool offline_get(const std::string &id, OfflineItem &out) {
	bool found = false;
	offline_lock.lock();
	for (auto &i : read_index()) {
		if (i.id == id) {
			out = i;
			found = true;
			break;
		}
	}
	offline_lock.unlock();
	return found;
}

void offline_add(const OfflineItem &item) {
	offline_lock.lock();
	auto items = read_index();
	for (size_t k = 0; k < items.size(); k++) {
		if (items[k].id == item.id) {
			items.erase(items.begin() + k);
			break;
		}
	}
	items.insert(items.begin(), item);
	write_index(items);
	offline_lock.unlock();
}

void offline_remove(const std::string &id) {
	offline_lock.lock();
	auto items = read_index();
	for (size_t k = 0; k < items.size(); k++) {
		if (items[k].id == id) {
			items.erase(items.begin() + k);
			break;
		}
	}
	write_index(items);
	remove(offline_file_path(id).c_str());
	remove(offline_video_path(id).c_str());
	offline_lock.unlock();
}

bool offline_has_id(const std::string &id) {
	offline_lock.lock();
	if (!saved_ids_valid) {
		saved_ids.clear();
		for (auto &i : read_index()) saved_ids.insert(i.id);
		saved_ids_valid = true;
	}
	bool res = saved_ids.count(id) > 0;
	offline_lock.unlock();
	return res;
}
