#include "headers.hpp"
#include "ui/notice.hpp"
#include "ui/colors.hpp"

static Mutex notice_lock;
static std::string notice_text;
static u64 notice_until = 0;

void notice_show(const std::string &text) {
	notice_lock.lock();
	notice_text = text;
	notice_until = osGetTime() + 3500;
	notice_lock.unlock();
	var_need_refresh = true;
}

void notice_draw(float bottom_y) {
	notice_lock.lock();
	std::string text = notice_text;
	u64 until = notice_until;
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
	double shown = 3500.0 - left;
	double alpha = left < 400 ? left / 400.0 : 1.0;      // fades out at the end
	double slide = shown < 150 ? 1.0 - shown / 150.0 : 0; // slides up when it appears
	float y = bottom_y - h - 3 + (float)(slide * 8);
	u32 a = (u32)(0xE0 * alpha);
	Draw_round_rect((int)x, (int)y, (int)w, (int)h, 9, (a << 24) | 0x000000);
	Draw_xy_centered(line, x, x + w, y, y + h, 0.5, 0.5, (u32)(0xFF * alpha) << 24 | 0xFFFFFF);
	var_need_refresh = true; // until it disappears
}
