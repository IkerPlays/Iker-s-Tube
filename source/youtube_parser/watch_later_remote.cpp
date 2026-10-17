#include "headers.hpp"
#include <regex>
#include <map>
#include "data_io/watch_later.hpp"
#include "oauth/oauth.hpp"
#include "internal_common.hpp"

// Mirrors the local "Watch later" list into the account's YouTube "Watch later" playlist (playlist id WL).
void watch_later_sync_remote(const std::string &video_id, bool add) {
	if (!OAuth::is_authenticated() || video_id == "") {
		return;
	}
	std::string body =
	    R"({"context": {"client": {"hl": "%0","gl": "%1","clientName": "ANDROID_VR","clientVersion": "1.65.10","deviceMake": "Oculus","deviceModel": "Quest 3","androidSdkVersion": "34","osName": "Android","osVersion": "14"}}, "playlistId": "WL", "actions": [{"action": "%2", "%3": "%4"}]})";
	body = std::regex_replace(body, std::regex("%0"), language_code);
	body = std::regex_replace(body, std::regex("%1"), country_code);
	body = std::regex_replace(body, std::regex("%2"), add ? "ACTION_ADD_VIDEO" : "ACTION_REMOVE_VIDEO_BY_VIDEO_ID");
	body = std::regex_replace(body, std::regex("%3"), add ? "addedVideoId" : "removedVideoId");
	body = std::regex_replace(body, std::regex("%4"), video_id);
	std::map<std::string, std::string> headers;
	headers["Authorization"] = "Bearer " + OAuth::get_access_token();
	auto response = http_post_json(get_innertube_api_url("browse/edit_playlist"), body, headers);
	logger.info("watch-later", std::string(add ? "add " : "remove ") + video_id + " -> " +
	                               (response.first ? "ok" : "request failed"));
}
