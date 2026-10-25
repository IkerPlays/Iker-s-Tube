#pragma once

// abgr8888 color
// COLOR_GRAY(0xFF) = WHITE, COLOR_GRAY(0x00) = BLACK
#define COLOR_GRAY(x) (0xFF000000 | (x) << 16 | (x) << 8 | (x))

#define DEF_DRAW_RED 0xFF0000FF
#define DEF_DRAW_GREEN 0xFF00FF00
#define DEF_DRAW_BLUE 0xFFFF0000
#define DEF_DRAW_BLACK 0xFF000000
#define DEF_DRAW_WHITE 0xFFFFFFFF
#define DEF_DRAW_AQUA 0xFFFFFF00
#define DEF_DRAW_YELLOW 0xFF00C5FF
#define DEF_DRAW_LIGHT_GRAY 0xFFAAAAAA
#define DEF_DRAW_GRAY 0xFF777777
#define DEF_DRAW_DARK_GRAY 0xFF333333
#define DEF_DRAW_WEAK_RED 0x500000FF
#define DEF_DRAW_WEAK_ORANGE 0x500078FF
#define DEF_DRAW_WEAK_GREEN 0x5000FF00
#define DEF_DRAW_WEAK_BLUE 0x50FF0000
#define DEF_DRAW_WEAK_BLACK 0x50000000
#define DEF_DRAW_WEAK_WHITE 0x50FFFFFF
#define DEF_DRAW_WEAK_AQUA 0x50FFFF00
#define DEF_DRAW_WEAK_YELLOW 0x5000C5FF
#define DEF_DRAW_NO_COLOR 0x0

#define COLOR_LIGHT_BLUE 0xFFEEAAAA
#define COLOR_LIGHT_GREEN 0xFFAAEEAA

#include <math.h>
extern int var_theme;
extern int var_custom_hue;
// Color themes: accent, then the dark surfaces : page, cards / rows, raised (buttons) and tint (selected) (the last theme is built from var_custom_hue)
#define RGB_(r, g, b) (0xFF000000u | ((unsigned)(b) << 16) | ((unsigned)(g) << 8) | (unsigned)(r))
#define THEME_COUNT 7
static const unsigned int theme_accent_table[THEME_COUNT] = {RGB_(0xFF, 0x4B, 0x4B), RGB_(0x3C, 0x9A, 0xFF), RGB_(0x50, 0xC8, 0x50), RGB_(0xE0, 0x50, 0x9B), RGB_(0xFF, 0xA0, 0x30), RGB_(0xFF, 0x4B, 0x4B), 0};
static const unsigned int theme_back_table[THEME_COUNT][4] = {
    {RGB_(0x0F, 0x0E, 0x12), RGB_(0x18, 0x17, 0x20), RGB_(0x24, 0x22, 0x2D), RGB_(0x5A, 0x24, 0x34)}, // Ruby
    {RGB_(0x0C, 0x11, 0x18), RGB_(0x15, 0x1C, 0x27), RGB_(0x21, 0x2A, 0x39), RGB_(0x1F, 0x4A, 0x7A)}, // Ocean
    {RGB_(0x0C, 0x12, 0x0E), RGB_(0x14, 0x1C, 0x17), RGB_(0x20, 0x2B, 0x24), RGB_(0x24, 0x5E, 0x35)}, // Forest
    {RGB_(0x12, 0x0E, 0x16), RGB_(0x1B, 0x16, 0x24), RGB_(0x28, 0x21, 0x34), RGB_(0x6A, 0x2C, 0x62)}, // Violet
    {RGB_(0x12, 0x10, 0x0B), RGB_(0x1C, 0x19, 0x13), RGB_(0x2A, 0x26, 0x1D), RGB_(0x7A, 0x5A, 0x1C)}, // Amber
    {RGB_(0x00, 0x00, 0x00), RGB_(0x0D, 0x0D, 0x10), RGB_(0x1A, 0x1A, 0x1F), RGB_(0x5A, 0x24, 0x34)}, // AMOLED
    {0, 0, 0, 0},                                                                                      // Custom (computed)
};
// HSV (h in degrees, s and v in 0..1) to an opaque ABGR color
static inline unsigned int theme_hsv(int h, float s, float v) {
	h = ((h % 360) + 360) % 360;
	float hp = h / 60.0f;
	float c = v * s, x = c * (1 - fabsf(fmodf(hp, 2.0f) - 1)), m = v - c;
	float r = 0, g = 0, b = 0;
	switch (h / 60) {
	case 0: r = c, g = x; break;
	case 1: r = x, g = c; break;
	case 2: g = c, b = x; break;
	case 3: g = x, b = c; break;
	case 4: r = x, b = c; break;
	default: r = c, b = x; break;
	}
	return 0xFF000000 | ((unsigned int)((b + m) * 255) << 16) | ((unsigned int)((g + m) * 255) << 8) | (unsigned int)((r + m) * 255);
}
static inline unsigned int theme_accent() {
	return var_theme == 6 ? theme_hsv(var_custom_hue, 0.72f, 1.0f) : theme_accent_table[var_theme];
}
static inline unsigned int theme_back(int level) {
	if (var_theme == 6) {
		static const float values[4] = {0.07f, 0.12f, 0.19f, 0.36f};
		return theme_hsv(var_custom_hue, level == 3 ? 0.70f : 0.28f, values[level]);
	}
	return theme_back_table[var_theme][level];
}
#define COLOR_LINK (theme_accent())

