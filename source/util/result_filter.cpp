#include "headers.hpp"
#include "util/result_filter.hpp"
#include "data_io/history.hpp"
#include "youtube_parser/parser.hpp"
#include <algorithm>
#include <vector>
#include <cstdio>

#define HIDDEN_PATH "sdmc:/3ds/IkersTube/hidden_channels.txt"

static std::vector<std::string> hidden_names;
static bool hidden_loaded = false;
static LightLock hidden_lock;
static bool hidden_lock_init = false;

static void hidden_prepare() {
	if (!hidden_lock_init) {
		hidden_lock_init = true;
		LightLock_Init(&hidden_lock);
	}
}
static void hidden_load_locked() {
	if (hidden_loaded) return;
	hidden_loaded = true;
	FILE *f = fopen(HIDDEN_PATH, "r");
	if (!f) return;
	char line[200];
	while (fgets(line, sizeof line, f)) {
		std::string s = line;
		while (s.size() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
		if (s != "") hidden_names.push_back(s);
	}
	fclose(f);
}
static void hidden_save_locked() {
	FILE *f = fopen(HIDDEN_PATH, "w");
	if (!f) return;
	for (auto &n : hidden_names) fprintf(f, "%s\n", n.c_str());
	fclose(f);
}

bool hidden_channel_is(const std::string &name) {
	if (name == "") return false;
	hidden_prepare();
	LightLock_Lock(&hidden_lock);
	hidden_load_locked();
	bool res = std::find(hidden_names.begin(), hidden_names.end(), name) != hidden_names.end();
	LightLock_Unlock(&hidden_lock);
	return res;
}
void hidden_channel_set(const std::string &name, bool hidden) {
	if (name == "" || name.find('\n') != std::string::npos) return;
	hidden_prepare();
	LightLock_Lock(&hidden_lock);
	hidden_load_locked();
	auto it = std::find(hidden_names.begin(), hidden_names.end(), name);
	if (hidden && it == hidden_names.end()) hidden_names.push_back(name);
	if (!hidden && it != hidden_names.end()) hidden_names.erase(it);
	hidden_save_locked();
	LightLock_Unlock(&hidden_lock);
}
int hidden_channel_count() {
	hidden_prepare();
	LightLock_Lock(&hidden_lock);
	hidden_load_locked();
	int n = (int)hidden_names.size();
	LightLock_Unlock(&hidden_lock);
	return n;
}
void hidden_channels_clear() {
	hidden_prepare();
	LightLock_Lock(&hidden_lock);
	hidden_load_locked();
	hidden_names.clear();
	hidden_save_locked();
	LightLock_Unlock(&hidden_lock);
}

bool result_video_hidden(const std::string &url, const std::string &author) {
	if (var_hide_shorts && url.find("/shorts/") != std::string::npos) return true;
	if (var_hide_watched) {
		std::string id = youtube_get_video_id_by_url(url);
		if (id != "" && history_has_id(id)) return true;
	}
	if (author != "" && hidden_channel_is(author)) return true;
	return false;
}
