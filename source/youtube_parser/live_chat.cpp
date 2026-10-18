#include "headers.hpp"
#include "youtube_parser/live_chat.hpp"
#include "youtube_parser/studio.hpp"
#include "internal_common.hpp"
#include <algorithm>
#include <cstring>
#include <deque>
#include <regex>

static LightLock chat_lock;
static struct ChatLockInit {
	ChatLockInit() { LightLock_Init(&chat_lock); }
} chat_lock_init;
static std::deque<ChatMessage> messages;
static std::string chat_status;
static std::string chat_video;
static unsigned chat_version = 0;
static volatile bool chat_stop = true;
static volatile int chat_generation = 0; // a thread of an older open/close sees it changed and ends
static volatile bool chat_thread_running = false;

// the text of a JSON string that starts right after `key` (key ends with the opening quote), unescaped
static std::string string_after(const std::string &s, size_t from, size_t to, const std::string &key, size_t *end_pos = NULL) {
	size_t p = s.find(key, from);
	if (p == std::string::npos || p >= to) return "";
	p += key.size();
	std::string res;
	size_t i = p;
	for (; i < s.size() && s[i] != '"'; i++) {
		if (s[i] != '\\' || i + 1 >= s.size()) {
			res += s[i];
			continue;
		}
		char c = s[++i];
		if (c == 'n' || c == 't') res += ' ';
		else if (c == 'u' && i + 4 < s.size()) {
			unsigned cp = (unsigned)strtoul(s.substr(i + 1, 4).c_str(), NULL, 16);
			i += 4;
			if (cp >= 0xD800 && cp < 0xDC00 && i + 6 < s.size() && s[i + 1] == '\\' && s[i + 2] == 'u') {
				unsigned low = (unsigned)strtoul(s.substr(i + 3, 4).c_str(), NULL, 16);
				cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
				i += 6;
			}
			if (cp < 0x80) res += (char)cp;
			else if (cp < 0x800) res += (char)(0xC0 | (cp >> 6)), res += (char)(0x80 | (cp & 0x3F));
			else if (cp < 0x10000) res += (char)(0xE0 | (cp >> 12)), res += (char)(0x80 | ((cp >> 6) & 0x3F)), res += (char)(0x80 | (cp & 0x3F));
			else res += (char)(0xF0 | (cp >> 18)), res += (char)(0x80 | ((cp >> 12) & 0x3F)), res += (char)(0x80 | ((cp >> 6) & 0x3F)), res += (char)(0x80 | (cp & 0x3F));
		} else res += c; // \" \\ \/
	}
	if (end_pos) *end_pos = i;
	return res;
}

static void set_status(const std::string &text) {
	LightLock_Lock(&chat_lock);
	chat_status = text;
	chat_version++;
	LightLock_Unlock(&chat_lock);
	var_need_refresh = true;
}

static std::string context_json() {
	std::string c = R"("context": {"client": {"hl": "%0","gl": "%1","clientName": "WEB","clientVersion": "2.20241126.01.00","utcOffsetMinutes": 0}})";
	c = std::regex_replace(c, std::regex("%0"), language_code);
	c = std::regex_replace(c, std::regex("%1"), country_code);
	return c;
}

