#include "headers.hpp"
#include <map>
#include <regex>
#include <algorithm>
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
static std::pair<bool, std::string> account_post(const std::string &endpoint, const std::string &fields, int client = 0) {
	if (!OAuth::is_authenticated()) {
		return {false, ""};
	}
	// 0 : ANDROID_VR, 1 : MWEB (the comments tokens of the app come from MWEB)
	const char *client_json = client == 1
	                              ? R"("clientName": "MWEB","clientVersion": "2.20241202.07.00","utcOffsetMinutes": 0)"
	                              : R"("clientName": "ANDROID_VR","clientVersion": "1.62.27")";
	std::string body = std::string(R"({"context": {"client": {"hl": "%0","gl": "%1",)") + client_json + R"(}}, %2})";
	body = std::regex_replace(body, std::regex("%0"), language_code);
	body = std::regex_replace(body, std::regex("%1"), country_code);
	size_t pos = body.find("%2");
	body.replace(pos, 2, fields);
	std::map<std::string, std::string> headers;
	headers["Authorization"] = "Bearer " + OAuth::get_access_token();
	auto response = http_post_json(get_innertube_api_url(endpoint), body, headers);
	bool ok = response.first && response.second.find("\"error\":{") == std::string::npos;
	logger.info("account", endpoint + " -> " + (ok ? "ok" : "failed : " + response.second.substr(0, 300)));
	if (!ok) logger.warning("account", endpoint + " failed (client " + std::to_string(client) + ")");
	return {ok, response.second};
}

static std::string text_after(const std::string &body, size_t from, size_t to, const std::string &key) {
	size_t p = body.find(key, from);
	if (p == std::string::npos || p >= to) return "";
	p += key.size();
	size_t e = p;
	while (e < body.size() && body[e] != '"') {
		if (body[e] == '\\') e++; // escaped character (\" or \u0026 ...)
		e++;
	}
	if (e >= body.size()) return "";
	// unescape the common sequences so that titles read normally
	std::string raw = body.substr(p, e - p), res;
	for (size_t i = 0; i < raw.size(); i++) {
		if (raw[i] == '\\' && i + 1 < raw.size()) {
			char c = raw[i + 1];
			if (c == '"' || c == '\\' || c == '/') res += c, i++;
			else if (c == 'n') res += ' ', i++;
			else if (c == 'u' && i + 5 < raw.size()) {
				unsigned cp = (unsigned)strtoul(raw.substr(i + 2, 4).c_str(), NULL, 16);
				i += 5;
				if (cp >= 0xD800 && cp < 0xDC00 && i + 6 < raw.size() && raw[i + 1] == '\\' && raw[i + 2] == 'u') { // surrogate pair
					unsigned low = (unsigned)strtoul(raw.substr(i + 3, 4).c_str(), NULL, 16);
					cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
					i += 6;
				}
				if (cp < 0x80) res += (char)cp;
				else if (cp < 0x800) res += (char)(0xC0 | (cp >> 6)), res += (char)(0x80 | (cp & 0x3F));
				else if (cp < 0x10000) res += (char)(0xE0 | (cp >> 12)), res += (char)(0x80 | ((cp >> 6) & 0x3F)), res += (char)(0x80 | (cp & 0x3F));
				else res += (char)(0xF0 | (cp >> 18)), res += (char)(0x80 | ((cp >> 12) & 0x3F)), res += (char)(0x80 | ((cp >> 6) & 0x3F)), res += (char)(0x80 | (cp & 0x3F));
			}
			else res += raw[i];
		} else {
			res += raw[i];
		}
	}
	return res;
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
static int comment_client = 1; // the client that last returned a usable comment section
static bool fetch_comment_section(const std::string &token, std::string &json) {
	for (int attempt = 0; attempt < 2; attempt++) {
		int client = attempt == 0 ? comment_client : 1 - comment_client;
		auto r = account_post("next", "\"continuation\": \"" + json_escape(token) + "\"", client);
		json = r.second;
		if (r.first && json.find("createCommentParams") != std::string::npos) {
			comment_client = client;
			return true;
		}
		if (r.first && attempt == 1) return true; // other actions (like, reply) do not need createCommentParams
	}
	return false;
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
	                    "\"commentText\": \"" + json_escape(text) + "\", \"createCommentParams\": \"" + json_escape(params) + "\"",
	                    comment_client)
	    .first;
}

