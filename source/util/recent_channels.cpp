#include "headers.hpp"
#include "util/recent_channels.hpp"
#include <cstdio>

#define RECENT_PATH "sdmc:/3ds/IkersTube/recent_channels.txt"
#define RECENT_MAX 8

static LightLock recent_lock;
static struct RecentLockInit {
	RecentLockInit() { LightLock_Init(&recent_lock); }
} recent_lock_init_object;
static std::vector<RecentChannel> recent;
static bool recent_loaded = false;

static void load_locked() {
	if (recent_loaded) return;
	recent_loaded = true;
	FILE *f = fopen(RECENT_PATH, "r");
	if (!f) return;
	char line[700];
	while (fgets(line, sizeof line, f)) {
		std::string s = line;
		while (s.size() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
		size_t tab = s.find('\t');
		if (tab != std::string::npos && tab > 0) recent.push_back({s.substr(0, tab), s.substr(tab + 1)});
	}
	fclose(f);
}

void recent_channel_add(const std::string &name, const std::string &url) {
	if (var_incognito || name == "" || url == "" || name.find('\t') != std::string::npos || name.find('\n') != std::string::npos) return;
	LightLock_Lock(&recent_lock);
	load_locked();
	for (size_t i = 0; i < recent.size(); i++) {
		if (recent[i].url == url) {
			recent.erase(recent.begin() + i);
			break;
		}
	}
	recent.insert(recent.begin(), {name, url});
	if (recent.size() > RECENT_MAX) recent.resize(RECENT_MAX);
	FILE *f = fopen(RECENT_PATH, "w");
	if (f) {
		for (auto &c : recent) fprintf(f, "%s\t%s\n", c.name.c_str(), c.url.c_str());
		fclose(f);
	}
	LightLock_Unlock(&recent_lock);
}

std::vector<RecentChannel> recent_channels_get() {
	LightLock_Lock(&recent_lock);
	load_locked();
	std::vector<RecentChannel> res = recent;
	LightLock_Unlock(&recent_lock);
	return res;
}