#define DEFAULT_TEXT_COLOR (var_night_mode ? RGB_(0xF4, 0xF6, 0xFA) : RGB_(0x14, 0x16, 0x1B))
#define LIGHT0_TEXT_COLOR (var_high_contrast ? DEFAULT_TEXT_COLOR : (var_night_mode ? RGB_(0xC9, 0xCE, 0xDA) : RGB_(0x3C, 0x42, 0x52)))
#define LIGHT1_TEXT_COLOR (var_high_contrast ? DEFAULT_TEXT_COLOR : (var_night_mode ? RGB_(0x93, 0x9A, 0xAC) : RGB_(0x69, 0x70, 0x84)))
// Iker's Tube palette : slightly blue dark theme, red accent (colors are ABGR)
#define COLOR_ACCENT (theme_accent())
#define DEFAULT_BACK_COLOR (var_night_mode ? (var_high_contrast ? 0xFF000000u : theme_back(0)) : (var_high_contrast ? 0xFFFFFFFFu : RGB_(0xF2, 0xF3, 0xF6)))
#define LIGHT0_BACK_COLOR (var_night_mode ? theme_back(1) : RGB_(0xFF, 0xFF, 0xFF))
#define LIGHT1_BACK_COLOR (var_night_mode ? theme_back(2) : RGB_(0xE6, 0xE8, 0xEE))
#define LIGHT2_BACK_COLOR (var_night_mode ? theme_back(3) : RGB_(0xD3, 0xD8, 0xE2))
#define LIGHT3_BACK_COLOR COLOR_ACCENT

// Surfaces of the interface : the page, the cards / rows on it, raised things (buttons) and the line between them
#define UI_PAGE_COLOR DEFAULT_BACK_COLOR
#define UI_CARD_COLOR LIGHT0_BACK_COLOR
#define UI_RAISED_COLOR LIGHT1_BACK_COLOR
#define UI_OUTLINE_COLOR (var_night_mode ? RGB_(0x2E, 0x33, 0x40) : RGB_(0xD9, 0xDC, 0xE3))
// a color in between (t from 0 to 1), channel by channel
static inline unsigned int ui_mix(unsigned int a, unsigned int b, float t) {
	unsigned int out = 0xFF000000u;
	t = t < 0 ? 0 : t > 1 ? 1 : t; // (a mix never goes past either color : the channels would wrap)
	for (int shift = 0; shift <= 16; shift += 8) {
		float ca = (a >> shift) & 0xFF, cb = (b >> shift) & 0xFF;
		out |= ((unsigned int)(ca + (cb - ca) * t) & 0xFF) << shift;
	}
	return out;
}

// the relative luminance of a color (WCAG), 0 black .. 1 white
static inline float ui_luminance(unsigned int abgr) {
	float c[3];
	for (int i = 0; i < 3; i++) {
		float v = ((abgr >> (8 * i)) & 0xFF) / 255.0f;
		c[i] = v <= 0.03928f ? v / 12.92f : powf((v + 0.055f) / 1.055f, 2.4f);
	}
	return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
}
static inline float ui_contrast(unsigned int a, unsigned int b) {
	float la = ui_luminance(a), lb = ui_luminance(b);
	return (std::max(la, lb) + 0.05f) / (std::min(la, lb) + 0.05f);
}

// The text color that reads best on top of the accent color : white, or near black when the accent is a bright one
static inline unsigned int ui_text_on(unsigned int abgr) {
	float luminance = ui_luminance(abgr);
	return luminance > 0.30f ? RGB_(0x14, 0x16, 0x1B) : 0xFFFFFFFFu;
}
#define UI_ON_ACCENT (ui_text_on(theme_accent()))
// The accent color used for TEXT : the accent is darkened (on a light page) or lightened (on a dark one) until it reads well on the
// surface (contrast 4.5 : 1). The answer is remembered while the accent and the mode do not change.
static inline unsigned int ui_accent_text(unsigned int accent, bool night, unsigned int surface) {
	static unsigned int last_accent = 0, last_surface = 0, last_result = 0;
	static bool last_night = false, have = false;
	if (have && last_accent == accent && last_night == night && last_surface == surface) return last_result;
	unsigned int result = accent;
	for (int step = 0; step < 12 && ui_contrast(result, surface) < 4.5f; step++) {
		result = ui_mix(accent, night ? 0xFFFFFFFFu : 0xFF000000u, 0.08f * (step + 1));
	}
	last_accent = accent, last_surface = surface, last_night = night, last_result = result, have = true;
	return result;
}
#define UI_ACCENT_TEXT (ui_accent_text(theme_accent(), var_night_mode, UI_CARD_COLOR))
