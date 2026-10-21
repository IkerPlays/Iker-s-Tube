#include <functional>
#include "ui/views/view.hpp"
#include "ui/colors.hpp"
#include "variables.hpp"

// The background of rows and buttons : a row is a card, a button (a small view) is a little lighter, and both are tinted with
// the accent while they are pressed.
const std::function<u32(const View &)> View::STANDARD_BACKGROUND = [](const View &view) {
	if (view.touch_darkness > 0 && view.touch_darkness < 1) {
		var_need_refresh = true;
	}
	float w = view.get_width(), h = view.get_height();
	bool button = w >= 40 && w <= 280 && h <= 40;
	return ui_mix(button ? (u32)UI_RAISED_COLOR : (u32)UI_CARD_COLOR, COLOR_ACCENT, 0.28f * (float)view.touch_darkness);
};
