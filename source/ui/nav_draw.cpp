#include "ui/nav_draw.hpp"
#include "ui/style.hpp"
#include "ui/ui_common.hpp"
#include "variables.hpp"
#include <algorithm>

static void nav_icon(int type, float cx, float cy, u32 c) {
	const float w = 1.6f;
	switch (type) {
	case 0: // home : roof and body
		Draw_line(cx - 8, cy, c, cx, cy - 7, c, w);
		Draw_line(cx, cy - 7, c, cx + 8, cy, c, w);
		Draw_texture(var_square_image[0], c, cx - 5, cy, 10, 6);
		break;
	case 1: { // search : a circle (octagon) and a handle
		const float px[8] = {1, 0.7f, 0, -0.7f, -1, -0.7f, 0, 0.7f}, py[8] = {0, 0.7f, 1, 0.7f, 0, -0.7f, -1, -0.7f};
		for (int i = 0; i < 8; i++) {
			int j = (i + 1) % 8;
			Draw_line(cx - 1 + px[i] * 5, cy - 1 + py[i] * 5, c, cx - 1 + px[j] * 5, cy - 1 + py[j] * 5, c, w);
		}
		Draw_line(cx + 3, cy + 3, c, cx + 8, cy + 8, c, w + 0.6f);
		break;
	}
	case 2: // library : a list
		Draw_texture(var_square_image[0], c, cx - 7, cy - 6, 14, 2);
		Draw_texture(var_square_image[0], c, cx - 7, cy - 1, 14, 2);
		Draw_texture(var_square_image[0], c, cx - 7, cy + 4, 9, 2);
		break;
	case 3: // downloads : an arrow into a tray
		Draw_line(cx, cy - 7, c, cx, cy + 2, c, w);
		Draw_line(cx - 5, cy - 3, c, cx, cy + 3, c, w);
		Draw_line(cx + 5, cy - 3, c, cx, cy + 3, c, w);
		Draw_texture(var_square_image[0], c, cx - 7, cy + 6, 14, 2);
		break;
	default: // more : three dots
		Draw_texture(var_square_image[0], c, cx - 8, cy - 1, 4, 4);
		Draw_texture(var_square_image[0], c, cx - 2, cy - 1, 4, 4);
		Draw_texture(var_square_image[0], c, cx + 4, cy - 1, 4, 4);
		break;
	}
}

void nav_paint(const NavPaint &p, const std::string labels[NAV_ITEM_COUNT]) {
	const int top = p.top;
	if (p.offline) { // no internet : say it, saved downloads still work
		std::string text = tr("No connection - saved downloads still play", "Sin conexión - lo descargado se puede ver");
		float w = Draw_get_width(text, 0.4) + 14;
		Draw_round_rect((320 - w) / 2, top - 15, w, 12, 6, 0xF0303ACF);
		Draw_x_centered(text, (320 - w) / 2, (320 + w) / 2, top - 14.5f, 0.4, 0.4, 0xFFFFFFFF);
	}
	if (p.incognito) { // a reminder that nothing is being remembered
		std::string text = tr("Incognito", "Incógnito");
		float w = Draw_get_width(text, 0.4) + 12;
		Draw_round_rect(5, top - (p.offline ? 30 : 15), w, 12, 6, 0xF0303038);
		Draw(text, 11, top - (p.offline ? 30 : 15) + 0.5f, 0.4, 0.4, 0xFFFFFFFF);
	}
	// the bar : the card color with a thin line on top
	Draw_texture(var_square_image[0], UI_CARD_COLOR, 0, top, 320, p.height);
	Draw_texture(var_square_image[0], var_high_contrast ? (u32)DEFAULT_TEXT_COLOR : (u32)UI_OUTLINE_COLOR, 0, top, 320, 1);
	for (int i = 0; i < NAV_ITEM_COUNT; i++) {
		float x0 = i * 320.0f / NAV_ITEM_COUNT, x1 = (i + 1) * 320.0f / NAV_ITEM_COUNT, cx = (x0 + x1) / 2, cy = top + 10;
		bool sel = i == p.selected, pressed = i == p.pressed;
		if (sel) {
			Draw_round_rect(cx - 17, cy - 8, 34, 17, 8, COLOR_ACCENT);
		} else if (pressed) {
			Draw_round_rect(cx - 17, cy - 8, 34, 17, 8, UI_RAISED_COLOR);
		}
		u32 color = sel ? (u32)UI_ON_ACCENT : var_high_contrast ? (u32)DEFAULT_TEXT_COLOR : (u32)LIGHT0_TEXT_COLOR;
		nav_icon(i, cx, cy, color);
		if (i == 3 && p.download_running) { // a download is running : a green dot on Downloads
			Draw_round_rect(cx + 8, cy - 9, 7, 7, 3.5f, 0xFF50C878);
			Draw_round_rect(cx + 9, cy - 8, 5, 5, 2.5f, 0xFF50C878);
		}
		if (i == 2 && p.queue_size > 0) { // the number of videos in the queue, on Library
			std::string n = std::to_string(std::min(99, p.queue_size));
			float w = std::max(11.0f, Draw_get_width(n, 0.4) + 6);
			Draw_round_rect(cx + 7, cy - 10, w, 11, 5.5f, COLOR_ACCENT);
			Draw_x_centered(n, cx + 7, cx + 7 + w, cy - 10.5f, 0.4, 0.4, UI_ON_ACCENT);
		}
		float label_size = 0.4f; // (a long label, as in German, gets smaller instead of touching its neighbours)
		while (label_size > 0.3f && Draw_get_width(labels[i], label_size) > x1 - x0 - 4) label_size -= 0.02f;
		Draw_x_centered(labels[i], x0, x1, top + 19.5f + (0.4f - label_size) * 6, label_size, label_size, sel ? (u32)DEFAULT_TEXT_COLOR : (u32)LIGHT1_TEXT_COLOR);
	}
}
