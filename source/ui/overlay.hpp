#pragma once
#include "types.hpp"
#include "scene_switcher.hpp"

#define OVERLAY_MENU_ICON_SIZE 29

#define NAV_BAR_HEIGHT 30

// Bottom navigation bar (Home, Search, Library, Downloads, More). A scene that wants it calls this every frame before
// update_overlay_menu() / draw_overlay_menu() with the y of the bottom of its area (240, or above the playing bar), and
// then uses (bottom - NAV_BAR_HEIGHT) as the bottom of its own content. While it is on, the menu button is the "More" item.
void overlay_set_nav(bool enabled, int bottom);
void draw_overlay_menu(int y);
void update_overlay_menu(Hid_info *key);
void close_overlay_menu();
void overlay_menu_on_resume();