// The tokens of a comment (like action, reply params) are protobuf messages in base64 that contain the id of the comment.
// So the token that belongs to a comment is found by decoding the candidates and looking for the id inside, which does not
// depend on how the answer is laid out.
static std::string base64_decode_loose(const std::string &in) {
	std::string clean;
	for (size_t i = 0; i < in.size(); i++) { // percent escapes like %3D
		if (in[i] == '%' && i + 2 < in.size() + 0) {
			clean += (char)strtol(in.substr(i + 1, 2).c_str(), NULL, 16);
			i += 2;
		} else {
			clean += in[i];
		}
	}
	std::string out;
	unsigned val = 0;
	int bits = -8;
	for (unsigned char c : clean) {
		int d;
		if (c >= 'A' && c <= 'Z') d = c - 'A';
		else if (c >= 'a' && c <= 'z') d = c - 'a' + 26;
		else if (c >= '0' && c <= '9') d = c - '0' + 52;
		else if (c == '+' || c == '-') d = 62;
		else if (c == '/' || c == '_') d = 63;
		else continue; // '=' and anything else
		val = (val << 6) | d;
		bits += 6;
		if (bits >= 0) {
			out += (char)((val >> bits) & 0xFF);
			bits -= 8;
		}
	}
	return out;
}

// looks for `token_key` in the `window` bytes that follow each occurrence of `anchor`; returns the first token whose
// decoded content contains the comment id
static std::string find_comment_token(const std::string &json, const std::string &anchor, size_t window,
                                      const std::string &token_key, const std::string &comment_id) {
	size_t pos = 0;
	while ((pos = json.find(anchor, pos)) != std::string::npos) {
		size_t end = std::min(json.size(), pos + window);
		size_t search = pos;
		while (true) {
			size_t p = json.find(token_key, search);
			if (p == std::string::npos || p >= end) break;
			std::string token = text_after(json, p, end, token_key);
			if (token != "" && base64_decode_loose(token).find(comment_id) != std::string::npos) return token;
			search = p + token_key.size();
		}
		pos += anchor.size();
	}
	return "";
}

bool youtube_account_comment_like(const std::string &comments_token, const std::string &comment_id) {
	std::string json;
	if (comments_token == "" || !fetch_comment_section(comments_token, json)) return false;
	std::string action = find_comment_token(json, "\"likeCommand\"", 1500, "\"action\":\"", comment_id);
	if (action == "") { // older layout : the action sits inside the comment itself
		size_t p = json.find("\"commentId\":\"" + comment_id + "\"");
		if (p != std::string::npos) action = text_after(json, p, std::min(json.size(), p + 6000), "\"performCommentActionEndpoint\":{\"action\":\"");
	}
	if (action == "") {
		logger.warning("account", "no like action found for the comment (not in this page, or the account layout differs)");
		return false;
	}
	return account_post("comment/perform_comment_action", "\"actions\": [\"" + json_escape(action) + "\"]", comment_client).first;
}

