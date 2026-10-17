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

// Color themes: accent, dark background, dark light0, light1, light2
#define THEME_COUNT 6
static const unsigned int theme_accent_table[THEME_COUNT] = {0xFF4B4BFF, 0xFFFF9A3C, 0xFF50C850, 0xFFE0509B, 0xFF30A0FF, 0xFF4B4BFF};
static const unsigned int theme_back_table[THEME_COUNT][4] = {
    {0xFF181212, 0xFF281E1E, 0xFF403030, 0xFF2A2058}, // Ruby
    {0xFF2A1810, 0xFF3A2418, 0xFF553A2A, 0xFF603A1E}, // Ocean
    {0xFF121A12, 0xFF1E281E, 0xFF304030, 0xFF1E5A2A}, // Forest
    {0xFF1E1218, 0xFF2E1E28, 0xFF483050, 0xFF58204A}, // Violet
    {0xFF10161C, 0xFF1C2630, 0xFF2E3C4A, 0xFF1E4A66}, // Amber
    {0xFF000000, 0xFF101010, 0xFF282828, 0xFF2A2058}, // AMOLED
};
#define COLOR_LINK (theme_accent_table[var_theme])

#define DEFAULT_TEXT_COLOR (var_night_mode ? COLOR_GRAY(0xFF) : COLOR_GRAY(0x00))
#define LIGHT0_TEXT_COLOR (var_night_mode ? COLOR_GRAY(0xCC) : COLOR_GRAY(0x33))
#define LIGHT1_TEXT_COLOR (var_night_mode ? COLOR_GRAY(0xA0) : COLOR_GRAY(0x77))
// Iker's Tube palette : slightly blue dark theme, red accent (colors are ABGR)
#define COLOR_ACCENT (theme_accent_table[var_theme])
#define DEFAULT_BACK_COLOR (var_night_mode ? theme_back_table[var_theme][0] : COLOR_GRAY(0xFF))
#define LIGHT0_BACK_COLOR (var_night_mode ? theme_back_table[var_theme][1] : COLOR_GRAY(0xEE))
#define LIGHT1_BACK_COLOR (var_night_mode ? theme_back_table[var_theme][2] : COLOR_GRAY(0xD0))
#define LIGHT2_BACK_COLOR (var_night_mode ? theme_back_table[var_theme][3] : COLOR_GRAY(0xB8))
#define LIGHT3_BACK_COLOR COLOR_ACCENT
