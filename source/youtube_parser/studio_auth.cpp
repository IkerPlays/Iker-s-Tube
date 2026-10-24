#include "headers.hpp"
#include "youtube_parser/studio_auth.hpp"
#include "youtube_parser/internal_common.hpp"
#include "oauth/oauth.hpp"
#include <sys/stat.h>

#define CLIENT_PATH "sdmc:/3ds/IkersTube/studio_client.txt"
#define TOKENS_PATH "sdmc:/3ds/IkersTube/studio_tokens.txt"

// answers of the token endpoint go to the log only with the token values hidden
static std::string redact_tokens(std::string text) {
	for (const char *key : {"\"access_token\"", "\"refresh_token\"", "\"id_token\""}) {
		size_t at = 0;
		while ((at = text.find(key, at)) != std::string::npos) {
			size_t colon = text.find(':', at);
			if (colon == std::string::npos) break; // (no value after the key)
			size_t open = text.find('"', colon + 1);
			size_t close = open == std::string::npos ? open : text.find('"', open + 1);
			if (close == std::string::npos) { text = text.substr(0, at); break; }
			text.replace(open + 1, close - open - 1, "***");
			at = open + 4;
		}
	}
	return text;
}

static LightLock auth_lock;
static struct AuthLockInit {
	AuthLockInit() { LightLock_Init(&auth_lock); }
} auth_lock_init;

static std::string client_id, client_secret; // read from the SD card
static std::string refresh_token, access_token;
static u64 access_expires_ms = 0;
static bool loaded = false;

static NetworkSessionList &auth_session() {
	static NetworkSessionList session;
	if (!session.inited) session.init();
	return session;
}

static std::string trim(std::string s) {
	while (s.size() && isspace((unsigned char)s.back())) s.pop_back();
	size_t i = 0;
	while (i < s.size() && isspace((unsigned char)s[i])) i++;
	return s.substr(i);
}

static std::string read_line(FILE *f) {
	char buf[512];
	if (!fgets(buf, sizeof buf, f)) return "";
	return trim(buf);
}

#ifndef STUDIO_CLIENT_ID
#define STUDIO_CLIENT_ID ""
#endif
#ifndef STUDIO_CLIENT_SECRET
#define STUDIO_CLIENT_SECRET ""
#endif

static void load_locked() {
	if (loaded) return;
	loaded = true;
	client_id = STUDIO_CLIENT_ID; // built into the app (GitHub secrets of the build)
	client_secret = STUDIO_CLIENT_SECRET;
	FILE *f = fopen(CLIENT_PATH, "r");
	if (f && (client_id == "" || client_secret == "")) { // or the one set up on this console
		client_id = read_line(f);
		client_secret = read_line(f);
	}
	if (f) fclose(f);
	f = fopen(TOKENS_PATH, "r");
	if (f) {
		refresh_token = read_line(f);
		fclose(f);
	}
}

static void save_tokens_locked() {
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	if (refresh_token == "") {
		remove(TOKENS_PATH);
		return;
	}
	FILE *f = fopen(TOKENS_PATH, "w");
	if (!f) return;
	fprintf(f, "%s\n", refresh_token.c_str());
	fclose(f);
}

// the text of a string field of a small JSON answer ("key": "value") and the number of a numeric one
static std::string json_text(const std::string &json, const std::string &key) {
	size_t p = json.find("\"" + key + "\"");
	if (p == std::string::npos) return "";
	p = json.find(':', p);
	if (p == std::string::npos) return "";
	p++;
	while (p < json.size() && (json[p] == ' ' || json[p] == '\n')) p++;
	if (p >= json.size()) return "";
	if (json[p] == '"') {
		size_t e = json.find('"', p + 1);
		return e == std::string::npos ? "" : json.substr(p + 1, e - p - 1);
	}
	size_t e = p;
	while (e < json.size() && (isalnum((unsigned char)json[e]) || json[e] == '.')) e++;
	return json.substr(p, e - p);
}

static std::string form_encode(const std::string &s) {
	std::string out;
	char buf[4];
	for (unsigned char c : s) {
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
		else {
			snprintf(buf, sizeof buf, "%%%02X", c);
			out += buf;
		}
	}
	return out;
}

static NetworkResult post_form(const std::string &url, const std::string &body) {
	return auth_session().perform(
	    HttpRequest::POST(url, {{"Content-Type", "application/x-www-form-urlencoded"}}, body));
}

bool studio_auth_set_client(const std::string &id, const std::string &secret) {
	std::string i = trim(id), s = trim(secret);
	if (i == "" || s == "") return false;
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	FILE *f = fopen(CLIENT_PATH, "w");
	if (!f) return false;
	fprintf(f, "%s\n%s\n", i.c_str(), s.c_str());
	fclose(f);
	LightLock_Lock(&auth_lock);
	loaded = false; // read again (the one built into the app still wins)
	client_id = client_secret = refresh_token = access_token = "";
	load_locked();
	LightLock_Unlock(&auth_lock);
	return true;
}

