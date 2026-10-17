#include "headers.hpp"
#include "youtube_parser/studio.hpp"
#include "youtube_parser/internal_common.hpp"
#include "oauth/oauth.hpp"
#include <sys/stat.h>
#include <algorithm>

static std::string json_quote(const std::string &s) {
	std::string out = "\"";
	for (unsigned char c : s) {
		if (c == '"' || c == '\\') {
			out += '\\';
			out += (char)c;
		} else if (c == '\n') {
			out += "\\n";
		} else if (c < 0x20) {
			out += ' ';
		} else {
			out += (char)c;
		}
	}
	return out + "\"";
}

// the digits of a number field written like  "viewCount": "1234"
static bool number_field(const std::string &json, const std::string &key, unsigned long long &out) {
	size_t p = json.find("\"" + key + "\"");
	if (p == std::string::npos) return false;
	p = json.find(':', p);
	if (p == std::string::npos) return false;
	p++;
	while (p < json.size() && (json[p] == ' ' || json[p] == '"')) p++;
	if (p >= json.size() || !isdigit((unsigned char)json[p])) return false;
	out = strtoull(json.c_str() + p, NULL, 10);
	return true;
}

static std::string short_number(unsigned long long n) {
	char buf[32];
	if (n >= 1000000000ULL) snprintf(buf, sizeof buf, "%.1fB", n / 1e9);
	else if (n >= 1000000ULL) snprintf(buf, sizeof buf, "%.1fM", n / 1e6);
	else if (n >= 10000ULL) snprintf(buf, sizeof buf, "%.1fK", n / 1e3);
	else snprintf(buf, sizeof buf, "%llu", n);
	std::string s = buf;
	size_t dot = s.find(".0");
	if (dot != std::string::npos) s.erase(dot, 2);
	return s;
}

static NetworkSessionList &studio_session() {
	static NetworkSessionList session;
	if (!session.inited) session.init();
	return session;
}

StudioStats youtube_studio_stats() {
	StudioStats res;
	if (!OAuth::is_authenticated()) {
		res.error = "not signed in";
		return res;
	}
	auto r = studio_session().perform(HttpRequest::GET("https://www.googleapis.com/youtube/v3/channels?part=statistics&mine=true",
	                                                 {{"Authorization", "Bearer " + OAuth::get_access_token()}}));
	std::string body(r.data.begin(), r.data.end());
	unsigned long long views = 0, subs = 0, videos = 0;
	if (!r.fail && r.status_code_is_success() && number_field(body, "viewCount", views)) {
		res.views = short_number(views);
		if (number_field(body, "subscriberCount", subs)) res.subscribers = short_number(subs);
		if (number_field(body, "videoCount", videos)) res.videos = short_number(videos);
		return res;
	}
	logger.warning("studio", "channels statistics failed : http " + std::to_string(r.status_code) + " " + r.error + " " +
	                             body.substr(0, 300));
	// the subscribers are known from the login
	if (OAuth::get_user_subscriber_count() != "") res.subscribers = OAuth::get_user_subscriber_count();
	res.error = r.fail ? r.error : "http " + std::to_string(r.status_code);
	return res;
}

static std::string mime_of(const std::string &path) {
	std::string ext = path.substr(path.find_last_of('.') == std::string::npos ? path.size() : path.find_last_of('.') + 1);
	for (auto &c : ext) c = tolower(c);
	if (ext == "mp4" || ext == "m4v") return "video/mp4";
	if (ext == "mov") return "video/quicktime";
	if (ext == "avi") return "video/x-msvideo";
	if (ext == "mkv") return "video/x-matroska";
	if (ext == "webm") return "video/webm";
	if (ext == "3gp") return "video/3gpp";
	return "application/octet-stream";
}

