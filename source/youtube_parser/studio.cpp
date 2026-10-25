#include "headers.hpp"
#include "util/json_text.hpp"
#include "youtube_parser/studio.hpp"
#include "youtube_parser/internal_common.hpp"
#include "oauth/oauth.hpp"
#include "youtube_parser/studio_auth.hpp"
#include <sys/stat.h>
#include <algorithm>
#include <cstring>

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

// the text of a field written like  "viewCountText":"1,234 views"  without its unit
static std::string count_text(const std::string &json, const std::string &key) {
	size_t p = json.find("\"" + key + "\":\"");
	if (p == std::string::npos) return "";
	p += key.size() + 4;
	size_t e = json.find('"', p);
	if (e == std::string::npos) return "";
	std::string t = json.substr(p, e - p);
	for (const char *unit : {" views", " view", " videos", " video", " subscribers", " subscriber"}) {
		size_t u = t.find(unit);
		if (u != std::string::npos && u + strlen(unit) == t.size()) t.erase(u);
	}
	return t;
}

// The totals come from the public About page of the channel (no Data API needed) : the channel page has the continuation
// tokens, one of them opens the About panel with the totals.
#define STATS_CACHE_PATH "sdmc:/3ds/IkersTube/studio_stats.txt"

static void stats_cache_save(const StudioStats &s) {
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	FILE *f = fopen(STATS_CACHE_PATH, "w");
	if (!f) return;
	fprintf(f, "%s\n%s\n%s\n", s.subscribers.c_str(), s.views.c_str(), s.videos.c_str());
	fclose(f);
}

// the numbers of the last time, shown when they cannot be loaded now (no connection)
static void stats_cache_load(StudioStats &s) {
	FILE *f = fopen(STATS_CACHE_PATH, "r");
	if (!f) return;
	char a[64] = {0}, b[64] = {0}, c[64] = {0};
	if (fgets(a, sizeof a, f) && fgets(b, sizeof b, f) && fgets(c, sizeof c, f)) {
		auto trim = [](char *x) { size_t n = strlen(x); while (n && (x[n - 1] == '\n' || x[n - 1] == '\r')) x[--n] = 0; return std::string(x); };
		s.subscribers = trim(a);
		s.views = trim(b);
		s.videos = trim(c);
	}
	fclose(f);
}

static StudioStats load_stats_online();

StudioStats youtube_studio_stats() {
	StudioStats res = load_stats_online();
	if (res.error == "") {
		stats_cache_save(res);
	} else {
		StudioStats cached;
		stats_cache_load(cached);
		if (cached.views != "-") { // keep the old numbers and say they are old
			res.subscribers = cached.subscribers;
			res.views = cached.views;
			res.videos = cached.videos;
			res.error += " - old numbers";
		}
	}
	return res;
}

