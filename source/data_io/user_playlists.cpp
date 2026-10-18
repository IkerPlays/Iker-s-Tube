#include "data_io/user_playlists.hpp"
#include "system/libctru_wrapper.hpp"
#include <cctype>
#include <cstdio>
#include <sys/stat.h>

#define PLAYLISTS_PATH "sdmc:/3ds/IkersTube/playlists.txt"
#define PLAYLISTS_TMP_PATH "sdmc:/3ds/IkersTube/playlists.tmp"
#define PLAYLISTS_MAX 30
#define PLAYLIST_VIDEOS_MAX 200

static Mutex playlists_lock;
static std::vector<UserPlaylist> playlists;
static bool loaded = false;

static std::string clean(std::string s) {
	for (auto &c : s) {
		if (c == '\t' || c == '\n' || c == '\r') {
			c = ' ';
		}
	}
	while (s.size() && s.front() == ' ') {
		s.erase(s.begin());
	}
	while (s.size() && s.back() == ' ') {
		s.pop_back();
	}
	if (s.size() > 40) { // keep the cut on a UTF-8 character boundary
		size_t n = 40;
		while (n && ((unsigned char)s[n] & 0xC0) == 0x80) {
			n--;
		}
		s.resize(n);
	}
	return s;
}

static void ensure_loaded_locked() {
	if (loaded) {
		return;
	}
	loaded = true;
	FILE *f = fopen(PLAYLISTS_PATH, "r");
	if (!f) {
		return;
	}
	char line[1024];
	while (fgets(line, sizeof line, f)) {
		std::string s = line;
		while (s.size() && (s.back() == '\n' || s.back() == '\r')) {
			s.pop_back();
		}
		if (!s.size()) {
			continue;
		}
		if (s[0] == '#') {
			if (playlists.size() < PLAYLISTS_MAX) {
				UserPlaylist p;
				p.name = s.substr(1);
				playlists.push_back(p);
			}
			continue;
		}
		if (!playlists.size()) {
			continue;
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
		if (parts.size() >= 2 && parts[0].size() && playlists.back().videos.size() < PLAYLIST_VIDEOS_MAX) {
			FavoriteVideo v;
			v.id = parts[0];
			v.title = parts[1];
			v.author = parts.size() > 2 ? parts[2] : "";
			v.length_text = parts.size() > 3 ? parts[3] : "";
			playlists.back().videos.push_back(v);
		}
	}
	fclose(f);
}

static void save_locked() {
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	FILE *f = fopen(PLAYLISTS_TMP_PATH, "w");
	if (!f) {
		return;
	}
	for (auto &p : playlists) {
		fprintf(f, "#%s\n", clean(p.name).c_str());
		for (auto &v : p.videos) {
			fprintf(f, "%s\t%s\t%s\t%s\n", clean(v.id).c_str(), clean(v.title).c_str(), clean(v.author).c_str(), clean(v.length_text).c_str());
		}
	}
	fclose(f);
	remove(PLAYLISTS_PATH);
	rename(PLAYLISTS_TMP_PATH, PLAYLISTS_PATH);
}

static bool same_name(const std::string &a, const std::string &b) {
	if (a.size() != b.size()) {
		return false;
	}
	for (size_t i = 0; i < a.size(); i++) {
		if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) {
			return false;
		}
	}
	return true;
}

std::vector<UserPlaylist> playlists_get() {
	playlists_lock.lock();
	ensure_loaded_locked();
	auto res = playlists;
	playlists_lock.unlock();
	return res;
}

bool playlist_create(const std::string &name_in) {
	std::string name = clean(name_in);
	if (name.empty() || name[0] == '#') {
		return false;
	}
	bool ok = true;
	playlists_lock.lock();
	ensure_loaded_locked();
	if (playlists.size() >= PLAYLISTS_MAX) {
		ok = false;
	}
	for (auto &p : playlists) {
		if (same_name(p.name, name)) {
			ok = false;
		}
	}
	if (ok) {
		UserPlaylist p;
		p.name = name;
		playlists.push_back(p);
		save_locked();
	}
	playlists_lock.unlock();
	return ok;
}

void playlist_delete(int index) {
	playlists_lock.lock();
	ensure_loaded_locked();
	if (index >= 0 && index < (int)playlists.size()) {
		playlists.erase(playlists.begin() + index);
		save_locked();
	}
	playlists_lock.unlock();
}

bool playlist_add(int index, const FavoriteVideo &video) {
	bool ok = false;
	playlists_lock.lock();
	ensure_loaded_locked();
	if (index >= 0 && index < (int)playlists.size() && video.id.size()) {
		auto &videos = playlists[index].videos;
		bool exists = false;
		for (auto &v : videos) {
			if (v.id == video.id) {
				exists = true;
			}
		}
		if (!exists && videos.size() < PLAYLIST_VIDEOS_MAX) {
			videos.push_back(video);
			save_locked();
			ok = true;
		}
	}
	playlists_lock.unlock();
	return ok;
}

void playlist_remove(int index, const std::string &video_id) {
	playlists_lock.lock();
	ensure_loaded_locked();
	if (index >= 0 && index < (int)playlists.size()) {
		auto &videos = playlists[index].videos;
		for (size_t i = 0; i < videos.size(); i++) {
			if (videos[i].id == video_id) {
				videos.erase(videos.begin() + i);
				save_locked();
				break;
			}
		}
	}
	playlists_lock.unlock();
}

bool playlist_rename(int index, const std::string &name_in) {
	std::string name = clean(name_in);
	if (name.empty() || name[0] == '#') return false;
	bool ok = true;
	playlists_lock.lock();
	ensure_loaded_locked();
	if (index < 0 || index >= (int)playlists.size()) ok = false;
	for (int i = 0; ok && i < (int)playlists.size(); i++) {
		if (i != index && same_name(playlists[i].name, name)) ok = false;
	}
	if (ok) {
		playlists[index].name = name;
		save_locked();
	}
	playlists_lock.unlock();
	return ok;
}
