#include "headers.hpp"
#include "data_io/last_played.hpp"
#include <cstdio>
#include <sys/stat.h>

#define LAST_PLAYED_PATH "sdmc:/3ds/IkersTube/last_played.txt"

void last_played_set(const std::string &id, const std::string &title) {
	if (var_incognito) return; // incognito : nothing is remembered
	if (id == "") return;
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	FILE *f = fopen(LAST_PLAYED_PATH, "w");
	if (!f) return;
	std::string clean = title;
	for (auto &c : clean) if (c == '\n' || c == '\r') c = ' ';
	fprintf(f, "%s\n%s\n", id.c_str(), clean.c_str());
	fclose(f);
}

bool last_played_get(std::string &id, std::string &title) {
	FILE *f = fopen(LAST_PLAYED_PATH, "r");
	if (!f) return false;
	char a[64] = {0}, b[512] = {0};
	bool ok = fgets(a, sizeof a, f) != NULL;
	if (ok) fgets(b, sizeof b, f);
	fclose(f);
	id = a, title = b;
	while (id.size() && (id.back() == '\n' || id.back() == '\r')) id.pop_back();
	while (title.size() && (title.back() == '\n' || title.back() == '\r')) title.pop_back();
	return ok && id != "";
}
