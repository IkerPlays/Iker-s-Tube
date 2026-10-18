#include "headers.hpp"
#include "youtube_parser/studio.hpp"
#include "youtube_parser/studio_auth.hpp"
#include <map>
#include <regex>
#include <algorithm>
#include <cstring>
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

// The clients that the account requests can be sent as (the login works with several of them).
static const char *CLIENTS[] = {
    R"("clientName": "ANDROID_VR","clientVersion": "1.62.27")",                                  // 0
    R"("clientName": "MWEB","clientVersion": "2.20241202.07.00","utcOffsetMinutes": 0)",         // 1 (the comments come from it)
    R"("clientName": "WEB","clientVersion": "2.20241126.01.00","utcOffsetMinutes": 0)",          // 2
    R"("clientName": "TVHTML5","clientVersion": "7.20250101.00.00")",                            // 3
    R"("clientName": "ANDROID","clientVersion": "19.51.37","androidSdkVersion": 34)",            // 4
};
static const int CLIENT_COUNT = sizeof CLIENTS / sizeof CLIENTS[0];

// shared between the UI thread and the worker threads : always behind this lock
static LightLock state_lock;
static struct StateLockInit {
	StateLockInit() { LightLock_Init(&state_lock); }
} state_lock_init;
static std::string last_error_text; // what the last failed request said (endpoint, client, HTTP status, start of the answer)
static std::string context_video;   // the video whose comments are being used

static void set_last_error(const std::string &text) {
	LightLock_Lock(&state_lock);
	last_error_text = text;
	LightLock_Unlock(&state_lock);
}
static std::string get_last_error() {
	LightLock_Lock(&state_lock);
	std::string res = last_error_text;
	LightLock_Unlock(&state_lock);
	return res;
}
static std::string get_context_video() {
	LightLock_Lock(&state_lock);
	std::string res = context_video;
	LightLock_Unlock(&state_lock);
	return res;
}

std::string youtube_account_last_error() { return get_last_error(); }
void youtube_account_set_error(const std::string &text) { set_last_error(text); }
void youtube_account_set_video(const std::string &video_id) {
	LightLock_Lock(&state_lock);
	if (context_video != video_id) context_video = video_id; // (called every frame : assign only when it changed)
	LightLock_Unlock(&state_lock);
}

