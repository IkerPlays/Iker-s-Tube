#include "data_io/search_history.hpp"
#include "system/libctru_wrapper.hpp"
#include <cctype>
#include <cstdio>
#include <sys/stat.h>

#define SEARCH_HISTORY_PATH "sdmc:/3ds/IkersTube/search_history.txt"
#define SEARCH_HISTORY_TMP_PATH "sdmc:/3ds/IkersTube/search_history.tmp"
#define SEARCH_HISTORY_MAX 15

static Mutex history_lock;
static std::vector<std::string> history;
static bool loaded = false;

static void ensure_loaded_locked() {
	if (loaded) {
		return;
	}
	loaded = true;
	FILE *f = fopen(SEARCH_HISTORY_PATH, "r");
	if (!f) {
		return;
	}
	char line[300];
	while (fgets(line, sizeof line, f)) {
		std::string s = line;
		while (s.size() && (s.back() == '\n' || s.back() == '\r')) {
			s.pop_back();
		}
		if (s.size() && history.size() < SEARCH_HISTORY_MAX) {
			history.push_back(s);
		}
	}
	fclose(f);
}

static void save_locked() {
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	FILE *f = fopen(SEARCH_HISTORY_TMP_PATH, "w");
	if (!f) {
		return;
	}
	for (auto &s : history) {
		fprintf(f, "%s\n", s.c_str());
	}
	fclose(f);
	remove(SEARCH_HISTORY_PATH);
	rename(SEARCH_HISTORY_TMP_PATH, SEARCH_HISTORY_PATH);
}

static bool equals_ignore_case(const std::string &a, const std::string &b) {
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

void search_history_add(const std::string &word) {
	if (word.empty() || word.find('\n') != std::string::npos) {
		return;
	}
	history_lock.lock();
	ensure_loaded_locked();
	for (size_t i = 0; i < history.size(); i++) {
		if (equals_ignore_case(history[i], word)) {
			history.erase(history.begin() + i);
			break;
		}
	}
	history.insert(history.begin(), word);
	if (history.size() > SEARCH_HISTORY_MAX) {
		history.resize(SEARCH_HISTORY_MAX);
	}
	save_locked();
	history_lock.unlock();
}

std::vector<std::string> search_history_get() {
	history_lock.lock();
	ensure_loaded_locked();
	auto res = history;
	history_lock.unlock();
	return res;
}

void search_history_clear() {
	history_lock.lock();
	history.clear();
	loaded = true;
	save_locked();
	history_lock.unlock();
}