bool youtube_studio_upload(const std::string &path, const std::string &title, const std::string &description,
                           const std::string &privacy, bool is_short, const std::string &category_id,
                           const std::function<void(u64, u64)> &progress, std::string &video_id, std::string &error) {
	if (!OAuth::is_authenticated()) {
		error = "not signed in";
		return false;
	}
	struct stat st;
	if (stat(path.c_str(), &st) != 0 || st.st_size <= 0) {
		error = "cannot read the file";
		return false;
	}
	u64 total = (u64)st.st_size;
	FILE *file = fopen(path.c_str(), "rb");
	if (!file) {
		error = "cannot open the file";
		return false;
	}
	static NetworkSessionList upload_session; // not shared with the statistics : a session belongs to one thread
	if (!upload_session.inited) upload_session.init();
	NetworkSessionList &session = upload_session;

	std::string full_title = title;
	std::string full_description = description;
	if (is_short) {
		if (full_title.find("#Shorts") == std::string::npos && full_title.find("#shorts") == std::string::npos &&
		    full_title.size() + 8 <= 100)
			full_title += " #Shorts";
		if (full_description.find("#Shorts") == std::string::npos) full_description += (full_description == "" ? "" : "\n") + std::string("#Shorts");
	}
	std::string meta = "{\"snippet\": {\"title\": " + json_quote(full_title) + ", \"description\": " + json_quote(full_description) +
	                   ", \"categoryId\": " + json_quote(category_id) + "}, \"status\": {\"privacyStatus\": " + json_quote(privacy) +
	                   ", \"selfDeclaredMadeForKids\": false}}";
	auto init = session.perform(HttpRequest::POST(
	    "https://www.googleapis.com/upload/youtube/v3/videos?uploadType=resumable&part=snippet,status",
	    {{"Authorization", "Bearer " + OAuth::get_access_token()},
	     {"Content-Type", "application/json; charset=UTF-8"},
	     {"X-Upload-Content-Length", std::to_string(total)},
	     {"X-Upload-Content-Type", mime_of(path)}},
	    meta));
	std::string upload_url = init.get_header("location");
	if (init.fail || !init.status_code_is_success() || upload_url == "") {
		error = init.fail ? init.error : "http " + std::to_string(init.status_code);
		std::string body(init.data.begin(), init.data.end());
		logger.warning("studio", "upload start failed : " + error + " " + body.substr(0, 400));
		size_t m = body.find("\"message\"");
		if (m != std::string::npos) error += " " + body.substr(m, 160);
		fclose(file);
		return false;
	}

	const u64 CHUNK = 512 * 1024; // a multiple of 256 KiB, as the upload protocol asks
	u64 offset = 0;
	std::vector<char> buf(CHUNK);
	int failures = 0;
	while (offset < total) {
		u64 len = std::min<u64>(CHUNK, total - offset);
		fseek(file, (long)offset, SEEK_SET);
		size_t got = fread(buf.data(), 1, len, file);
		if (got != len) {
			error = "cannot read the file";
			fclose(file);
			return false;
		}
		std::string body(buf.data(), len);
		auto r = session.perform(HttpRequest{"PUT",
		                                     upload_url,
		                                     HttpRequest::default_headers_added(
		                                         {{"Authorization", "Bearer " + OAuth::get_access_token()},
		                                          {"Content-Type", mime_of(path)},
		                                          {"Content-Range", "bytes " + std::to_string(offset) + "-" +
		                                                                std::to_string(offset + len - 1) + "/" +
		                                                                std::to_string(total)}}),
		                                     body,
		                                     false});
		if (!r.fail && (r.status_code == 200 || r.status_code == 201)) {
			std::string resp(r.data.begin(), r.data.end());
			size_t p = resp.find("\"id\"");
			if (p != std::string::npos) {
				p = resp.find('"', resp.find(':', p));
				size_t e = resp.find('"', p + 1);
				if (p != std::string::npos && e != std::string::npos) video_id = resp.substr(p + 1, e - p - 1);
			}
			if (progress) progress(total, total);
			fclose(file);
			return true;
		}
		if (!r.fail && r.status_code == 308) { // chunk accepted
			std::string range = r.get_header("range"); // "bytes=0-1234" : what the server really has
			size_t dash = range.find('-');
			u64 have = dash != std::string::npos ? strtoull(range.c_str() + dash + 1, NULL, 10) + 1 : 0; // no Range : nothing stored
			offset = have <= total ? have : offset + len;
			failures = 0;
			if (progress) progress(offset, total);
			continue;
		}
		failures++;
		std::string resp(r.data.begin(), r.data.end());
		logger.warning("studio", "upload chunk failed at " + std::to_string(offset) + " : http " + std::to_string(r.status_code) +
		                             " " + r.error + " " + resp.substr(0, 300));
		if (failures >= 4) {
			error = r.fail ? r.error : "http " + std::to_string(r.status_code);
			fclose(file);
			return false;
		}
		svcSleepThread((s64)failures * 1000000000LL); // back off before trying again
		// ask the server how much it has before sending again
		auto q = session.perform(HttpRequest{"PUT", upload_url,
		                                     HttpRequest::default_headers_added({{"Authorization", "Bearer " + OAuth::get_access_token()},
		                                                                         {"Content-Range", "bytes */" + std::to_string(total)}}),
		                                     "", false});
		if (!q.fail && (q.status_code == 200 || q.status_code == 201)) { // the upload had finished, only its answer was lost
			std::string resp(q.data.begin(), q.data.end());
			size_t p = resp.find("\"id\"");
			if (p != std::string::npos) {
				p = resp.find('"', resp.find(':', p));
				size_t e = resp.find('"', p + 1);
				if (p != std::string::npos && e != std::string::npos) video_id = resp.substr(p + 1, e - p - 1);
			}
			if (progress) progress(total, total);
			fclose(file);
			return true;
		}
		if (!q.fail && q.status_code == 308) {
			std::string range = q.get_header("range");
			size_t dash = range.find('-');
			offset = dash != std::string::npos ? strtoull(range.c_str() + dash + 1, NULL, 10) + 1 : 0;
		}
	}
	fclose(file);
	error = "the server did not confirm the upload";
	return false;
}

