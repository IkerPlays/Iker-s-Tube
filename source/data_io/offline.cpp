#include "data_io/offline.hpp"
#include "system/libctru_wrapper.hpp"
#include <cstdio>
#include <sys/stat.h>

#define OFFLINE_DIR "sdmc:/3ds/IkersTube/offline"
#define OFFLINE_INDEX OFFLINE_DIR "/index.txt"
#define OFFLINE_INDEX_TMP OFFLINE_DIR "/index.tmp"

static Mutex offline_lock;

static std::string clean(std::string s) {
	for (auto &c : s) {
		if (c == '\t' || c == '\n' || c == '\r') c = ' ';
	}
	return s;
}

std::string offline_file_path(const std::string &id) {
	return std::string(OFFLINE_DIR) + "/" + id + ".m4a";
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
		items.push_back({s.substr(0, t1), s.substr(t1 + 1, t2 - t1 - 1), s.substr(t2 + 1)});
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
	for (auto &i : items) fprintf(f, "%s\t%s\t%s\n", clean(i.id).c_str(), clean(i.title).c_str(), clean(i.author).c_str());
	fclose(f);
	remove(OFFLINE_INDEX);
	rename(OFFLINE_INDEX_TMP, OFFLINE_INDEX);
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
	offline_lock.unlock();
}
