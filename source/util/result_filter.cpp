#include "headers.hpp"
#include "util/result_filter.hpp"
#include "data_io/history.hpp"
#include "youtube_parser/parser.hpp"

bool result_video_hidden(const std::string &url) {
	if (var_hide_shorts && url.find("/shorts/") != std::string::npos) return true;
	if (var_hide_watched) {
		std::string id = youtube_get_video_id_by_url(url);
		if (id != "" && history_has_id(id)) return true;
	}
	return false;
}