// ---------------------------------------------------------------------------------------------------------------------
// Data API helpers

// the raw (still escaped) text of the string that follows `key` ("key": "...") after `from`, "" if absent
static std::string raw_string(const std::string &json, size_t from, const std::string &key, size_t *end_pos = NULL) {
	size_t p = json.find("\"" + key + "\"", from);
	if (p == std::string::npos) return "";
	p = json.find('"', json.find(':', p));
	if (p == std::string::npos) return "";
	size_t e = p + 1;
	while (e < json.size() && json[e] != '"') {
		if (json[e] == '\\') e++;
		e++;
	}
	if (e >= json.size()) return "";
	if (end_pos) *end_pos = e;
	return json.substr(p + 1, e - p - 1);
}

static std::string unescape_json(const std::string &raw) {
	std::string out;
	for (size_t i = 0; i < raw.size(); i++) {
		if (raw[i] != '\\' || i + 1 >= raw.size()) {
			out += raw[i];
			continue;
		}
		char c = raw[++i];
		if (c == 'n') out += ' ';
		else if (c == 't') out += ' ';
		else if (c == 'u' && i + 4 < raw.size()) {
			unsigned code = strtoul(raw.substr(i + 1, 4).c_str(), NULL, 16);
			i += 4;
			if (code < 0x80) out += (char)code;
			else if (code < 0x800) {
				out += (char)(0xC0 | (code >> 6));
				out += (char)(0x80 | (code & 0x3F));
			} else {
				out += (char)(0xE0 | (code >> 12));
				out += (char)(0x80 | ((code >> 6) & 0x3F));
				out += (char)(0x80 | (code & 0x3F));
			}
		} else out += c; // \" \\ \/
	}
	return out;
}

// the API answer as text : the message of the error if there is one
static std::string api_error(const NetworkResult &r, const std::string &body) {
	if (r.fail) return r.error;
	std::string msg = unescape_json(raw_string(body, 0, "message"));
	return "http " + std::to_string(r.status_code) + (msg != "" ? " " + msg.substr(0, 120) : "");
}

static NetworkResult api_request(const std::string &method, const std::string &url, const std::string &body,
                                 const std::string &content_type, std::string &out_body) {
	std::map<std::string, std::string> headers = {{"Authorization", "Bearer " + OAuth::get_access_token()}};
	if (content_type != "") headers["Content-Type"] = content_type;
	NetworkResult r = studio_session().perform(
	    HttpRequest{method, url, HttpRequest::default_headers_added(headers), body, false});
	out_body.assign(r.data.begin(), r.data.end());
	return r;
}

bool youtube_studio_list_videos(std::vector<StudioVideo> &out, std::string &error) {
	if (!OAuth::is_authenticated()) {
		error = "not signed in";
		return false;
	}
	std::string body;
	auto r = api_request("GET", "https://www.googleapis.com/youtube/v3/channels?part=contentDetails&mine=true", "", "", body);
	std::string uploads = raw_string(body, body.find("\"uploads\"") == std::string::npos ? 0 : body.find("\"uploads\"") - 1, "uploads");
	if (r.fail || !r.status_code_is_success() || uploads == "") {
		error = api_error(r, body);
		logger.warning("studio", "uploads playlist failed : " + error + " " + body.substr(0, 200));
		return false;
	}
	r = api_request("GET", "https://www.googleapis.com/youtube/v3/playlistItems?part=snippet,status&maxResults=25&playlistId=" + uploads, "", "", body);
	if (r.fail || !r.status_code_is_success()) {
		error = api_error(r, body);
		logger.warning("studio", "uploads list failed : " + error);
		return false;
	}
	size_t pos = 0;
	const std::string marker = "\"youtube#playlistItem\"";
	while ((pos = body.find(marker, pos)) != std::string::npos) {
		size_t next = body.find(marker, pos + marker.size());
		std::string item = body.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
		StudioVideo v;
		size_t res = item.find("\"resourceId\"");
		v.id = res == std::string::npos ? "" : raw_string(item, res, "videoId");
		v.title = unescape_json(raw_string(item, 0, "title"));
		v.privacy = raw_string(item, 0, "privacyStatus");
		if (v.id != "") out.push_back(v);
		pos += marker.size();
	}
	return true;
}

