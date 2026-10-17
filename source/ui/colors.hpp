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
// Color themes: accent, dark background, dark light0, light1, light2 (the last theme is built from var_custom_hue)
#define THEME_COUNT 7
static const unsigned int theme_accent_table[THEME_COUNT] = {0xFF4B4BFF, 0xFFFF9A3C, 0xFF50C850, 0xFFE0509B, 0xFF30A0FF, 0xFF4B4BFF, 0};
static const unsigned int theme_back_table[THEME_COUNT][4] = {
    {0xFF181212, 0xFF281E1E, 0xFF403030, 0xFF2A2058}, // Ruby
    {0xFF2A1810, 0xFF3A2418, 0xFF553A2A, 0xFF603A1E}, // Ocean
    {0xFF121A12, 0xFF1E281E, 0xFF304030, 0xFF1E5A2A}, // Forest
    {0xFF1E1218, 0xFF2E1E28, 0xFF483050, 0xFF58204A}, // Violet
    {0xFF10161C, 0xFF1C2630, 0xFF2E3C4A, 0xFF1E4A66}, // Amber
    {0xFF000000, 0xFF101010, 0xFF282828, 0xFF2A2058}, // AMOLED
    {0, 0, 0, 0},                                      // Custom (computed)
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
		static const float values[4] = {0.08f, 0.14f, 0.22f, 0.32f};
		return theme_hsv(var_custom_hue, level == 3 ? 0.75f : 0.35f, values[level]);
	}
	return theme_back_table[var_theme][level];
}
#define COLOR_LINK (theme_accent())

#define DEFAULT_TEXT_COLOR (var_night_mode ? COLOR_GRAY(0xFF) : COLOR_GRAY(0x00))
#define LIGHT0_TEXT_COLOR (var_high_contrast ? DEFAULT_TEXT_COLOR : (var_night_mode ? COLOR_GRAY(0xCC) : COLOR_GRAY(0x33)))
#define LIGHT1_TEXT_COLOR (var_high_contrast ? DEFAULT_TEXT_COLOR : (var_night_mode ? COLOR_GRAY(0xA0) : COLOR_GRAY(0x77)))
// Iker's Tube palette : slightly blue dark theme, red accent (colors are ABGR)
#define COLOR_ACCENT (theme_accent())
#define DEFAULT_BACK_COLOR (var_night_mode ? (var_high_contrast ? 0xFF000000u : theme_back(0)) : COLOR_GRAY(0xFF))
#define LIGHT0_BACK_COLOR (var_night_mode ? theme_back(1) : COLOR_GRAY(0xEE))
#define LIGHT1_BACK_COLOR (var_night_mode ? theme_back(2) : COLOR_GRAY(0xD0))
#define LIGHT2_BACK_COLOR (var_night_mode ? theme_back(3) : COLOR_GRAY(0xB8))
#define LIGHT3_BACK_COLOR COLOR_ACCENT