bool studio_auth_client_built_in() { return std::string(STUDIO_CLIENT_ID) != "" && std::string(STUDIO_CLIENT_SECRET) != ""; }

bool studio_auth_has_client() {
	LightLock_Lock(&auth_lock);
	load_locked();
	bool ok = client_id != "" && client_secret != "";
	LightLock_Unlock(&auth_lock);
	return ok;
}

bool studio_auth_signed_in() {
	LightLock_Lock(&auth_lock);
	load_locked();
	bool ok = refresh_token != "" && client_id != "";
	LightLock_Unlock(&auth_lock);
	return ok;
}

void studio_auth_sign_out() {
	LightLock_Lock(&auth_lock);
	load_locked();
	refresh_token = "";
	access_token = "";
	access_expires_ms = 0;
	save_tokens_locked();
	LightLock_Unlock(&auth_lock);
}

std::string studio_auth_access_token() {
	LightLock_Lock(&auth_lock);
	load_locked();
	if (refresh_token == "" || client_id == "") {
		LightLock_Unlock(&auth_lock);
		return "";
	}
	if (access_token != "" && osGetTime() + 60000 < access_expires_ms) {
		std::string t = access_token;
		LightLock_Unlock(&auth_lock);
		return t;
	}
	std::string body = "client_id=" + form_encode(client_id) + "&client_secret=" + form_encode(client_secret) +
	                   "&refresh_token=" + form_encode(refresh_token) + "&grant_type=refresh_token";
	LightLock_Unlock(&auth_lock);
	auto r = post_form("https://oauth2.googleapis.com/token", body);
	std::string answer(r.data.begin(), r.data.end());
	std::string token = json_text(answer, "access_token");
	LightLock_Lock(&auth_lock);
	if (token == "") {
		logger.warning("studio-auth", "refresh failed : http " + std::to_string(r.status_code) + " " + redact_tokens(answer).substr(0, 200));
		access_token = "";
	} else {
		access_token = token;
		access_expires_ms = osGetTime() + (u64)atoi(json_text(answer, "expires_in").c_str()) * 1000;
	}
	std::string res = access_token;
	LightLock_Unlock(&auth_lock);
	return res;
}

std::string studio_bearer_token() {
	std::string t = studio_auth_access_token();
	if (t != "") return t;
	return OAuth::is_authenticated() ? OAuth::get_fresh_access_token() : "";
}

bool studio_auth_request_code(StudioDeviceCode &out, std::string &error) {
	LightLock_Lock(&auth_lock);
	load_locked();
	std::string id = client_id;
	LightLock_Unlock(&auth_lock);
	if (id == "") {
		error = "no studio_client.txt";
		return false;
	}
	auto r = post_form("https://oauth2.googleapis.com/device/code",
	                   "client_id=" + form_encode(id) + "&scope=" + form_encode("https://www.googleapis.com/auth/youtube"));
	std::string answer(r.data.begin(), r.data.end());
	out.device_code = json_text(answer, "device_code");
	out.user_code = json_text(answer, "user_code");
	out.url = json_text(answer, "verification_url");
	if (out.url == "") out.url = json_text(answer, "verification_uri");
	out.interval = std::max(3, atoi(json_text(answer, "interval").c_str()));
	out.expires_in = atoi(json_text(answer, "expires_in").c_str());
	if (r.fail || out.device_code == "" || out.user_code == "") {
		std::string msg = json_text(answer, "error_description");
		if (msg == "") msg = json_text(answer, "error");
		error = r.fail ? r.error : "http " + std::to_string(r.status_code) + " " + msg;
		logger.warning("studio-auth", "device code failed : " + error + " " + redact_tokens(answer).substr(0, 200));
		return false;
	}
	return true;
}

int studio_auth_poll(const StudioDeviceCode &code, std::string &error) {
	LightLock_Lock(&auth_lock);
	load_locked();
	std::string id = client_id, secret = client_secret;
	LightLock_Unlock(&auth_lock);
	auto r = post_form("https://oauth2.googleapis.com/token",
	                   "client_id=" + form_encode(id) + "&client_secret=" + form_encode(secret) + "&device_code=" +
	                       form_encode(code.device_code) + "&grant_type=" + form_encode("urn:ietf:params:oauth:grant-type:device_code"));
	std::string answer(r.data.begin(), r.data.end());
	std::string token = json_text(answer, "access_token");
	if (token != "") {
		LightLock_Lock(&auth_lock);
		access_token = token;
		access_expires_ms = osGetTime() + (u64)atoi(json_text(answer, "expires_in").c_str()) * 1000;
		std::string refresh = json_text(answer, "refresh_token");
		if (refresh != "") refresh_token = refresh;
		save_tokens_locked();
		LightLock_Unlock(&auth_lock);
		return 1;
	}
	std::string err = json_text(answer, "error");
	if (err == "authorization_pending" || err == "slow_down") return 0;
	error = err != "" ? err : (r.fail ? r.error : "http " + std::to_string(r.status_code));
	logger.warning("studio-auth", "poll failed : " + error + " " + redact_tokens(answer).substr(0, 200));
	return -1;
}