bool youtube_studio_set_privacy(const std::string &video_id, const std::string &privacy, std::string &error) {
	std::string body;
	auto r = api_request("PUT", "https://www.googleapis.com/youtube/v3/videos?part=status",
	                     "{\"id\": " + json_quote(video_id) + ", \"status\": {\"privacyStatus\": " + json_quote(privacy) + "}}",
	                     "application/json; charset=UTF-8", body);
	if (r.fail || !r.status_code_is_success()) {
		error = api_error(r, body);
		logger.warning("studio", "set privacy failed : " + error);
		return false;
	}
	return true;
}

bool youtube_studio_set_title(const std::string &video_id, const std::string &title, std::string &error) {
	std::string body;
	// the update replaces the whole snippet, so the current description, category and tags are read first and sent back
	auto r = api_request("GET", "https://www.googleapis.com/youtube/v3/videos?part=snippet&id=" + video_id, "", "", body);
	if (r.fail || !r.status_code_is_success() || body.find("\"snippet\"") == std::string::npos) {
		error = api_error(r, body);
		return false;
	}
	size_t snippet = body.find("\"snippet\"");
	std::string category = raw_string(body, snippet, "categoryId");
	std::string description = raw_string(body, snippet, "description");
	std::string tags;
	size_t tp = body.find("\"tags\"", snippet);
	if (tp != std::string::npos) {
		size_t open = body.find('[', tp), close = body.find(']', tp);
		if (open != std::string::npos && close != std::string::npos && close > open) tags = body.substr(open, close - open + 1);
	}
	std::string snippet_json = "{\"title\": " + json_quote(title) + ", \"description\": \"" + description + "\", \"categoryId\": \"" +
	                           (category == "" ? "22" : category) + "\"" + (tags != "" ? ", \"tags\": " + tags : "") + "}";
	r = api_request("PUT", "https://www.googleapis.com/youtube/v3/videos?part=snippet",
	                "{\"id\": " + json_quote(video_id) + ", \"snippet\": " + snippet_json + "}", "application/json; charset=UTF-8", body);
	if (r.fail || !r.status_code_is_success()) {
		error = api_error(r, body);
		logger.warning("studio", "set title failed : " + error);
		return false;
	}
	return true;
}

bool youtube_studio_delete(const std::string &video_id, std::string &error) {
	std::string body;
	auto r = api_request("DELETE", "https://www.googleapis.com/youtube/v3/videos?id=" + video_id, "", "", body);
	if (r.fail || !r.status_code_is_success()) {
		error = api_error(r, body);
		logger.warning("studio", "delete failed : " + error);
		return false;
	}
	return true;
}

bool youtube_studio_set_thumbnail(const std::string &video_id, const std::string &path, std::string &error) {
	struct stat st;
	if (stat(path.c_str(), &st) != 0 || st.st_size <= 0 || st.st_size > 2 * 1024 * 1024) {
		error = "the image must be up to 2 MB";
		return false;
	}
	FILE *f = fopen(path.c_str(), "rb");
	if (!f) {
		error = "cannot open the image";
		return false;
	}
	std::string data((size_t)st.st_size, '\0');
	size_t got = fread(&data[0], 1, data.size(), f);
	fclose(f);
	if (got != data.size()) {
		error = "cannot read the image";
		return false;
	}
	std::string ext = path.substr(path.find_last_of('.') == std::string::npos ? path.size() : path.find_last_of('.') + 1);
	for (auto &c : ext) c = tolower(c);
	std::string body;
	auto r = api_request("POST", "https://www.googleapis.com/upload/youtube/v3/thumbnails/set?uploadType=media&videoId=" + video_id,
	                     data, ext == "png" ? "image/png" : "image/jpeg", body);
	if (r.fail || !r.status_code_is_success()) {
		error = api_error(r, body);
		logger.warning("studio", "thumbnail failed : " + error);
		return false;
	}
	return true;
}
