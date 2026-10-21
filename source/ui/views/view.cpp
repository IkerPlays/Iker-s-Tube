#include <functional>
#include "ui/views/view.hpp"
#include "ui/colors.hpp"
#include "variables.hpp"

// The background of rows and buttons : the card color, tinted with the accent while it is pressed.
const std::function<u32(const View &)> View::STANDARD_BACKGROUND = [](const View &view) {
	if (view.touch_darkness > 0 && view.touch_darkness < 1) {
		var_need_refresh = true;
	}
	return ui_mix(UI_CARD_COLOR, COLOR_ACCENT, 0.28f * (float)view.touch_darkness);
};
