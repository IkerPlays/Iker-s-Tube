#include "util/file_recover.hpp"
#include "data_io/favorites.hpp"
#include "system/libctru_wrapper.hpp"
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#define FAVORITES_PATH "sdmc:/3ds/IkersTube/favorites.txt"
#define FAVORITES_TMP_PATH "sdmc:/3ds/IkersTube/favorites.tmp"

static Mutex favorites_lock;
static std::vector<FavoriteVideo> favorites;

static std::string clean_field(std::string s) {
	for (auto &c : s) {
		if (c == '\t' || c == '\n' || c == '\r') {
			c = ' ';
		}
	}
	if (s.size() > 200) { // a line of the file must fit the 1023 bytes read back by fgets : a long title is cut (on a letter boundary)
		s.resize(200);
		while (!s.empty() && ((unsigned char)s.back() & 0xC0) == 0x80) s.pop_back(); // (a cut in the middle of a letter)
		if (!s.empty() && ((unsigned char)s.back() & 0x80)) s.pop_back();
	}
	return s;
}

static void save_locked() {
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	FILE *f = fopen(FAVORITES_TMP_PATH, "w");
	if (!f) {
		return;
	}
	for (auto &v : favorites) {
		fprintf(f, "%s\t%s\t%s\t%s\t%s\n", clean_field(v.id).c_str(), clean_field(v.title).c_str(),
		        clean_field(v.author).c_str(), clean_field(v.length_text).c_str(), clean_field(v.tag).c_str());
	}
	bool good = !ferror(f);
	if (fclose(f) != 0) good = false;
	if (!good) { // e.g. the SD card is full: the old file stays
		remove(FAVORITES_TMP_PATH);
		return;
	}
	remove(FAVORITES_PATH);
	rename(FAVORITES_TMP_PATH, FAVORITES_PATH);
}

void favorites_load() {
	favorites_lock.lock();
	favorites.clear();
	FILE *f = fopen_or_recover(FAVORITES_PATH, FAVORITES_TMP_PATH, "r");
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
				v.tag = parts.size() > 4 ? parts[4] : "";
				bool duplicate = false; // (a hand-edited or restored file may repeat an id : the second one is dropped)
				for (auto &o : favorites) duplicate = duplicate || o.id == v.id;
				if (!duplicate && favorites.size() < 1000) favorites.push_back(v);
			}
		}
		fclose(f);
	}
	favorites_lock.unlock();
}

bool favorites_contains(const std::string &video_id) {
	bool res = false;
	favorites_lock.lock();
	for (auto &v : favorites) {
		if (v.id == video_id) {
			res = true;
			break;
		}
	}
	favorites_lock.unlock();
	return res;
}

bool favorites_toggle(const FavoriteVideo &video) {
	bool added = true;
	favorites_lock.lock();
	for (size_t i = 0; i < favorites.size(); i++) {
		if (favorites[i].id == video.id) {
			favorites.erase(favorites.begin() + i);
			added = false;
			break;
		}
	}
	if (added) {
		favorites.insert(favorites.begin(), video); // newest first
		if (favorites.size() > 1000) favorites.resize(1000); // (the whole list is rewritten at every save : a cap keeps that quick; the oldest ones go)
	}
	save_locked();
	favorites_lock.unlock();
	return added;
}

void favorites_remove(const std::string &video_id) {
	favorites_lock.lock();
	for (size_t i = 0; i < favorites.size(); i++) {
		if (favorites[i].id == video_id) {
			favorites.erase(favorites.begin() + i);
			break;
		}
	}
	save_locked();
	favorites_lock.unlock();
}

int favorites_remove_many(const std::vector<std::string> &video_ids) {
	int removed = 0;
	favorites_lock.lock();
	for (auto &id : video_ids) {
		for (size_t i = 0; i < favorites.size(); i++) {
			if (favorites[i].id == id) {
				favorites.erase(favorites.begin() + i);
				removed++;
				break;
			}
		}
	}
	if (removed) save_locked();
	favorites_lock.unlock();
	return removed;
}

void favorites_clear() {
	favorites_lock.lock();
	favorites.clear();
	save_locked();
	favorites_lock.unlock();
}

std::vector<FavoriteVideo> favorites_get() {
	favorites_lock.lock();
	auto res = favorites;
	favorites_lock.unlock();
	return res;
}

void favorites_set_tag(const std::string &video_id, const std::string &tag) {
	favorites_lock.lock();
	for (auto &v : favorites) {
		if (v.id == video_id) {
			v.tag = tag;
			break;
		}
	}
	save_locked();
	favorites_lock.unlock();
}
