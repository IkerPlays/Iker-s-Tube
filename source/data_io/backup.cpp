#include "data_io/backup.hpp"
#include <cstdio>
#include <sys/stat.h>
#include <string>
#include <unistd.h>
#include <time.h>

#ifndef IKT_VERSION
#define IKT_VERSION "?"
#endif

#define MAIN_DIR "sdmc:/3ds/IkersTube/"
#define BACKUP_DIR "sdmc:/3ds/IkersTube/backup/"

static const char *backup_files[] = {"favorites.txt",    "watch_later.txt", "bookmarks.txt",     "playlists.txt",
                                     "search_history.txt", "resume.txt",      "subscription.json", "watch_history.json",
                                     "settings.txt",       "queue.txt",       "audio_lang.txt",    "caption_lang.txt",  "hidden_channels.txt", "pinned_channels.txt", "recent_channels.txt", "channel_volume.txt", "channel_skip.txt", "channel_speed.txt", "badges.txt", "play_days.txt", "last_played.txt", "feed_seen.txt"};

static bool copy_file(const std::string &from, const std::string &to) {
	FILE *in = fopen(from.c_str(), "rb");
	if (!in) {
		return false;
	}
	// copied to a .tmp first: a full SD card or a write error leaves the old copy of the file as it was
	const std::string tmp = to + ".tmp";
	FILE *out = fopen(tmp.c_str(), "wb");
	if (!out) {
		fclose(in);
		return false;
	}
	char buf[4096];
	size_t n;
	size_t total = 0;
	bool ok = true;
	while ((n = fread(buf, 1, sizeof buf, in)) > 0) {
		if (fwrite(buf, 1, n, out) != n) {
			ok = false;
			break;
		}
		total += n;
	}
	fclose(in);
	if (fclose(out) != 0) ok = false;
	if (!ok || total == 0) { // an empty source (a damaged file) never replaces a good one
		remove(tmp.c_str());
		return false;
	}
	if (to.find("/backup/") != std::string::npos && access(to.c_str(), F_OK) == 0) { // the backup before this one stays as *.prev (a damaged file must not erase the last good copy)
		std::string prev = to + ".prev";
		remove(prev.c_str());
		rename(to.c_str(), prev.c_str());
	}
	remove(to.c_str());
	return rename(tmp.c_str(), to.c_str()) == 0;
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
	int copied = copy_all(MAIN_DIR, BACKUP_DIR);
	if (copied > 0) { // says which version and when (used by the Settings screen)
		if (FILE *info = fopen(BACKUP_DIR "backup_info.txt", "w")) {
			fprintf(info, "version=%s\ntime=%lld\nfiles=%d\n", IKT_VERSION, (long long)time(NULL), copied);
			fclose(info);
		}
	}
	return copied;
}

long long backup_time() {
	struct stat st;
	if (stat(BACKUP_DIR "backup_info.txt", &st) == 0 && st.st_size > 0) return (long long)st.st_mtime;
	if (stat(BACKUP_DIR "settings.txt", &st) == 0 && st.st_size > 0) return (long long)st.st_mtime;
	return 0;
}

#define RESTORE_FLAG "sdmc:/3ds/IkersTube/restore_backup.flag"

// The files are not copied right away: the running app still holds the old data and would write it over the restored files
// (on the next save or when it exits). The restore is done at the next start, before anything is loaded.
int backup_import() {
	struct stat st;
	if (stat("sdmc:/3ds/IkersTube/backup", &st) != 0) {
		return -1;
	}
	int available = 0;
	for (auto name : backup_files) {
		struct stat file_stat;
		if (stat((std::string(BACKUP_DIR) + name).c_str(), &file_stat) == 0 && file_stat.st_size > 0) available++;
	}
	if (available > 0) {
		if (FILE *flag = fopen(RESTORE_FLAG, "w")) {
			fputs("1\n", flag);
			fclose(flag);
		}
	}
	return available;
}

void backup_apply_pending() {
	if (access(RESTORE_FLAG, F_OK) != 0) return;
	copy_all(BACKUP_DIR, MAIN_DIR);
	remove(RESTORE_FLAG);
}
