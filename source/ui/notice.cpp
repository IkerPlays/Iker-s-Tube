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
	bool visible = osGetTime() < notice_until;
	notice_lock.unlock();
	if (!visible || text.empty()) {
		return;
	}
	std::string line = truncate_first_line(text, 300, 0.5);
	float h = 18;
	Draw_texture(var_square_image[0], 0xE0000000, 4, bottom_y - h - 2, 312, h);
	Draw_xy_centered(line, 4, 316, bottom_y - h - 2, bottom_y - 2, 0.5, 0.5, DEF_DRAW_WHITE);
	var_need_refresh = true; // until it disappears
}