bool youtube_account_comment_reply(const std::string &comments_token, const std::string &comment_id,
                                   const std::string &text) {
	std::string json;
	if (comments_token == "" || !fetch_comment_section(comments_token, json)) return false;
	std::string params = find_comment_token(json, "\"replyCommand\"", 6000, "\"createReplyParams\":\"", comment_id);
	if (params == "") params = find_comment_token(json, "\"replyCommand\"", 6000, "\"createCommentReplyParams\":\"", comment_id);
	if (params == "") {
		size_t p = json.find("\"commentId\":\"" + comment_id + "\"");
		if (p != std::string::npos) {
			size_t end = std::min(json.size(), p + 8000);
			params = text_after(json, p, end, "\"createReplyParams\":\"");
			if (params == "") params = text_after(json, p, end, "\"createCommentReplyParams\":\"");
		}
	}
	if (params == "") {
		logger.warning("account", "no reply params found for the comment (not in this page, or the account layout differs)");
		return false;
	}
	return account_post("comment/create_comment_reply",
	                    "\"commentText\": \"" + json_escape(text) + "\", \"createReplyParams\": \"" + json_escape(params) + "\"",
	                    comment_client)
	    .first;
}

// Recommendations / notifications : the answers are minified JSON, the fields of each video are read as text.
static bool is_video_id(const std::string &s) {
	if (s.size() != 11) return false;
	for (char c : s) {
		if (!(isalnum((unsigned char)c) || c == '-' || c == '_')) return false;
	}
	return true;
}

bool youtube_account_recommendations(std::vector<YouTubeVideoSuccinct> &out) {
	auto r = account_post("browse", "\"browseId\": \"FEwhat_to_watch\"");
	if (!r.first) return false;
	const std::string &json = r.second;
	const std::string marker = "\"videoId\":\"";
	std::vector<std::string> seen;
	size_t pos = 0;
	while ((pos = json.find(marker, pos)) != std::string::npos && out.size() < 40) {
		size_t id_start = pos + marker.size();
		std::string id = json.substr(id_start, 11);
		pos = id_start + 11;
		if (!is_video_id(id) || std::find(seen.begin(), seen.end(), id) != seen.end()) continue;
		seen.push_back(id);
		size_t end = std::min(json.size(), pos + 2500);
		YouTubeVideoSuccinct v;
		v.title = text_after(json, pos, end, "\"title\":{\"runs\":[{\"text\":\"");
		if (v.title == "") v.title = text_after(json, pos, end, "\"title\":{\"simpleText\":\"");
		if (v.title == "") v.title = text_after(json, pos, end, "\"headline\":{\"runs\":[{\"text\":\"");
		if (v.title == "") continue;
		v.author = text_after(json, pos, end, "\"shortBylineText\":{\"runs\":[{\"text\":\"");
		v.duration_text = text_after(json, pos, end, "\"lengthText\":{\"simpleText\":\"");
		v.url = youtube_get_video_url_by_id(id);
		v.thumbnail_url = youtube_get_video_thumbnail_url_by_id(id);
		out.push_back(v);
	}
	logger.info("account", "recommendations : " + std::to_string(out.size()));
	return true;
}

bool youtube_account_notifications(std::vector<YouTubeVideoSuccinct> &out) {
	auto r = account_post("notification/get_notification_menu", "\"notificationsMenuRequestType\": \"NOTIFICATIONS_MENU_REQUEST_TYPE_INBOX\"");
	if (!r.first) return false;
	const std::string &json = r.second;
	const std::string marker = "\"notificationRenderer\"";
	size_t pos = 0;
	while ((pos = json.find(marker, pos)) != std::string::npos && out.size() < 40) {
		size_t next = json.find(marker, pos + marker.size());
		size_t end = next == std::string::npos ? json.size() : next;
		pos += marker.size();
		YouTubeVideoSuccinct v;
		v.title = text_after(json, pos, end, "\"shortMessage\":{\"simpleText\":\"");
		std::string id = text_after(json, pos, end, "\"videoId\":\"");
		if (!is_video_id(id)) {
			size_t w = json.find("watch?v=", pos);
			if (w != std::string::npos && w < end) id = json.substr(w + 8, 11);
		}
		if (v.title == "" || !is_video_id(id)) continue;
		v.url = youtube_get_video_url_by_id(id);
		v.thumbnail_url = youtube_get_video_thumbnail_url_by_id(id);
		out.push_back(v);
	}
	logger.info("account", "notifications : " + std::to_string(out.size()));
	return true;
}
