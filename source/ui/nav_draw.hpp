#pragma once
#include <string>

// What the bottom navigation bar needs to know to draw itself (the drawing is kept apart from the touch handling of overlay.cpp)
struct NavPaint {
	int top = 210;
	int height = 30;
	int selected = -1; // the item of the current scene (-1 : none)
	int pressed = -1;  // the item under the finger (-1 : none)
	bool incognito = false;
	bool offline = false;
	bool download_running = false;
	int queue_size = 0;
};
#define NAV_ITEM_COUNT 5
void nav_paint(const NavPaint &state, const std::string labels[NAV_ITEM_COUNT]);
