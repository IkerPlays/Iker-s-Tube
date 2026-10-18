#pragma once
#include <functional>
#include <string>
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
// A menu of actions for one video (play next, queue, watch later, favorites, save offline, not interested), opened by a long
// press on a video of any list. It is drawn and handled by the overlay, so every scene that has the overlay gets it.
// The same for a channel : subscribe / unsubscribe (also on the account when signed in) and the notification bell.
void overlay_show_channel_menu(const std::string &id, const std::string &name, const std::string &icon_url, const std::string &subscribers);
void overlay_show_video_menu(const std::string &url, const std::string &title, const std::string &author,
                             const std::string &extra_label = "", std::function<void()> extra_action = nullptr); // (an optional extra option, for example Remove from history)
void overlay_menu_on_resume();
