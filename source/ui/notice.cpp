#include "headers.hpp"
#include "ui/notice.hpp"
#include "ui/colors.hpp"

static Mutex notice_lock;
static std::string notice_text;
static u64 notice_until = 0;
static u64 notice_total = 3500; // long texts stay longer
static u64 notice_start = 0;
static std::string notice_next; // a second notice that arrived while the first was still new : it waits for its turn
static bool notice_has_next = false;

static void notice_start_locked(const std::string &text) {
	notice_text = text;
	notice_total = std::min<u64>(7000, 3500 + 40 * (u64)text.size());
	notice_start = osGetTime();
	notice_until = notice_start + notice_total;
}

void notice_show(const std::string &text) {
	notice_lock.lock();
	u64 now = osGetTime();
	if (now < notice_until && !notice_text.empty() && text != notice_text && now - notice_start < 1200) {
		notice_next = text; // (every notice is readable for at least a second : a badge or a battery warning is not wiped at once)
		notice_has_next = true;
	} else {
		notice_has_next = false;
		notice_start_locked(text);
	}
	notice_lock.unlock();
	var_need_refresh = true;
}

void notice_draw(float bottom_y) {
	notice_lock.lock();
	if (notice_has_next && osGetTime() >= notice_start + 1200) { // the waiting notice takes over once the first one has had its second
		notice_has_next = false;
		notice_start_locked(notice_next);
	}
	std::string text = notice_text;
	u64 until = notice_until;
	u64 total = notice_total;
	notice_lock.unlock();
	u64 now = osGetTime();
	if (now >= until || text.empty()) {
		return;
	}
	std::string line = truncate_first_line(text, 290, 0.5);
	float h = 20;
	float w = std::min(312.0f, Draw_get_width(line, 0.5) + 28);
	float x = (320 - w) / 2;
	double left = (double)(until - now);
	double shown = (double)total - left;
	double alpha = left < 400 ? left / 400.0 : 1.0;      // fades out at the end
	double slide = shown < 150 ? 1.0 - shown / 150.0 : 0; // slides up when it appears
	float y = bottom_y - h - 3 + (float)(slide * 8);
	u32 a = (u32)(0xE0 * alpha);
	Draw_round_rect((int)x, (int)y, (int)w, (int)h, 9, (a << 24) | 0x000000);
	Draw_xy_centered(line, x, x + w, y, y + h, 0.5, 0.5, (u32)(0xFF * alpha) << 24 | 0xFFFFFF);
	var_need_refresh = true; // until it disappears
}
