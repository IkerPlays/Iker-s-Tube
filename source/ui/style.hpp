#pragma once
// Iker UI : the shapes the whole interface is made of. Everything is drawn with rectangles, so it works on both screens.
#include <math.h>
#include <string>
#include <algorithm>
#include "ui/draw/draw.hpp"
#include "ui/colors.hpp"
#include "variables.hpp"

#define UI_RADIUS_CARD 8   // rows, cards, dialogs
#define UI_RADIUS_BUTTON 7 // buttons, tabs, selectors
#define UI_RADIUS_CHIP 3   // little labels on top of pictures

// Paints the four corners of the box (x, y, w, h) with `color` where a rounded corner of radius r would leave them empty.
// Used on pictures, which cannot be drawn rounded : the corners are covered with the color of what is behind them.
// corners : bit 0 top left, 1 top right, 2 bottom left, 3 bottom right.
static inline void ui_mask_corners(float x, float y, float w, float h, int r, u32 color, int corners = 15) {
	if (w <= 0 || h <= 0) return;
	r = std::min(r, (int)(std::min(w, h) / 2)); // (a corner can not be bigger than half the picture)
	if (r <= 0) return;
	// rows that need the same width are drawn as one taller rectangle (a round picture would otherwise need ~90 draw calls)
	int run_start = 0, run_width = -1;
	for (int k = 0; k <= r; k++) {
		int width = 0;
		if (k < r) {
			float dy = r - k - 0.5f;
			float inset = r - sqrtf((float)r * r - dy * dy);
			width = inset < 0.5f ? 0 : (int)ceilf(inset);
		}
		if (k == 0) { run_width = width; continue; }
		if (width != run_width) {
			if (run_width > 0) {
				int rows = k - run_start;
				if (corners & 1) Draw_texture(var_square_image[0], color, x, y + run_start, run_width, rows);
				if (corners & 2) Draw_texture(var_square_image[0], color, x + w - run_width, y + run_start, run_width, rows);
				if (corners & 4) Draw_texture(var_square_image[0], color, x, y + h - k, run_width, rows);
				if (corners & 8) Draw_texture(var_square_image[0], color, x + w - run_width, y + h - k, run_width, rows);
			}
			run_start = k;
			run_width = width;
		}
	}
}

// A little rounded label (duration, DL, NEW...) : the text is `size` big, `pad` pixels from the border.
static inline float ui_chip_width(const std::string &text, float size = 0.4f, float pad = 3) { return Draw_get_width(text, size) + pad * 2; }
static inline void ui_chip(const std::string &text, float x, float y, u32 background, u32 text_color, float size = 0.4f, float pad = 3, float height = 11) {
	float w = ui_chip_width(text, size, pad);
	Draw_round_rect(x, y, w, height, UI_RADIUS_CHIP, background);
	Draw(text, x + pad, y + (height - 20.0f * size) / 2 - 1, size, size, text_color);
}

// A soft shadow under a rounded box (three faint layers).
static inline void ui_shadow(float x, float y, float w, float h, float r) {
	Draw_round_rect(x - 1, y + 2, w + 2, h + 2, r + 1, 0x14000000);
	Draw_round_rect(x - 2, y + 3, w + 4, h + 3, r + 2, 0x0C000000);
}

// A check mark of 7 x 6 pixels (pixel by pixel : no font is needed).
static inline void ui_check(float x, float y, u32 color) {
	static const int steps[7][2] = {{0, 3}, {1, 4}, {2, 5}, {3, 4}, {4, 3}, {5, 2}, {6, 1}};
	for (auto &s : steps) Draw_texture(var_square_image[0], color, x + s[0], y + s[1] - 1, 1, 2);
}

// A vertical fade from color c1 (top) to color c2 (bottom), drawn as thin strips.
static inline void ui_gradient_v(float x, float y, float w, float h, u32 c1, u32 c2, int strip = 4) {
	int steps = std::max(1, (int)(h / strip));
	for (int i = 0; i < steps; i++) {
		float y0 = y + h * i / steps, y1 = y + h * (i + 1) / steps;
		Draw_texture(var_square_image[0], ui_mix(c1, c2, (float)i / std::max(1, steps - 1)), x, y0, w, y1 - y0 + 0.5f);
	}
}