static void chat_thread(void *arg) {
	int generation = (int)(intptr_t)arg;
	std::string video;
	LightLock_Lock(&chat_lock);
	video = chat_video;
	LightLock_Unlock(&chat_lock);
	auto alive = [&]() { return !chat_stop && chat_generation == generation; };

	// the first continuation comes with the page of the video
	std::string continuation;
	auto page = http_post_json(get_innertube_api_url("next"), "{" + context_json() + ", \"videoId\": \"" + video + "\"}");
	if (page.first) {
		size_t r = page.second.find("\"liveChatRenderer\"");
		if (r != std::string::npos) continuation = string_after(page.second, r, page.second.size(), "\"continuation\":\"");
	}
	if (continuation == "") {
		set_status(tr("This video has no live chat", "Este vídeo no tiene chat en directo"));
		chat_thread_running = false;
		return;
	}
	set_status("");
	std::vector<std::string> seen;
	int failures = 0;
	while (alive()) {
		auto r = http_post_json(get_innertube_api_url("live_chat/get_live_chat"),
		                        "{" + context_json() + ", \"continuation\": \"" + continuation + "\"}");
		int wait_ms = 4000;
		if (!r.first) {
			if (++failures >= 5) {
				set_status(tr("The chat stopped (no connection?)", "El chat se detuvo (¿sin conexión?)"));
				break;
			}
		} else {
			failures = 0;
			const std::string &json = r.second;
			std::vector<ChatMessage> fresh;
			size_t pos = 0;
			while ((pos = json.find("\"liveChatTextMessageRenderer\"", pos)) != std::string::npos) {
				size_t next = json.find("\"addChatItemAction\"", pos + 20);
				size_t end = next == std::string::npos ? json.size() : next;
				pos += 20;
				std::string id = string_after(json, pos, end, "\"id\":\"");
				if (id != "" && std::find(seen.begin(), seen.end(), id) != seen.end()) continue;
				if (id != "") {
					seen.push_back(id);
					if (seen.size() > 300) seen.erase(seen.begin(), seen.begin() + 100);
				}
				ChatMessage m;
				m.author = string_after(json, pos, end, "\"authorName\":{\"simpleText\":\"");
				size_t name_at = json.find("\"authorName\"", pos);
				size_t msg_at = json.find("\"message\":{\"runs\":[", pos);
				if (msg_at != std::string::npos && msg_at < end) {
					size_t limit = (name_at != std::string::npos && name_at > msg_at && name_at < end) ? name_at : end;
					size_t p = msg_at;
					while (true) {
						size_t after = 0;
						std::string part = string_after(json, p, limit, "\"text\":\"", &after);
						if (after == 0) break;
						m.text += part;
						p = after;
					}
				}
				if (m.text != "") fresh.push_back(m);
			}
			// Super Chats : the amount in front of the author
			pos = 0;
			while ((pos = json.find("\"liveChatPaidMessageRenderer\"", pos)) != std::string::npos) {
				size_t next = json.find("\"addChatItemAction\"", pos + 20);
				size_t end = next == std::string::npos ? json.size() : next;
				pos += 20;
				std::string id = string_after(json, pos, end, "\"id\":\"");
				if (id != "" && std::find(seen.begin(), seen.end(), id) != seen.end()) continue;
				if (id != "") seen.push_back(id);
				ChatMessage m;
				std::string amount = string_after(json, pos, end, "\"purchaseAmountText\":{\"simpleText\":\"");
				m.author = "[" + amount + "] " + string_after(json, pos, end, "\"authorName\":{\"simpleText\":\"");
				size_t msg_at = json.find("\"message\":{\"runs\":[", pos);
				if (msg_at != std::string::npos && msg_at < end) {
					size_t p = msg_at;
					size_t limit = json.find("\"authorName\"", msg_at);
					if (limit == std::string::npos || limit > end) limit = std::min(end, msg_at + 600);
					while (true) {
						size_t after = 0;
						std::string part = string_after(json, p, limit, "\"text\":\"", &after);
						if (after == 0) break;
						m.text += part;
						p = after;
					}
				}
				if (m.text == "") m.text = "(Super Chat)";
				fresh.push_back(m);
			}
			// where to continue and when
			size_t c = json.rfind("\"continuation\":\"");
			if (c != std::string::npos) {
				std::string next_token = string_after(json, c, json.size(), "\"continuation\":\"");
				if (next_token != "") continuation = next_token;
			}
			size_t t = json.rfind("\"timeoutMs\":");
			if (t != std::string::npos) {
				int ms = atoi(json.c_str() + t + 12);
				if (ms > 0) wait_ms = std::min(8000, std::max(2000, ms));
			}
			if (fresh.size()) {
				LightLock_Lock(&chat_lock);
				for (auto &m : fresh) messages.push_back(m);
				while (messages.size() > 80) messages.pop_front();
				chat_version++;
				LightLock_Unlock(&chat_lock);
				var_need_refresh = true;
			}
		}
		for (int waited = 0; waited < wait_ms && alive(); waited += 100) svcSleepThread(100000000LL);
	}
	chat_thread_running = false;
}

void live_chat_open(const std::string &video_id) {
	LightLock_Lock(&chat_lock);
	bool same = chat_video == video_id && !chat_stop;
	LightLock_Unlock(&chat_lock);
	if (same) return;
	live_chat_close();
	while (chat_thread_running) svcSleepThread(50000000LL); // the old thread is ending
	LightLock_Lock(&chat_lock);
	messages.clear();
	chat_status = tr("Loading the chat...", "Cargando el chat...");
	chat_video = video_id;
	chat_version++;
	LightLock_Unlock(&chat_lock);
	chat_stop = false;
	int generation = ++chat_generation;
	chat_thread_running = true;
	if (!threadCreate(chat_thread, (void *)(intptr_t)generation, 64 * 1024, 0x30, -1, true)) {
		chat_thread_running = false;
		chat_stop = true;
		set_status(tr("Could not start the chat", "No se pudo iniciar el chat"));
	}
}

void live_chat_close() {
	chat_stop = true;
	chat_generation++;
}

bool live_chat_is_open() { return !chat_stop; }

unsigned live_chat_version() {
	LightLock_Lock(&chat_lock);
	unsigned v = chat_version;
	LightLock_Unlock(&chat_lock);
	return v;
}

std::vector<ChatMessage> live_chat_latest(size_t count) {
	std::vector<ChatMessage> out;
	LightLock_Lock(&chat_lock);
	for (auto it = messages.rbegin(); it != messages.rend() && out.size() < count; ++it) out.push_back(*it);
	LightLock_Unlock(&chat_lock);
	return out;
}

std::string live_chat_status() {
	LightLock_Lock(&chat_lock);
	std::string s = chat_status;
	LightLock_Unlock(&chat_lock);
	return s;
}

bool live_chat_send(const std::string &video_id, const std::string &text, std::string &error) {
	return youtube_studio_live_chat_send(video_id, text, error);
}
