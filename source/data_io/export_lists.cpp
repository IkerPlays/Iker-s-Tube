#include "headers.hpp"
#include "data_io/export_lists.hpp"
#include "data_io/favorites.hpp"
#include "data_io/history.hpp"
#include "data_io/subscription_util.hpp"
#include "data_io/watch_later.hpp"
#include "util/file_recover.hpp"
#include <sys/stat.h>
#include <ctime>

#define EXPORT_DIR "sdmc:/3ds/IkersTube/export/"

static std::string one_line(std::string s) { // (a tab or a line break would break the file)
	for (auto &c : s) {
		if (c == '\t' || c == '\n' || c == '\r') c = ' ';
	}
	return s;
}
static std::string csv_field(const std::string &s) { // "a ""quoted"" name"
	std::string out = "\"";
	for (char c : one_line(s)) {
		if (c == '"') out += '"';
		out += c;
	}
	return out + "\"";
}

bool export_my_lists(int &subscriptions, int &favorites, int &watch_later, int &history) {
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	mkdir(EXPORT_DIR, 0777);
	bool ok = true;

	std::string text = "Channel Id,Channel Url,Channel Title\n";
	subscriptions = 0;
	for (auto &c : get_valid_subscribed_channels()) {
		if (c.id.empty()) continue;
		text += c.id + ",https://www.youtube.com/channel/" + c.id + "," + csv_field(c.name) + "\n";
		subscriptions++;
	}
	ok = save_text_atomic(EXPORT_DIR "subscriptions.csv", text) && ok;

	text = "";
	favorites = 0;
	for (auto &v : favorites_get()) {
		text += "https://www.youtube.com/watch?v=" + v.id + "\t" + one_line(v.title) + "\n";
		favorites++;
	}
	ok = save_text_atomic(EXPORT_DIR "favorites.txt", text) && ok;

	text = "";
	watch_later = 0;
	for (auto &v : watch_later_get()) {
		text += "https://www.youtube.com/watch?v=" + v.id + "\t" + one_line(v.title) + "\n";
		watch_later++;
	}
	ok = save_text_atomic(EXPORT_DIR "watch_later.txt", text) && ok;

	text = "";
	history = 0;
	for (auto &v : get_valid_watch_history()) {
		char date[24] = {0};
		time_t t = v.last_watch_time;
		struct tm *date_tm = gmtime(&t);
		if (date_tm) strftime(date, sizeof date, "%Y-%m-%d %H:%M", date_tm);
		text += "https://www.youtube.com/watch?v=" + v.id + "\t" + date + "\t" + one_line(v.title) + "\n";
		history++;
	}
	ok = save_text_atomic(EXPORT_DIR "history.txt", text) && ok;
	return ok;
}
