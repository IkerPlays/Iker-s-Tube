#pragma once
#include <cstdio>
#include <unistd.h>
#include <string>

// Files saved as "write .tmp, remove the old one, rename" can be left with only the .tmp if the app is killed between the
// last two steps. This opens `path`; if it is missing but its finished .tmp exists, the .tmp becomes the file first.
inline FILE *fopen_or_recover(const char *path, const char *tmp, const char *mode) {
	FILE *f = fopen(path, mode);
	if (f) return f;
	if (access(tmp, F_OK) == 0 && rename(tmp, path) == 0) return fopen(path, mode);
	return NULL;
}

// Writes `content` to `path` through `path`.tmp : a full card, a cut or a power loss in the middle never leaves a half written
// file (the old one stays until the new one is complete; fopen_or_recover() finishes a swap that was cut between the last steps).
inline bool save_text_atomic(const char *path, const std::string &content) {
	const std::string tmp = std::string(path) + ".tmp";
	FILE *f = fopen(tmp.c_str(), "w");
	if (!f) return false;
	bool good = fwrite(content.data(), 1, content.size(), f) == content.size() && !ferror(f);
	if (fclose(f) != 0) good = false;
	if (!good) {
		remove(tmp.c_str());
		return false;
	}
	remove(path);
	return rename(tmp.c_str(), path) == 0;
}
