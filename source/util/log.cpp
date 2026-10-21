#include "headers.hpp"

#include <sys/stat.h>
#define LOG_DIR "sdmc:/3ds/IkersTube"
#define LOG_PATH "sdmc:/3ds/IkersTube/log.txt"
#define LOG_PREV_PATH "sdmc:/3ds/IkersTube/log_previous.txt"

void Logger::init() { osTickCounterStart(&stopwatch); }

// warnings and errors are also appended to the SD card so they can be shared when something fails
static void append_to_log_file(const std::string &line) {
	static bool first = true;
	if (first) { // new session : keep the previous log as log_previous.txt
		first = false;
		mkdir("sdmc:/3ds", 0777);
		mkdir(LOG_DIR, 0777);
		remove(LOG_PREV_PATH);
		rename(LOG_PATH, LOG_PREV_PATH);
	}
	static size_t written = 0;
	if (written > 512 * 1024) { // a warning loop must not fill the SD card: the file is rotated
		remove(LOG_PREV_PATH);
		rename(LOG_PATH, LOG_PREV_PATH);
		written = 0;
	}
	FILE *f = fopen(LOG_PATH, "a");
	if (f) {
		written += fprintf(f, "%s\n", line.c_str()) > 0 ? line.size() + 1 : 0;
		fclose(f);
	}
}

// writes everything buffered (all levels) to sdmc:/3ds/IkersTube/log_full.txt
bool Logger::save_full_log() {
	mkdir("sdmc:/3ds", 0777);
	mkdir(LOG_DIR, 0777);
	FILE *f = fopen(LOG_DIR "/log_full.txt", "w");
	if (!f) {
		return false;
	}
	content_lock.lock();
	for (auto &entry : logs) {
		const char *level = entry.level == LogLevel::ERROR ? "ERROR" : entry.level == LogLevel::WARNING ? "WARN " : entry.level == LogLevel::CAUTION ? "CAUT " : "INFO ";
		fprintf(f, "%s %s\n", level, entry.str.c_str());
	}
	content_lock.unlock();
	fclose(f);
	return true;
}
void Logger::log(LogLevel level, const std::string &str) {
	content_lock.lock();
	// get time
	osTickCounterUpdate(&stopwatch);
	acc_time += osTickCounterRead(&stopwatch) / 1000;

	char time_str[32] = {0};
	snprintf(time_str, 32, "[%.5f]", acc_time);

	if (draw_offset_y + DRAW_LINES <= (int)logs.size()) {
		draw_offset_y++;
	}
	logs.push_back({acc_time, level, std::string(time_str) + " " + str.substr(0, 120)});
	if (logs.size() > MAX_BUFFERED_LINES) {
		logs.pop_front(), draw_offset_y = std::max(0, draw_offset_y - 1);
	}
	if (draw_enabled) {
		var_need_refresh = true;
	}
	if (level == LogLevel::WARNING || level == LogLevel::ERROR) {
	// the same line again and again (a thumbnail that keeps failing) is written to the SD card only now and then
	static std::string last_line;
	static int repeats = 0;
	if (str == last_line) {
		repeats++;
		if (repeats % 50 == 0) append_to_log_file(logs.back().str + " (x" + std::to_string(repeats) + ")");
	} else {
		last_line = str;
		repeats = 0;
		append_to_log_file(logs.back().str);
	}
	}
	content_lock.unlock();
}
void Logger::update(Hid_info key) {
	if (draw_enabled) { // move only if drawing is enabled
		content_lock.lock();
		float draw_offset_x_old = draw_offset_x;
		int draw_offset_y_old = draw_offset_y;
		if (key.h_c_up) {
			draw_offset_y = std::max(0, draw_offset_y - 1);
		}
		if (key.h_c_down) {
			draw_offset_y = std::min((int)logs.size() - 1, draw_offset_y + 1);
		}
		if (key.h_c_left) {
			draw_offset_x = std::max(0.0f, draw_offset_x - XSCROLL_SPEED);
		}
		if (key.h_c_right) {
			draw_offset_x = std::min(XSCROLL_MAX, draw_offset_x + XSCROLL_SPEED);
		}
		if (draw_offset_x != draw_offset_x_old || draw_offset_y != draw_offset_y_old) {
			var_need_refresh = true;
		}
		content_lock.unlock();
	}
}

void Logger::draw() {
	if (draw_enabled) {
		content_lock.lock();
		for (int i = 0; i < DRAW_LINES && draw_offset_y + i < (int)logs.size(); i++) {
			auto &cur_log = logs[draw_offset_y + i];
			Draw(cur_log.str, -draw_offset_x, (1 + i) * draw_y_interval, font_size, font_size,
			     get_log_color(cur_log.level));
		}
		content_lock.unlock();
	}
}
size_t Logger::get_memory_consumption() {
	content_lock.lock();
	size_t res = 0;
	res += sizeof(Logger);
	res += sizeof(logs[0]) * logs.size();
	for (auto &i : logs) {
		res += i.str.size();
	}
	content_lock.unlock();
	return res;
}
Logger logger; // global logger
