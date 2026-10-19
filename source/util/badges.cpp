#include "headers.hpp"
#include "util/badges.hpp"
#include "data_io/history.hpp"
#include "data_io/favorites.hpp"
#include "data_io/offline.hpp"
#include "data_io/subscription_util.hpp"
#include "ui/notice.hpp"
#include "variables.hpp"
#include <algorithm>
#include <cstdio>
#include <map>
#include <set>

#define BADGES_PATH "sdmc:/3ds/IkersTube/badges.txt"

std::vector<Badge> badges_compute() {
	auto history = get_valid_watch_history();
	std::map<std::string, int> counts;
	int hours[24] = {0};
	std::set<long long> days;
	int total_views = 0;
	for (auto &video : history) {
		if (video.author_name != "") counts[video.author_name] += std::max(1, video.my_view_count);
		total_views += std::max(1, video.my_view_count);
		time_t t = video.last_watch_time;
		struct tm *local = localtime(&t);
		if (local) hours[local->tm_hour]++;
		days.insert((long long)video.last_watch_time / 86400);
	}
	int top_count = 0;
	for (auto &c : counts) top_count = std::max(top_count, c.second);
	int busiest = 0;
	for (int h = 1; h < 24; h++) if (hours[h] > hours[busiest]) busiest = h;
	int streak = 0; // days in a row with something watched, counting back from today (or yesterday)
	long long day = (long long)time(NULL) / 86400;
	if (!days.count(day)) day--;
	while (days.count(day)) streak++, day--;
	const size_t n = history.size();
	const int play_seconds = (int)var_total_play_seconds;
	const size_t favorites = favorites_get().size(), saved = offline_list().size(), channels = get_valid_subscribed_channels().size();
	return {
	    {0, n >= 1, tr("First steps (1 video)", "Primeros pasos (1 vídeo)")},
	    {1, n >= 50, tr("Regular (50 videos)", "Habitual (50 vídeos)")},
	    {2, total_views >= 250, tr("Binge watcher (250 views)", "Maratoniano (250 vistas)")},
	    {3, streak >= 3, tr("3 days in a row", "3 días seguidos")},
	    {4, streak >= 7, tr("A full week in a row", "Una semana seguida")},
	    {5, top_count >= 10, tr("Loyal (10 views of one channel)", "Fiel (10 vistas de un canal)")},
	    {6, n > 0 && busiest <= 4, tr("Night owl (favorite hour 0-4)", "Búho nocturno (hora favorita 0-4)")},
	    {7, n > 0 && busiest >= 5 && busiest <= 8, tr("Early bird (favorite hour 5-8)", "Madrugador (hora favorita 5-8)")},
	    {8, play_seconds >= 10 * 3600, tr("10 hours playing", "10 horas reproduciendo")},
	    {9, play_seconds >= 100 * 3600, tr("100 hours playing", "100 horas reproduciendo")},
	    {10, favorites >= 10, tr("Collector (10 favorites)", "Coleccionista (10 favoritos)")},
	    {11, saved >= 5, tr("Offline fan (5 saved)", "Sin conexión (5 guardados)")},
	    {12, channels >= 10, tr("Subscriber (10 channels)", "Suscriptor (10 canales)")}};
}

void badges_check_new() {
	if (var_incognito) return; // nothing is remembered in incognito
	std::set<int> earned;
	bool existed = false;
	if (FILE *f = fopen(BADGES_PATH, "r")) {
		existed = true;
		int id;
		while (fscanf(f, "%d", &id) == 1) earned.insert(id);
		fclose(f);
	}
	bool changed = false;
	for (auto &badge : badges_compute()) {
		if (!badge.done || earned.count(badge.id)) continue;
		earned.insert(badge.id);
		changed = true;
		if (existed) notice_show(tr("Badge unlocked: ", "Logro desbloqueado: ") + badge.name);
	}
	if (changed || !existed) {
		if (FILE *f = fopen(BADGES_PATH, "w")) {
			for (int id : earned) fprintf(f, "%d\n", id);
			fclose(f);
		}
	}
}