// POST to /youtubei/v1/<endpoint> as the signed-in user. `fields` is the part of the JSON body after the context
// (for example `"videoId":"abc"`). Returns {ok, body}.
static std::pair<bool, std::string> account_post(const std::string &endpoint, const std::string &fields, int client = 0) {
	if (!OAuth::is_authenticated()) {
		set_last_error("not signed in");
		return {false, ""};
	}
	if (client < 0 || client >= CLIENT_COUNT) client = 0;
	std::string body = std::string(R"({"context": {"client": {"hl": "%0","gl": "%1",)") + CLIENTS[client] + R"(}}, %2})";
	body = std::regex_replace(body, std::regex("%0"), language_code);
	body = std::regex_replace(body, std::regex("%1"), country_code);
	size_t pos = body.find("%2");
	body.replace(pos, 2, fields);
	std::map<std::string, std::string> headers;
	headers["Authorization"] = "Bearer " + OAuth::get_access_token();
	auto response = http_post_json(get_innertube_api_url(endpoint), body, headers);
	bool ok = response.first && response.second.find("\"error\":{") == std::string::npos;
	if (!ok) {
		std::string msg;
		size_t m = response.second.find("\"message\"");
		if (m != std::string::npos) msg = response.second.substr(m, 90);
		std::string why = endpoint + "/c" + std::to_string(client) + " " + (response.first ? msg : response.second.substr(0, 60));
		set_last_error(why);
		logger.warning("account", "failed : " + why + " | " + response.second.substr(0, 300));
	} else {
		logger.info("account", endpoint + " ok (client " + std::to_string(client) + ")");
	}
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
	if (account_post(endpoint, "\"target\": {\"videoId\": \"" + json_escape(video_id) + "\"}").first) return true;
	std::string err; // the app login refused it : the Data API with the Studio login
	if (studio_auth_signed_in() && youtube_studio_rate(video_id, rating > 0 ? "like" : rating < 0 ? "dislike" : "none", err)) return true;
	return false;
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

bool youtube_account_playlist_create(const std::string &title, const std::string &privacy, std::string &playlist_id) {
	std::string fields = "\"title\": \"" + json_escape(title) + "\", \"privacyStatus\": \"" + json_escape(privacy) + "\"";
	for (int client = 0; client < 2; client++) { // the web client first : it is the one the site uses for this
		auto r = account_post("playlist/create", fields, 1 - client);
		if (!r.first) continue;
		playlist_id = text_after(r.second, 0, r.second.size(), "\"playlistId\":\"");
		if (playlist_id == "") playlist_id = text_after(r.second, 0, r.second.size(), "\"playlistId\": \"");
		return true;
	}
	return false;
}

bool youtube_account_playlist_edit(const std::string &playlist_id, const std::string &video_id, bool add) {
	std::string fields = "\"playlistId\": \"" + json_escape(playlist_id) + "\", \"actions\": [{\"action\": \"" +
	                     (add ? "ACTION_ADD_VIDEO\", \"addedVideoId\"" : "ACTION_REMOVE_VIDEO_BY_VIDEO_ID\", \"removedVideoId\"") +
	                     ": \"" + json_escape(video_id) + "\"}]";
	return account_post("browse/edit_playlist", fields).first;
}

// The comment section as the signed-in user sees it : it contains the tokens that allow writing, replying and liking.
// The login does not work the same with every YouTube client, so several are tried (the one that worked last time first),
// first with the continuation token of the page and then, if that is refused, by asking the client for the video itself.
static volatile int comment_client = 1; // the client that last returned a usable comment section

static bool comment_section_usable(const std::string &json, bool need_params) {
	if (need_params) return json.find("createCommentParams") != std::string::npos;
	return json.find("likeCommand") != std::string::npos || json.find("replyCommand") != std::string::npos ||
	       json.find("\"commentId\"") != std::string::npos;
}

static std::vector<std::string> continuation_tokens(const std::string &json, size_t max) {
	std::vector<std::string> out;
	const char *keys[] = {"\"token\":\"", "\"continuation\":\""};
	for (const char *key : keys) {
		size_t pos = 0;
		while (out.size() < max && (pos = json.find(key, pos)) != std::string::npos) {
			std::string t = text_after(json, pos, json.size(), key);
			pos += strlen(key);
			if (t.size() > 40 && std::find(out.begin(), out.end(), t) == out.end()) out.push_back(t);
		}
	}
	return out;
}

// The continuation tokens that belong to the comments : the ones found right after the marks of the comment section
// (the page of a video has many more tokens, for the suggestions, and trying them blindly misses the comments).
static std::vector<std::string> comment_section_tokens(const std::string &json) {
	std::vector<std::string> out;
	const char *marks[] = {"comment-item-section", "engagement-panel-comments-section", "comments-section", "commentsEntryPointHeaderRenderer", "\"comments\""};
	for (const char *mark : marks) {
		size_t pos = 0;
		while (out.size() < 6 && (pos = json.find(mark, pos)) != std::string::npos) {
			size_t end = std::min(json.size(), pos + 4000);
			for (const char *key : {"\"token\":\"", "\"continuation\":\""}) {
				std::string t = text_after(json, pos, end, key);
				if (t.size() > 40 && std::find(out.begin(), out.end(), t) == out.end()) out.push_back(t);
			}
			pos += strlen(mark);
		}
	}
	return out;
}

static bool fetch_comment_section(const std::string &token, std::string &json, bool need_params) {
	int order[6] = {comment_client, 1, 2, 0, 3, 4};
	std::vector<int> clients;
	for (int c : order) {
		if (std::find(clients.begin(), clients.end(), c) == clients.end()) clients.push_back(c);
	}
	for (int client : clients) {
		auto r = token != "" ? account_post("next", "\"continuation\": \"" + json_escape(token) + "\"", client)
		                     : std::make_pair(false, std::string()); // no token (comments not opened) : straight to the page of the video
		if (r.first && comment_section_usable(r.second, need_params)) {
			comment_client = client;
			json = r.second;
			return true;
		}
		if (!r.first && token != "") continue; // this client does not take the login (or the token) : the next one
		std::string video_id = get_context_video();
		if (video_id != "") { // the page of the video as this client sees it has its own comment tokens
			auto page = account_post("next", "\"videoId\": \"" + json_escape(video_id) + "\"", client);
			if (!page.first) continue;
			if (comment_section_usable(page.second, need_params)) {
				comment_client = client;
				json = page.second;
				return true;
			}
			std::vector<std::string> candidates = comment_section_tokens(page.second);
			for (auto &t : continuation_tokens(page.second, 5)) {
				if (std::find(candidates.begin(), candidates.end(), t) == candidates.end()) candidates.push_back(t);
			}
			for (auto &t : candidates) {
				auto section = account_post("next", "\"continuation\": \"" + json_escape(t) + "\"", client);
				if (section.first && comment_section_usable(section.second, need_params)) {
					comment_client = client;
					json = section.second;
					return true;
				}
			}
		}
	}
	if (get_last_error() == "") set_last_error("no comment section");
	logger.warning("account", "no usable comment section in any client");
	return false;
}

// Writing through the Data API with the Studio login : the app login (YouTube TV) is often refused for comments.
static bool data_api_comments_blocked = false; // YouTube answered "insufficient scopes" : this login cannot comment through the Data API
static bool studio_ready() { return studio_auth_signed_in() && !data_api_comments_blocked; }
static void note_data_api_error(const std::string &err) {
	if (err.find("insufficient") != std::string::npos) data_api_comments_blocked = true;
}

bool youtube_account_comment_create(const std::string &comments_token, const std::string &text) {
	std::string video = get_context_video(), err;
	if (studio_ready() && video != "") {
		if (youtube_studio_comment_insert(video, text, err)) return true;
		note_data_api_error(err);
		set_last_error("");
	}
	std::string json;
	if (!fetch_comment_section(comments_token, json, true)) {
		if (err != "") set_last_error(get_last_error() + " | data api: " + err);
		return false;
	}
	size_t end = json.size();
	std::string params = text_after(json, 0, end, "\"createCommentParams\":\"");
	if (params == "") {
		set_last_error("no createCommentParams");
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
	if (!fetch_comment_section(comments_token, json, false)) return false;
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

bool youtube_account_comment_delete(const std::string &comments_token, const std::string &comment_id) {
	std::string err;
	if (studio_ready() && comment_id != "") {
		if (youtube_studio_comment_delete(comment_id, err)) return true;
		note_data_api_error(err);
	}
	std::string json;
	if (!fetch_comment_section(comments_token, json, false)) return false;
	// the menu of an own comment has an entry with the DELETE icon, followed by the action that does it
	std::string action;
	for (const char *marker : {"\"iconType\":\"DELETE\"", "\"iconType\":\"TRASH\"", "\"icon\":\"DELETE\"", "\"icon\":\"TRASH\"", "\"DELETE\""}) {
		size_t pos = 0;
		while (action == "" && (pos = json.find(marker, pos)) != std::string::npos) {
			size_t end = std::min(json.size(), pos + 3500);
			std::string token = text_after(json, pos, end, "\"performCommentActionEndpoint\":{\"action\":\"");
			if (token == "") token = text_after(json, pos, end, "\"action\":\"");
			if (token != "" && base64_decode_loose(token).find(comment_id) != std::string::npos) action = token;
			pos += strlen(marker);
		}
		if (action != "") break;
	}
	if (action == "") {
		set_last_error("no delete action found");
		logger.warning("account", "no delete action found for the comment");
		return false;
	}
	return account_post("comment/perform_comment_action", "\"actions\": [\"" + json_escape(action) + "\"]", comment_client).first;
}

bool youtube_account_comment_reply(const std::string &comments_token, const std::string &comment_id,
                                   const std::string &text) {
	std::string err;
	if (studio_ready() && comment_id != "") {
		if (youtube_studio_comment_reply(comment_id, text, err)) return true;
		note_data_api_error(err);
		set_last_error("");
	}
	std::string json;
	if (!fetch_comment_section(comments_token, json, false)) return false;
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

// the feedback tokens of the recommended videos {video id -> {not interested, don't recommend the channel}}
static std::map<std::string, std::pair<std::string, std::string>> feedback_tokens;

bool youtube_account_not_interested(const std::string &video_id, bool whole_channel) {
	std::string token;
	LightLock_Lock(&state_lock);
	auto it = feedback_tokens.find(video_id);
	if (it != feedback_tokens.end()) token = whole_channel ? it->second.second : it->second.first;
	LightLock_Unlock(&state_lock);
	if (token == "") {
		set_last_error("open it from the recommendations first");
		return false;
	}
	for (int client : {1, 0}) {
		if (account_post("feedback", "\"feedbackTokens\": [\"" + json_escape(token) + "\"]", client).first) return true;
	}
	return false;
}

bool youtube_account_channel_bell(const std::string &channel_id, int level) {
	for (int client : {1, 0, 2, 3}) {
		auto page = account_post("browse", "\"browseId\": \"" + json_escape(channel_id) + "\"", client);
		if (!page.first) continue;
		// the menu of the bell has three entries (all, personalized, none) with the action that sets each one
		std::vector<std::string> params;
		const std::string key = "\"modifyChannelNotificationPreferenceEndpoint\":{\"params\":\"";
		for (size_t pos = 0; params.size() < 3 && (pos = page.second.find(key, pos)) != std::string::npos; pos += key.size()) {
			std::string p = text_after(page.second, pos, page.second.size(), key);
			if (p != "" && std::find(params.begin(), params.end(), p) == params.end()) params.push_back(p);
		}
		if (params.size() < 3) continue; // not subscribed on the account (or a layout without the bell)
		return account_post("notification/modify_channel_preference", "\"params\": \"" + json_escape(params[std::max(0, std::min(2, level))]) + "\"", client).first;
	}
	set_last_error("no bell found : subscribe on your account first");
	return false;
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
		{ // the feedback tokens of its menu
			size_t window = std::min(json.size(), pos + 9000);
			std::string not_interested, dont_recommend;
			size_t ni = json.find("NOT_INTERESTED", pos);
			if (ni != std::string::npos && ni < window) not_interested = text_after(json, ni, std::min(json.size(), ni + 1500), "\"feedbackToken\":\"");
			size_t dr = json.find("REMOVE", pos);
			if (dr != std::string::npos && dr < window) dont_recommend = text_after(json, dr, std::min(json.size(), dr + 1500), "\"feedbackToken\":\"");
			if (not_interested != "" || dont_recommend != "") {
				LightLock_Lock(&state_lock);
				feedback_tokens[id] = {not_interested, dont_recommend};
				LightLock_Unlock(&state_lock);
			}
		}
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

// Tells YouTube the video was watched (the "videostatsPlaybackUrl" of the player answer + a random client playback nonce).
void youtube_account_mark_watched(const std::string &playback_url) {
	if (!OAuth::is_authenticated() || playback_url == "") return;
	static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	std::string cpn;
	for (int i = 0; i < 16; i++) cpn += alphabet[rand() % 64];
	std::map<std::string, std::string> headers;
	headers["Authorization"] = "Bearer " + OAuth::get_access_token();
	auto r = http_get(playback_url + "&ver=2&cpn=" + cpn, headers);
	logger.info("account", std::string("history ping ") + (r.first ? "ok" : "failed"));
}