static StudioStats load_stats_online() {
	StudioStats res;
	std::string channel = OAuth::get_user_channel_id();
	if (channel == "") {
		res.error = "channel unknown (sign in again)";
		return res;
	}
	if (OAuth::get_user_subscriber_count() != "") res.subscribers = OAuth::get_user_subscriber_count();
	auto browse = [&](const std::string &fields) {
		std::string body = std::string(R"({"context": {"client": {"hl": "en","gl": "US","clientName": "MWEB","clientVersion": "2.20241202.07.00"}}, )") + fields + "}";
		auto r = studio_session().perform(
		    HttpRequest::POST(get_innertube_api_url("browse"), {{"Content-Type", "application/json"}}, body));
		return std::string(r.data.begin(), r.data.end());
	};
	std::string page = browse("\"browseId\": \"" + channel + "\"");
	std::vector<std::string> tokens;
	const std::string key = "\"continuationCommand\":{\"token\":\"";
	for (size_t pos = 0; tokens.size() < 6 && (pos = page.find(key, pos)) != std::string::npos;) {
		pos += key.size();
		size_t e = page.find('"', pos);
		if (e == std::string::npos) break;
		std::string t = page.substr(pos, e - pos);
		if (std::find(tokens.begin(), tokens.end(), t) == tokens.end()) tokens.push_back(t);
	}
	for (auto &token : tokens) {
		std::string about = browse("\"continuation\": \"" + token + "\"");
		if (about.find("aboutChannelViewModel") == std::string::npos) continue;
		std::string views = count_text(about, "viewCountText"), videos = count_text(about, "videoCountText"),
		            subs = count_text(about, "subscriberCountText");
		if (views != "") res.views = views;
		if (videos != "") res.videos = videos;
		if (subs != "") res.subscribers = subs;
		return res;
	}
	logger.warning("studio", "no About panel found for the channel " + channel + " (" + std::to_string(tokens.size()) + " tokens)");
	res.error = "no About data";
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

// days since 1970-01-01 of a civil date (no timegm on the console)
static long long days_from_civil(long long y, unsigned m, unsigned d) {
	y -= m <= 2;
	const long long era = (y >= 0 ? y : y - 399) / 400;
	const unsigned yoe = (unsigned)(y - era * 400);
	const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + (long long)doe - 719468;
}

bool youtube_studio_utc_now(long long &epoch) {
	auto r = studio_session().perform(HttpRequest::GET("https://www.google.com/generate_204", {}));
	std::string date = r.get_header("date"); // "Sat, 17 Oct 2026 16:00:00 GMT"
	char month_name[4] = {0};
	int day = 0, year = 0, hour = 0, minute = 0, second = 0;
	if (r.fail || sscanf(date.c_str(), "%*3s, %d %3s %d %d:%d:%d", &day, month_name, &year, &hour, &minute, &second) != 6) return false;
	static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	int month = 0;
	for (int i = 0; i < 12; i++) {
		if (strncmp(month_name, months[i], 3) == 0) month = i + 1;
	}
	if (!month) return false;
	epoch = days_from_civil(year, month, day) * 86400LL + hour * 3600 + minute * 60 + second;
	return true;
}

#define PENDING_UPLOAD_PATH "sdmc:/3ds/IkersTube/studio_upload.txt"

static void save_pending_upload(const std::string &path, const std::string &title, const std::string &url) {
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	FILE *f = fopen(PENDING_UPLOAD_PATH, "w");
	if (!f) return;
	std::string clean_title = title;
	for (auto &c : clean_title) {
		if (c == '\n' || c == '\r') c = ' ';
	}
	fprintf(f, "%s\n%s\n%s\n", url.c_str(), path.c_str(), clean_title.c_str());
	fclose(f);
}

void youtube_studio_clear_pending_upload() { remove(PENDING_UPLOAD_PATH); }

bool youtube_studio_pending_upload(std::string &path, std::string &title, std::string &upload_url) {
	FILE *f = fopen(PENDING_UPLOAD_PATH, "r");
	if (!f) return false;
	char a[1024] = {0}, b[1024] = {0}, c[512] = {0};
	bool ok = fgets(a, sizeof a, f) && fgets(b, sizeof b, f);
	if (ok) fgets(c, sizeof c, f);
	fclose(f);
	auto trim = [](std::string s) { while (s.size() && (s.back() == '\n' || s.back() == '\r')) s.pop_back(); return s; };
	upload_url = trim(a);
	path = trim(b);
	title = trim(c);
	struct stat st;
	return ok && upload_url != "" && stat(path.c_str(), &st) == 0; // the file must still be there
}

bool youtube_studio_upload(const std::string &path, const std::string &title, const std::string &description,
                           const std::string &privacy, bool is_short, const std::string &category_id, const std::string &tags, int publish_in_seconds,
                           const std::string &resume_url, const std::function<void(u64, u64)> &progress, std::string &video_id,
                           std::string &error) {
	if (studio_bearer_token() == "") {
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
	std::string tags_json; // "a, b, c" -> ["a","b","c"]
	{
		std::string cur;
		int count = 0;
		for (char c : tags + ",") {
			if (c == ',') {
				while (cur.size() && isspace((unsigned char)cur.back())) cur.pop_back();
				size_t first = 0;
				while (first < cur.size() && isspace((unsigned char)cur[first])) first++;
				cur = cur.substr(first);
				if (cur != "" && count < 15) {
					tags_json += (count ? ", " : "") + json_quote(cur);
					count++;
				}
				cur = "";
			} else {
				cur += c;
			}
		}
	}
	std::string publish_at;
	std::string used_privacy = privacy;
	if (publish_in_seconds > 0) { // scheduled : private until the moment, using the time of the internet
		long long now = 0;
		if (!youtube_studio_utc_now(now)) {
			error = "could not get the time from the internet";
			fclose(file);
			return false;
		}
		time_t when = (time_t)(now + publish_in_seconds);
		struct tm *t = gmtime(&when);
		char buf[40];
		snprintf(buf, sizeof buf, "%04d-%02d-%02dT%02d:%02d:%02dZ", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
		publish_at = buf;
		used_privacy = "private";
	}
	std::string meta = "{\"snippet\": {\"title\": " + json_quote(full_title) + ", \"description\": " + json_quote(full_description) +
	                   ", \"categoryId\": " + json_quote(category_id) + (tags_json != "" ? ", \"tags\": [" + tags_json + "]" : "") +
	                   "}, \"status\": {\"privacyStatus\": " + json_quote(used_privacy) + (publish_at != "" ? ", \"publishAt\": " + json_quote(publish_at) : "") + ", \"selfDeclaredMadeForKids\": false}}";
	std::string upload_url = resume_url;
	u64 resume_offset = 0;
	if (upload_url == "") {
		auto init = session.perform(HttpRequest::POST(
		    "https://www.googleapis.com/upload/youtube/v3/videos?uploadType=resumable&part=snippet,status",
		    {{"Authorization", "Bearer " + studio_bearer_token()},
		     {"Content-Type", "application/json; charset=UTF-8"},
		     {"X-Upload-Content-Length", std::to_string(total)},
		     {"X-Upload-Content-Type", mime_of(path)}},
		    meta));
		upload_url = init.get_header("location");
		if (init.fail || !init.status_code_is_success() || upload_url == "") {
			error = init.fail ? init.error : "http " + std::to_string(init.status_code);
			std::string body(init.data.begin(), init.data.end());
			logger.warning("studio", "upload start failed : " + error + " " + body.substr(0, 400));
			size_t m = body.find("\"message\"");
			if (m != std::string::npos) error += " " + body.substr(m, 160);
			fclose(file);
			return false;
		}
		save_pending_upload(path, title, upload_url); // so it can be continued if it is interrupted
	} else { // continuing : the server says how much it already has
		auto q = session.perform(HttpRequest{"PUT", upload_url,
		                                     HttpRequest::default_headers_added({{"Authorization", "Bearer " + studio_bearer_token()},
		                                                                         {"Content-Range", "bytes */" + std::to_string(total)}}),
		                                     "", false});
		if (!q.fail && (q.status_code == 200 || q.status_code == 201)) { // it had finished
			std::string resp(q.data.begin(), q.data.end());
			size_t p = resp.find("\"id\"");
			if (p != std::string::npos) {
				p = resp.find('"', resp.find(':', p));
				size_t e = resp.find('"', p + 1);
				if (p != std::string::npos && e != std::string::npos) video_id = resp.substr(p + 1, e - p - 1);
			}
			youtube_studio_clear_pending_upload();
			if (progress) progress(total, total);
			fclose(file);
			return true;
		}
		if (q.fail || q.status_code != 308) {
			error = q.fail ? q.error : "http " + std::to_string(q.status_code);
			if (q.status_code == 404 || q.status_code == 410) youtube_studio_clear_pending_upload(); // the session expired
			fclose(file);
			return false;
		}
		std::string range = q.get_header("range");
		size_t dash = range.find('-');
		resume_offset = dash != std::string::npos ? strtoull(range.c_str() + dash + 1, NULL, 10) + 1 : 0;
		if (resume_offset > total) resume_offset = 0;
	}

	const u64 CHUNK = 512 * 1024; // a multiple of 256 KiB, as the upload protocol asks
	u64 offset = resume_offset;
	std::vector<char> buf(CHUNK);
	int failures = 0;
	int no_progress = 0; // answers in a row that did not move the offset forward
	while (offset < total) {
		u64 len = std::min<u64>(CHUNK, total - offset);
		if (fseeko(file, (off_t)offset, SEEK_SET) != 0) { // (64-bit offsets: a file can be up to 4 GB)
			error = "cannot seek in the file";
			fclose(file);
			return false;
		}
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
		                                         {{"Authorization", "Bearer " + studio_bearer_token()},
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
			youtube_studio_clear_pending_upload();
			if (progress) progress(total, total);
			fclose(file);
			return true;
		}
		if (!r.fail && r.status_code == 308) { // chunk accepted
			std::string range = r.get_header("range"); // "bytes=0-1234" : what the server really has
			size_t dash = range.find('-');
			u64 have = dash != std::string::npos ? strtoull(range.c_str() + dash + 1, NULL, 10) + 1 : 0; // no Range : nothing stored
			u64 previous_offset = offset;
			offset = have <= total ? have : offset + len;
			if (offset <= previous_offset) { // the server stores nothing more: it must not go on forever
				if (++no_progress >= 5) {
					error = "the upload makes no progress";
					fclose(file);
					return false;
				}
			} else {
				no_progress = 0;
			}
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
		                                     HttpRequest::default_headers_added({{"Authorization", "Bearer " + studio_bearer_token()},
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

static std::string unescape_json(const std::string &raw) { return json_text_unescape(raw); }

// the API answer as text : the message of the error if there is one
static std::string api_error(const NetworkResult &r, const std::string &body) {
	if (r.fail) return r.error;
	std::string msg = unescape_json(raw_string(body, 0, "message"));
	return "http " + std::to_string(r.status_code) + (msg != "" ? " " + msg.substr(0, 120) : "");
}

static NetworkResult api_request(const std::string &method, const std::string &url, const std::string &body,
                                 const std::string &content_type, std::string &out_body) {
	std::map<std::string, std::string> headers = {{"Authorization", "Bearer " + studio_bearer_token()}};
	if (content_type != "") headers["Content-Type"] = content_type;
	NetworkResult r = studio_session().perform(
	    HttpRequest{method, url, HttpRequest::default_headers_added(headers), body, false});
	out_body.assign(r.data.begin(), r.data.end());
	return r;
}

bool youtube_studio_list_videos(std::vector<StudioVideo> &out, std::string &error) {
	if (studio_bearer_token() == "") {
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
	// the view counts of those videos, in one request
	if (out.size()) {
		std::string ids;
		for (auto &v : out) ids += (ids == "" ? "" : ",") + v.id;
		std::string stats;
		auto sr = api_request("GET", "https://www.googleapis.com/youtube/v3/videos?part=statistics&maxResults=25&id=" + ids, "", "", stats);
		if (!sr.fail && sr.status_code_is_success()) {
			for (auto &v : out) {
				size_t at = stats.find("\"id\": \"" + v.id + "\"");
				if (at == std::string::npos) continue;
				size_t end = stats.find("\"kind\": \"youtube#video\"", at + 10);
				unsigned long long views = 0;
				std::string chunk = stats.substr(at, end == std::string::npos ? std::string::npos : end - at);
				if (number_field(chunk, "viewCount", views)) v.views = short_number(views);
			}
		}
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
	std::string default_language = raw_string(body, snippet, "defaultLanguage"); // kept as it was
	std::string description = raw_string(body, snippet, "description");
	std::string tags;
	size_t tp = body.find("\"tags\"", snippet);
	if (tp != std::string::npos) {
		size_t open = body.find('[', tp), close = body.find(']', tp);
		if (open != std::string::npos && close != std::string::npos && close > open) tags = body.substr(open, close - open + 1);
	}
	std::string snippet_json = "{\"title\": " + json_quote(title) + ", \"description\": \"" + description + "\", \"categoryId\": \"" +
	                           (category == "" ? "22" : category) + "\"" + (tags != "" ? ", \"tags\": " + tags : "") +
	                           (default_language != "" ? ", \"defaultLanguage\": \"" + default_language + "\"" : "") + "}";
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

// ---- comments, ratings and lists through the Data API (more reliable than the app login for writing) ----
static bool api_json(const std::string &method, const std::string &url, const std::string &body, std::string &error, std::string *out = NULL) {
	if (studio_bearer_token() == "") {
		error = "not signed in for Studio";
		return false;
	}
	std::string answer;
	auto r = api_request(method, url, body, body != "" ? "application/json" : "", answer);
	if (out) *out = answer;
	if (r.fail || !r.status_code_is_success()) {
		error = api_error(r, answer);
		logger.warning("studio", method + " " + url.substr(0, 70) + " failed : " + error);
		return false;
	}
	return true;
}

bool youtube_studio_comment_insert(const std::string &video_id, const std::string &text, std::string &error) {
	return api_json("POST", "https://www.googleapis.com/youtube/v3/commentThreads?part=snippet",
	                "{\"snippet\": {\"videoId\": " + json_quote(video_id) + ", \"topLevelComment\": {\"snippet\": {\"textOriginal\": " + json_quote(text) + "}}}}", error);
}

bool youtube_studio_comment_reply(const std::string &parent_id, const std::string &text, std::string &error) {
	return api_json("POST", "https://www.googleapis.com/youtube/v3/comments?part=snippet",
	                "{\"snippet\": {\"parentId\": " + json_quote(parent_id) + ", \"textOriginal\": " + json_quote(text) + "}}", error);
}

bool youtube_studio_comment_delete(const std::string &comment_id, std::string &error) {
	return api_json("DELETE", "https://www.googleapis.com/youtube/v3/comments?id=" + comment_id, "", error);
}

bool youtube_studio_live_chat_send(const std::string &video_id, const std::string &text, std::string &error) {
	std::string answer;
	if (!api_json("GET", "https://www.googleapis.com/youtube/v3/videos?part=liveStreamingDetails&id=" + video_id, "", error, &answer)) return false;
	size_t p = answer.find("\"activeLiveChatId\"");
	std::string chat_id = p == std::string::npos ? "" : raw_string(answer, p, "activeLiveChatId");
	if (chat_id == "") {
		error = "this video has no active chat";
		return false;
	}
	return api_json("POST", "https://www.googleapis.com/youtube/v3/liveChat/messages?part=snippet",
	                "{\"snippet\": {\"liveChatId\": " + json_quote(chat_id) + ", \"type\": \"textMessageEvent\", \"textMessageDetails\": {\"messageText\": " + json_quote(text) + "}}}", error);
}

bool youtube_studio_rate(const std::string &video_id, const std::string &rating, std::string &error) {
	return api_json("POST", "https://www.googleapis.com/youtube/v3/videos/rate?id=" + video_id + "&rating=" + rating, "", error);
}

bool youtube_studio_set_description(const std::string &video_id, const std::string &description, std::string &error) {
	std::string body;
	auto r = api_request("GET", "https://www.googleapis.com/youtube/v3/videos?part=snippet&id=" + video_id, "", "", body);
	if (r.fail || !r.status_code_is_success() || body.find("\"snippet\"") == std::string::npos) {
		error = api_error(r, body);
		return false;
	}
	size_t snippet = body.find("\"snippet\"");
	std::string category = raw_string(body, snippet, "categoryId");
	std::string default_language = raw_string(body, snippet, "defaultLanguage"); // kept as it was
	std::string title = raw_string(body, snippet, "title"); // (still escaped, as it came)
	std::string tags;
	size_t tp = body.find("\"tags\"", snippet);
	if (tp != std::string::npos) {
		size_t open = body.find('[', tp), close = body.find(']', tp);
		if (open != std::string::npos && close != std::string::npos && close > open) tags = body.substr(open, close - open + 1);
	}
	std::string snippet_json = "{\"title\": \"" + title + "\", \"description\": " + json_quote(description) + ", \"categoryId\": \"" +
	                           (category == "" ? "22" : category) + "\"" + (tags != "" ? ", \"tags\": " + tags : "") +
	                           (default_language != "" ? ", \"defaultLanguage\": \"" + default_language + "\"" : "") + "}";
	r = api_request("PUT", "https://www.googleapis.com/youtube/v3/videos?part=snippet",
	                "{\"id\": " + json_quote(video_id) + ", \"snippet\": " + snippet_json + "}", "application/json; charset=UTF-8", body);
	if (r.fail || !r.status_code_is_success()) {
		error = api_error(r, body);
		logger.warning("studio", "set description failed : " + error);
		return false;
	}
	return true;
}

bool youtube_studio_set_tags(const std::string &video_id, const std::string &comma_separated_tags, std::string &error) {
	std::string body;
	auto r = api_request("GET", "https://www.googleapis.com/youtube/v3/videos?part=snippet&id=" + video_id, "", "", body);
	if (r.fail || !r.status_code_is_success() || body.find("\"snippet\"") == std::string::npos) {
		error = api_error(r, body);
		return false;
	}
	size_t snippet = body.find("\"snippet\"");
	std::string category = raw_string(body, snippet, "categoryId");
	std::string title = raw_string(body, snippet, "title");
	std::string description = raw_string(body, snippet, "description");
	std::string default_language = raw_string(body, snippet, "defaultLanguage");
	std::string tags_json;
	size_t start = 0;
	while (start <= comma_separated_tags.size()) {
		size_t comma = comma_separated_tags.find(',', start);
		std::string tag = comma_separated_tags.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
		while (tag.size() && tag[0] == ' ') tag.erase(0, 1);
		while (tag.size() && tag.back() == ' ') tag.pop_back();
		if (tag != "") tags_json += (tags_json == "" ? "" : ", ") + json_quote(tag);
		if (comma == std::string::npos) break;
		start = comma + 1;
	}
	std::string snippet_json = "{\"title\": \"" + title + "\", \"description\": \"" + description + "\", \"categoryId\": \"" + (category == "" ? "22" : category) + "\"" +
	                           (tags_json != "" ? ", \"tags\": [" + tags_json + "]" : "") + (default_language != "" ? ", \"defaultLanguage\": \"" + default_language + "\"" : "") + "}";
	r = api_request("PUT", "https://www.googleapis.com/youtube/v3/videos?part=snippet",
	                "{\"id\": " + json_quote(video_id) + ", \"snippet\": " + snippet_json + "}", "application/json; charset=UTF-8", body);
	if (r.fail || !r.status_code_is_success()) {
		error = api_error(r, body);
		logger.warning("studio", "set tags failed : " + error);
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

bool youtube_studio_video_stats(const std::string &video_id, std::string &views, std::string &likes, std::string &comments) {
	std::string body;
	auto r = api_request("GET", "https://www.googleapis.com/youtube/v3/videos?part=statistics&id=" + video_id, "", "", body);
	if (r.fail || !r.status_code_is_success()) return false;
	unsigned long long n = 0;
	views = number_field(body, "viewCount", n) ? short_number(n) : "-";
	likes = number_field(body, "likeCount", n) ? short_number(n) : "-";
	comments = number_field(body, "commentCount", n) ? short_number(n) : "-";
	return true;
}
