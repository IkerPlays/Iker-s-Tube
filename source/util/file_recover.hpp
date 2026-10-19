#pragma once
#include <cstdio>
#include <unistd.h>

// Files saved as "write .tmp, remove the old one, rename" can be left with only the .tmp if the app is killed between the
// last two steps. This opens `path`; if it is missing but its finished .tmp exists, the .tmp becomes the file first.
inline FILE *fopen_or_recover(const char *path, const char *tmp, const char *mode) {
	FILE *f = fopen(path, mode);
	if (f) return f;
	if (access(tmp, F_OK) == 0 && rename(tmp, path) == 0) return fopen(path, mode);
	return NULL;
}
