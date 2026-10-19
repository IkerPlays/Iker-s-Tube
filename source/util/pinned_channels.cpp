#include "headers.hpp"
#include "util/pinned_channels.hpp"
#include <algorithm>
#include <cstdio>
#include <vector>

#define PINNED_PATH "sdmc:/3ds/IkersTube/pinned_channels.txt"

static LightLock pinned_lock;
static struct PinnedLockInit {
	PinnedLockInit() { LightLock_Init(&pinned_lock); }
} pinned_lock_init_object;
static std::vector<std::string> pinned;
static bool pinned_loaded = false;

static void load_locked() {
	if (pinned_loaded) return;
	pinned_loaded = true;
	FILE *f = fopen(PINNED_PATH, "r");
	if (!f) return;
	char line[200];
	while (fgets(line, sizeof line, f)) {
		std::string s = line;
		while (s.size() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
		if (s != "") pinned.push_back(s);
	}
	fclose(f);
}

bool pinned_channel_is(const std::string &channel_id) {
	LightLock_Lock(&pinned_lock);
	load_locked();
	bool res = std::find(pinned.begin(), pinned.end(), channel_id) != pinned.end();
	LightLock_Unlock(&pinned_lock);
	return res;
}

void pinned_channel_set(const std::string &channel_id, bool value) {
	if (channel_id == "" || channel_id.size() > 100) return;
	LightLock_Lock(&pinned_lock);
	load_locked();
	auto it = std::find(pinned.begin(), pinned.end(), channel_id);
	if (value && it == pinned.end()) pinned.push_back(channel_id);
	if (!value && it != pinned.end()) pinned.erase(it);
	FILE *f = fopen(PINNED_PATH, "w");
	if (f) {
		for (auto &id : pinned) fprintf(f, "%s\n", id.c_str());
		fclose(f);
	}
	LightLock_Unlock(&pinned_lock);
}
