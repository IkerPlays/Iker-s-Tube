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

// The answer is minified JSON : the fields of every "playlistVideoRenderer" are read as text (no need for a full parse)
static std::string text_after(const std::string &body, size_t from, size_t to, const std::string &key) {
	size_t p = body.find(key, from);
	if (p == std::string::npos || p >= to) {
		return "";
	}
	p += key.size();
	size_t e = body.find('"', p);
	return e == std::string::npos ? "" : body.substr(p, e - p);
}

int watch_later_pull_remote() {
	if (!OAuth::is_authenticated()) {
		return -1;
	}
	std::string body =
	    R"({"context": {"client": {"hl": "%0","gl": "%1","clientName": "ANDROID_VR","clientVersion": "1.65.10","deviceMake": "Oculus","deviceModel": "Quest 3","androidSdkVersion": "34","osName": "Android","osVersion": "14"}}, "browseId": "VLWL"})";
	body = std::regex_replace(body, std::regex("%0"), language_code);
	body = std::regex_replace(body, std::regex("%1"), country_code);
	std::map<std::string, std::string> headers;
	headers["Authorization"] = "Bearer " + OAuth::get_access_token();
	auto response = http_post_json(get_innertube_api_url("browse"), body, headers);
	if (!response.first) {
		logger.warning("watch-later", "pull failed (request)");
		return -1;
	}
	const std::string &json = response.second;
	std::vector<FavoriteVideo> videos;
	size_t pos = 0;
	while ((pos = json.find("\"playlistVideoRenderer\"", pos)) != std::string::npos) {
		size_t next = json.find("\"playlistVideoRenderer\"", pos + 20);
		size_t end = next == std::string::npos ? json.size() : next;
		FavoriteVideo v;
		v.id = text_after(json, pos, end, "\"videoId\":\"");
		v.title = text_after(json, pos, end, "\"title\":{\"runs\":[{\"text\":\"");
		v.author = text_after(json, pos, end, "\"shortBylineText\":{\"runs\":[{\"text\":\"");
		v.length_text = text_after(json, pos, end, "\"lengthText\":{\"accessibility\":{\"accessibilityData\":{\"label\":\"");
		std::string simple = text_after(json, pos, end, "\"lengthText\":{\"simpleText\":\"");
		if (simple != "") v.length_text = simple;
		if (v.id != "" && v.title != "") {
			videos.push_back(v);
		}
		pos += 20;
	}
	int added = watch_later_merge(videos);
	logger.info("watch-later", "pull: " + std::to_string(videos.size()) + " found, " + std::to_string(added) + " new");
	return added;
}
