#include "headers.hpp"
#include <map>
#include <regex>
#include "youtube_parser/account.hpp"
#include "oauth/oauth.hpp"
#include "internal_common.hpp"

static std::string json_escape(const std::string &s) {
	std::string out;
	for (unsigned char c : s) {
		switch (c) {
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\n': out += "\\n"; break;
		case '\r': break;
		case '\t': out += "\\t"; break;
		default:
			if (c < 0x20) out += ' ';
			else out += (char)c;
		}
	}
	return out;
}

// POST to /youtubei/v1/<endpoint> as the signed-in user. `fields` is the part of the JSON body after the context
// (for example `"videoId":"abc"`). Returns {ok, body}.
static std::pair<bool, std::string> account_post(const std::string &endpoint, const std::string &fields) {
	if (!OAuth::is_authenticated()) {
		return {false, ""};
	}
	std::string body = R"({"context": {"client": {"hl": "%0","gl": "%1","clientName": "ANDROID_VR","clientVersion": "1.62.27"}}, %2})";
	body = std::regex_replace(body, std::regex("%0"), language_code);
	body = std::regex_replace(body, std::regex("%1"), country_code);
	size_t pos = body.find("%2");
	body.replace(pos, 2, fields);
	std::map<std::string, std::string> headers;
	headers["Authorization"] = "Bearer " + OAuth::get_access_token();
	auto response = http_post_json(get_innertube_api_url(endpoint), body, headers);
	bool ok = response.first && response.second.find("\"error\":{") == std::string::npos;
	logger.info("account", endpoint + " -> " + (ok ? "ok" : "failed : " + response.second.substr(0, 100)));
	if (!ok) logger.warning("account", endpoint + " failed");
	return {ok, response.second};
}

static std::string text_after(const std::string &body, size_t from, size_t to, const std::string &key) {
	size_t p = body.find(key, from);
	if (p == std::string::npos || p >= to) return "";
	p += key.size();
	size_t e = body.find('"', p);
	return e == std::string::npos ? "" : body.substr(p, e - p);
}

bool youtube_account_like(const std::string &video_id, int rating) {
	const char *endpoint = rating > 0 ? "like/like" : rating < 0 ? "like/dislike" : "like/removelike";
	return account_post(endpoint, "\"target\": {\"videoId\": \"" + json_escape(video_id) + "\"}").first;
}

bool youtube_account_subscribe(const std::string &channel_id, bool subscribe) {
	return account_post(subscribe ? "subscription/subscribe" : "subscription/unsubscribe",
	                    "\"channelIds\": [\"" + json_escape(channel_id) + "\"]")
	    .first;
}

bool youtube_account_playlists(const std::string &video_id, std::vector<AccountPlaylist> &out) {
	auto r = account_post("playlist/get_add_to_playlist", "\"videoId\": \"" + json_escape(video_id) + "\", \"excludeWatchLater\": false");
	if (!r.first) return false;
	const std::string &json = r.second;
	const std::string marker = "\"playlistAddToOptionRenderer\"";
	size_t pos = 0;
	while ((pos = json.find(marker, pos)) != std::string::npos) {
		size_t next = json.find(marker, pos + marker.size());
		size_t end = next == std::string::npos ? json.size() : next;
		AccountPlaylist p;
		p.id = text_after(json, pos, end, "\"playlistId\":\"");
		p.title = text_after(json, pos, end, "\"title\":{\"simpleText\":\"");
		if (p.title == "") p.title = text_after(json, pos, end, "\"title\":{\"runs\":[{\"text\":\"");
		p.has_video = text_after(json, pos, end, "\"containsSelectedVideos\":\"") == "ALL";
		if (p.id != "" && p.title != "") out.push_back(p);
		pos += marker.size();
	}
	return true;
}

bool youtube_account_playlist_edit(const std::string &playlist_id, const std::string &video_id, bool add) {
	std::string fields = "\"playlistId\": \"" + json_escape(playlist_id) + "\", \"actions\": [{\"action\": \"" +
	                     (add ? "ACTION_ADD_VIDEO\", \"addedVideoId\"" : "ACTION_REMOVE_VIDEO_BY_VIDEO_ID\", \"removedVideoId\"") +
	                     ": \"" + json_escape(video_id) + "\"}]";
	return account_post("browse/edit_playlist", fields).first;
}

// The comment section as the signed-in user sees it : it contains the tokens that allow writing, replying and liking.
static bool fetch_comment_section(const std::string &token, std::string &json) {
	auto r = account_post("next", "\"continuation\": \"" + json_escape(token) + "\"");
	json = r.second;
	return r.first;
}

bool youtube_account_comment_create(const std::string &comments_token, const std::string &text) {
	std::string json;
	if (comments_token == "" || !fetch_comment_section(comments_token, json)) return false;
	size_t end = json.size();
	std::string params = text_after(json, 0, end, "\"createCommentParams\":\"");
	if (params == "") {
		logger.warning("account", "no createCommentParams in the comment section");
		return false;
	}
	return account_post("comment/create_comment",
	                    "\"commentText\": \"" + json_escape(text) + "\", \"createCommentParams\": \"" + json_escape(params) + "\"")
	    .first;
}

// finds the part of the comment section that belongs to one comment id
static bool find_comment_chunk(const std::string &json, const std::string &comment_id, size_t &from, size_t &to) {
	size_t p = json.find("\"commentId\":\"" + comment_id + "\"");
	if (p == std::string::npos) return false;
	size_t next = json.find("\"commentId\":\"", p + 20);
	from = p;
	to = next == std::string::npos ? json.size() : next;
	return true;
}

bool youtube_account_comment_like(const std::string &comments_token, const std::string &comment_id) {
	std::string json;
	if (comments_token == "" || !fetch_comment_section(comments_token, json)) return false;
	size_t from, to;
	if (!find_comment_chunk(json, comment_id, from, to)) {
		logger.warning("account", "comment not found in the first page : " + comment_id);
		return false;
	}
	std::string action = text_after(json, from, to, "\"performCommentActionEndpoint\":{\"action\":\"");
	if (action == "") {
		logger.warning("account", "no like action for the comment");
		return false;
	}
	return account_post("comment/perform_comment_action", "\"actions\": [\"" + json_escape(action) + "\"]").first;
}

bool youtube_account_comment_reply(const std::string &comments_token, const std::string &comment_id,
                                   const std::string &text) {
	std::string json;
	if (comments_token == "" || !fetch_comment_section(comments_token, json)) return false;
	size_t from, to;
	if (!find_comment_chunk(json, comment_id, from, to)) {
		logger.warning("account", "comment not found in the first page : " + comment_id);
		return false;
	}
	std::string params = text_after(json, from, to, "\"createReplyParams\":\"");
	if (params == "") params = text_after(json, from, to, "\"createCommentReplyParams\":\"");
	if (params == "") {
		logger.warning("account", "no reply params for the comment");
		return false;
	}
	return account_post("comment/create_comment_reply",
	                    "\"commentText\": \"" + json_escape(text) + "\", \"createReplyParams\": \"" + json_escape(params) + "\"")
	    .first;
}
