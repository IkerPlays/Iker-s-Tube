#include "data_io/feed_seen.hpp"
#include "system/libctru_wrapper.hpp"
#include <algorithm>
#include <cstdio>
#include <set>
#include <sys/stat.h>

#define SEEN_PATH "sdmc:/3ds/IkersTube/feed_seen.txt"
#define SEEN_MAX 600

static Mutex seen_lock;

static bool read_seen(std::vector<std::string> &out) {
	FILE *f = fopen(SEEN_PATH, "r");
	if (!f) return false;
	char line[64];
	while (fgets(line, sizeof line, f)) {
		std::string s = line;
		while (s.size() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
		if (s != "") out.push_back(s);
	}
	fclose(f);
	return true;
}

int feed_seen_count_new(const std::vector<std::string> &ids) {
	seen_lock.lock();
	std::vector<std::string> seen;
	bool existed = read_seen(seen);
	int count = 0;
	if (existed) {
		std::set<std::string> set(seen.begin(), seen.end());
		for (auto &id : ids) if (id != "" && !set.count(id)) count++;
	}
	seen_lock.unlock();
	if (!existed) feed_seen_mark(ids); // first run : nothing is "new"
	return count;
}

void feed_seen_mark(const std::vector<std::string> &ids) {
	seen_lock.lock();
	std::vector<std::string> all;
	read_seen(all);
	std::set<std::string> set(all.begin(), all.end());
	for (auto &id : ids) {
		if (id != "" && !set.count(id)) {
			all.push_back(id);
			set.insert(id);
		}
	}
	if (all.size() > SEEN_MAX) all.erase(all.begin(), all.end() - SEEN_MAX);
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	FILE *f = fopen(SEEN_PATH, "w");
	if (f) {
		for (auto &id : all) fprintf(f, "%s\n", id.c_str());
		fclose(f);
	}
	seen_lock.unlock();
}
