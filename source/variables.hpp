#pragma once

extern bool var_connect_test_success;
extern bool var_need_refresh;
extern bool var_update_available;
extern const char *volatile var_update_banner; // status of an automatic/manual update shown in the menu (NULL : none)
extern int var_show_devbuilds; // 1 : also offer devbuilds (a build that is itself a devbuild always sees them)
extern u64 var_sleep_timer_end; // osGetTime() at which the playback pauses (0 : no timer)
extern int var_audio_screens_off; // 1 : in audio-only playback the screens turn off after a few seconds
extern u64 var_data_session_bytes; // downloaded since the app started
extern double var_data_total_bytes; // downloaded before this session (saved in the settings)
extern double var_av_sync_ms; // > 0 : show the video earlier (compensates the display latency), < 0 : later // set by the background updater when a newer release exists
extern bool var_allow_send_app_info;
extern bool var_hide_pointer;
extern bool var_night_mode;
extern int var_caption_size;  // 0 small, 1 normal, 2 large
extern int var_caption_color; // 0 white, 1 yellow, 2 cyan, 3 green
extern bool var_auto_quality; // lower the quality for the next videos when playback keeps stalling
extern int var_auto_quality_cap; // current cap in p (0 = none)
extern bool var_auto_dim; // dim the bottom screen while a video plays and the console is not touched
extern int var_text_scale; // 0 small, 1 normal, 2 large (global text size)
extern int var_start_scene; // 0 home, 1 library, 2 search : screen opened at start
extern bool var_auto_theme; // dark theme at night, light during the day
extern bool var_grid_view; // video lists as a grid of thumbnails (search results)
extern bool var_data_saver; // limits the quality to 240p and does not download thumbnails
extern int var_data_limit_index; // 0 off, then 100 MB, 250 MB, 500 MB, 1 GB, 2 GB (session)
extern double var_total_play_seconds; // statistics : time spent playing
extern int var_button_x; // action of the X button in the player (0 none, 1 +30 s, 2 next in queue, 3 restart, 4 mini player, 5 bookmark, 6 stop after this video)
extern int var_button_l; // same for L (default restart)
extern int var_button_r; // same for R (default next in queue)
extern int var_button_y; // same for Y
extern bool var_left_handed; // mirrors the mini player controls
extern int var_custom_hue; // 0..359 : accent color of the "Custom" theme
extern bool var_high_contrast; // pure black/white texts and backgrounds
extern bool var_welcome_done; // false on the very first launch : the settings open once so language, theme and quality can be chosen
extern int var_default_speed; // 0..5 : 100%, 75%, 125%, 150%, 175%, 200% (for channels without a speed of their own)
extern int var_wl_autoremove; // 1 : a video leaves Watch later when it is watched until the end
extern int var_hide_studio; // 1 : the Studio tab of the Home screen is not shown (takes effect when the app starts)
extern int var_download_window; // 0 : downloads run any time, 1 : only at night (1:00 to 7:00 console time)
extern int var_autoplay_delay; // 0..3 : 0 s, 3 s, 5 s, 10 s before the next video starts by itself
extern bool var_tour_done;    // the first-use tour was shown
extern bool var_incognito;    // nothing is remembered : no watch history, searches, resume points or last played video
extern int var_sponsorblock;  // 0 off, 1 skip sponsors, 2 skip sponsors, self promotion, intros and outros (SponsorBlock)
extern bool var_hide_shorts;  // leave the Shorts out of the search results and the home feed
extern bool var_hide_watched; // leave the videos that are in the watch history out of them
extern bool var_offline_autodelete; // delete a saved video / audio after it was watched until the end
extern bool var_mark_watched; // check mark on the thumbnails of videos that are in the history
extern int var_new_feed_videos; // subscription videos not seen yet (red dot on the menu icon)
extern bool var_resume_videos; // continue a video where it was left the last time
extern int var_theme;             // color theme index (see colors.hpp)
extern bool var_large_thumbnails; // bigger thumbnails in video lists
extern bool var_disable_pull_to_refresh;
extern bool var_eco_mode;
extern bool var_debug_mode;
extern bool var_history_enabled;
extern bool var_oauth_enabled;
extern int var_autoplay_level;
extern int var_loop_mode;
extern int var_3d_mode;
extern int var_video_quality;
extern bool var_show_fps;
extern bool var_full_dislike_like_count;
extern bool var_full_screen_mode;
extern bool var_video_show_debug_info;
extern int var_player_response;
extern bool var_video_linear_filter;
extern double var_forward_buffer_ratio;
extern u8 var_wifi_state;
extern u8 var_wifi_signal;
extern u8 var_battery_charge;
extern int var_hours;
extern int var_minutes;
extern int var_seconds;
extern int var_days;
extern int var_months;
extern int var_years;
extern int var_battery_level_raw;
extern float var_afk_time;

#define DPAD_SCROLL_SPEED_MIN 1.0
#define DPAD_SCROLL_SPEED0_DEFAULT 6.0
#define DPAD_SCROLL_SPEED1_DEFAULT 9.0
#define DPAD_SCROLL_SPEED_MAX 15.0
extern double var_dpad_scroll_speed0;
extern double var_dpad_scroll_speed1;
#define DPAD_SCROLL_THRESHOLD_MIN 0.3
#define DPAD_SCROLL_THRESHOLD_DEFAULT 2.0
#define DPAD_SCROLL_THRESHOLD_MAX 5.0
extern double var_dpad_scroll_speed1_threshold; // in seconds
extern double var_dpad_scroll_speed1_threshold_legacy;
extern int var_free_ram;
extern int var_free_linear_ram;
extern int var_lcd_brightness;
extern int var_time_to_turn_off_lcd;
extern int var_screen_off_mode;
extern int var_num_of_app_start;
extern int var_system_region;
extern bool var_is_new3ds;
extern bool var_core2_available;
extern bool var_core3_available;
extern bool var_app_suspended; // i.e. home button preessed

#define COMMUNITY_IMAGE_SIZE_MIN 50
#define COMMUNITY_IMAGE_SIZE_DEFAULT 200
#define COMMUNITY_IMAGE_SIZE_MAX 280
extern int var_community_image_size;

extern double var_scroll_speed;
extern double var_battery_voltage;
extern char var_status[128];
extern std::string var_clipboard;
extern std::string var_connected_ssid;
extern std::string var_lang;
extern std::string var_lang_content;
extern u8 var_model;
extern C2D_Image var_square_image[1];
extern C2D_Image var_texture_thumb_up[2];
extern C2D_Image var_texture_thumb_down[2];
