#include "data_io/backup.hpp"
#include <cstdio>
#include <sys/stat.h>
#include <string>

#define MAIN_DIR "sdmc:/3ds/IkersTube/"
#define BACKUP_DIR "sdmc:/3ds/IkersTube/backup/"

static const char *backup_files[] = {"favorites.txt",    "watch_later.txt", "bookmarks.txt",     "playlists.txt",
                                     "search_history.txt", "resume.txt",      "subscription.json", "watch_history.json",
                                     "settings.txt",       "queue.txt",       "audio_lang.txt",    "caption_lang.txt"};

static bool copy_file(const std::string &from, const std::string &to) {
	FILE *in = fopen(from.c_str(), "rb");
	if (!in) {
		return false;
	}
	FILE *out = fopen(to.c_str(), "wb");
	if (!out) {
		fclose(in);
		return false;
	}
	char buf[4096];
	size_t n;
	bool ok = true;
	while ((n = fread(buf, 1, sizeof buf, in)) > 0) {
		if (fwrite(buf, 1, n, out) != n) {
			ok = false;
			break;
		}
	}
	fclose(in);
	fclose(out);
	return ok;
}

static int copy_all(const std::string &from_dir, const std::string &to_dir) {
	int copied = 0;
	for (auto name : backup_files) {
		if (copy_file(from_dir + name, to_dir + name)) {
			copied++;
		}
	}
	return copied;
}

int backup_export() {
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	if (mkdir("sdmc:/3ds/IkersTube/backup", 0777) != 0) {
		struct stat st;
		if (stat("sdmc:/3ds/IkersTube/backup", &st) != 0) {
			return -1;
		}
	}
	return copy_all(MAIN_DIR, BACKUP_DIR);
}

int backup_import() {
	struct stat st;
	if (stat("sdmc:/3ds/IkersTube/backup", &st) != 0) {
		return -1;
	}
	return copy_all(BACKUP_DIR, MAIN_DIR);
}
