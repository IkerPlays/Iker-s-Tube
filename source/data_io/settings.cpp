#include "headers.hpp"
#include "settings.hpp"
#include "youtube_parser/parser.hpp"
#include "scenes/video_player.hpp"

#define SETTINGS_FILE_PATH (DEF_MAIN_DIR + "settings.txt")

void load_settings() {
	// "Reset the settings" (Settings, Data) leaves a flag : the settings file is dropped at the next start, so the defaults load
	if (access((DEF_MAIN_DIR + "reset_settings.flag").c_str(), F_OK) == 0) {
		remove(SETTINGS_FILE_PATH.c_str());
		remove((DEF_MAIN_DIR + "reset_settings.flag").c_str());
	}
	static char buf[0x4001];
	memset(buf, 0, sizeof buf);
	u32 read_size;
	Result_with_string result = Path(SETTINGS_FILE_PATH).read_file((u8 *)buf, 0x4000, read_size);
	logger.info(DEF_SEM_INIT_STR, "read_file()..." + result.string + result.error_description, result.code);
	auto settings = parse_xml_like_text(buf);

	auto load_int = [&](std::string key, int default_value) {
		if (!settings.count(key)) {
			return default_value;
		}
		char *end;
		long res = strtol(settings[key].c_str(), &end, 10);
		if (*end) {
			return default_value;
		}
		res = std::min<long>(res, std::numeric_limits<int>::max());
		res = std::max<long>(res, std::numeric_limits<int>::min());
		return (int)res;
	};
	auto load_double = [&](std::string key, double default_value) {
		if (!settings.count(key)) {
			return default_value;
		}
		char *end;
		double res = strtod(settings[key].c_str(), &end);
		if (*end) {
			return default_value;
		}
		return res;
	};
	auto load_string = [&](std::string key, std::string default_value) {
		return settings.count(key) ? settings[key] : default_value;
	};
	var_lang = load_string("lang_ui", "en");
	if (var_lang != "en" && var_lang != "ja" && var_lang != "de" && var_lang != "fr" && var_lang != "it") {
		var_lang = "en";
	}
	var_lang_content = load_string("lang_content", "en");
	if (var_lang_content != "en" && var_lang_content != "ja" && var_lang_content != "de" && var_lang_content != "fr" &&
	    var_lang_content != "it") {
		var_lang_content = "en";
	}
	var_lcd_brightness = load_int("lcd_brightness", 100);
	if (var_lcd_brightness < 15 || var_lcd_brightness > 163) {
		var_lcd_brightness = 100;
	}
	var_time_to_turn_off_lcd = load_int("time_to_turn_off_lcd", 150);
	if (var_time_to_turn_off_lcd < 10 && var_time_to_turn_off_lcd != 0) {
		var_time_to_turn_off_lcd = 150;
	}
	var_eco_mode = load_int("eco_mode", 1);
	var_screen_off_mode = load_int("screen_off_mode", 0);
	var_full_screen_mode = load_int("full_screen_mode", 0);
	var_full_dislike_like_count = load_int("full_dislike_like_count", 0);
	var_hide_pointer = load_int("hide_pointer", 0);
	var_night_mode = load_int("dark_theme", 1);
	var_caption_size = std::max(0, std::min(2, load_int("caption_size", 1)));
	var_caption_color = std::max(0, std::min(3, load_int("caption_color", 0)));
	var_auto_quality = load_int("auto_quality", 1);
	var_auto_quality_cap = load_int("auto_quality_cap", 0);
	var_auto_dim = load_int("auto_dim", 0);
	var_text_scale = std::max(0, std::min(2, load_int("text_scale", 1)));
	var_start_scene = std::max(0, std::min(3, load_int("start_scene", 0)));
	var_auto_theme = load_int("auto_theme", 0);
	var_grid_view = load_int("grid_view", 0);
	var_data_saver = load_int("data_saver", 0);
	var_data_limit_index = std::max(0, std::min(5, load_int("data_limit_index", 0)));
	var_total_play_seconds = load_double("total_play_seconds", 0.0);
	var_button_x = std::max(0, std::min(10, load_int("button_x", 1)));
	var_button_y = std::max(0, std::min(10, load_int("button_y", 4)));
	var_button_l = std::max(0, std::min(10, load_int("button_l", 3)));
	var_button_r = std::max(0, std::min(10, load_int("button_r", 2)));
	var_left_handed = load_int("left_handed", 0);
	var_custom_hue = std::max(0, std::min(359, load_int("custom_hue", 200)));
	var_high_contrast = load_int("high_contrast", 0);
	var_welcome_done = load_int("welcome_done", 0);
	var_mark_watched = load_int("mark_watched", 1);
	var_offline_autodelete = load_int("offline_autodelete", 0);
	var_hide_shorts = load_int("hide_shorts", 0);
	var_incognito = load_int("incognito", 0);
	var_tour_done = load_int("tour_done", 0);
	var_autoplay_delay = std::max(0, std::min(5, load_int("autoplay_delay", 0)));
	var_studio_privacy = std::max(0, std::min(2, load_int("studio_privacy", 0)));
	var_chat_slow = std::max(0, std::min(1, load_int("chat_slow", 0)));
	var_channels_unsorted = std::max(0, std::min(1, load_int("channels_unsorted", 0)));
	var_hide_explore = std::max(0, std::min(1, load_int("hide_explore", 0)));
	var_studio_category = std::max(0, std::min(30, load_int("studio_category", 0)));
	var_autoplay_skip_watched = std::max(0, std::min(1, load_int("autoplay_skip_watched", 0)));
	var_download_quality = std::max(0, std::min(3, load_int("download_quality", 0)));
	var_auto_chat = std::max(0, std::min(1, load_int("auto_chat", 0)));
	var_default_speed = std::max(0, std::min(5, load_int("default_speed", 0)));
	var_wl_autoremove = std::max(0, std::min(1, load_int("wl_autoremove", 0)));
	var_hide_studio = std::max(0, std::min(1, load_int("hide_studio", 0)));
	var_download_window = std::max(0, std::min(1, load_int("download_window", 0)));
	var_sponsorblock = std::max(0, std::min(2, load_int("sponsorblock", 0)));
	var_hide_watched = load_int("hide_watched", 0);
	var_resume_videos = load_int("resume_videos", 1);
	var_theme = std::max(0, std::min(6, load_int("color_theme", 0)));
	var_large_thumbnails = load_int("large_thumbnails", 0);
	var_disable_pull_to_refresh = load_int("disable_pull_to_refresh", 0);
	var_community_image_size =
	    std::min(COMMUNITY_IMAGE_SIZE_MAX,
	             std::max(COMMUNITY_IMAGE_SIZE_MIN, load_int("community_image_size", COMMUNITY_IMAGE_SIZE_DEFAULT)));
	var_autoplay_level = std::min(2, std::max(0, load_int("autoplay_level_v2", 1)));
	var_loop_mode = std::min(2, std::max(0, load_int("loop_mode", 0)));
	var_video_quality = std::min(480, std::max(0, load_int("video_quality", var_is_new3ds ? 360 : 144)));
	var_forward_buffer_ratio = std::max(0.1, std::min(1.0, load_double("forward_buffer_ratio", 0.8)));
	var_av_sync_ms = std::max(-300.0, std::min(300.0, load_double("av_sync_ms", 60.0)));
	var_show_devbuilds = load_int("show_devbuilds", 0) ? 1 : 0;
	var_audio_screens_off = load_int("audio_screens_off", 1) ? 1 : 0;
	var_data_total_bytes = std::max(0.0, load_double("data_total_bytes", 0.0));
	var_history_enabled = load_int("history_enabled", 1);
	var_video_show_debug_info = load_int("video_show_debug_info", 0);
	var_player_response = load_int("player_response", 1); // (Beta 24.1, switch to Android VR for now)
	var_video_linear_filter = load_int("linear_filter", 1);
	var_dpad_scroll_speed0 = std::max(1.0, std::min(12.0, load_double("dpad_scroll_speed0", 6.0)));
	var_dpad_scroll_speed1 = std::max(var_dpad_scroll_speed0, std::min(12.0, load_double("dpad_scroll_speed1", 9.0)));
	var_dpad_scroll_speed1_threshold = std::max(0.3, std::min(5.0, load_double("dpad_scroll_speed1_threshold", 2.0)));
	// For old ThirdTube and FourthTube <32 config files that used the wrong spelling of "threshold"
	var_dpad_scroll_speed1_threshold_legacy =
	    std::max(0.3, std::min(5.0, load_double("dpad_scroll_speed1_threashold", 2.0)));
	if (var_dpad_scroll_speed1_threshold_legacy != DPAD_SCROLL_THRESHOLD_DEFAULT &&
	    var_dpad_scroll_speed1_threshold == DPAD_SCROLL_THRESHOLD_DEFAULT) {
		var_dpad_scroll_speed1_threshold = var_dpad_scroll_speed1_threshold_legacy;
	}

	Util_cset_set_wifi_state(true);
	Util_cset_set_screen_brightness(true, true, var_lcd_brightness);
	youtube_change_content_language(var_lang_content);
	video_set_linear_filter_enabled(var_video_linear_filter);
	video_set_show_debug_info(var_video_show_debug_info);
}
void save_settings() {
	std::string data;
	auto add_str = [&](const std::string &key, const std::string &val) {
		data += "<" + key + ">" + val + "</" + key + ">\n";
	};
	auto add_int = [&](const std::string &key, int val) {
		data += "<" + key + ">" + std::to_string(val) + "</" + key + ">\n";
	};
	auto add_double = [&](const std::string &key, double val) {
		data += "<" + key + ">" + std::to_string(val) + "</" + key + ">\n";
	};

	add_str("lang_ui", var_lang);
	add_str("lang_content", var_lang_content);
	add_int("lcd_brightness", var_lcd_brightness);
	add_int("time_to_turn_off_lcd", var_time_to_turn_off_lcd);
	add_int("eco_mode", var_eco_mode);
	add_int("screen_off_mode", var_screen_off_mode);
	add_int("full_screen_mode", var_full_screen_mode);
	add_int("full_dislike_like_count", var_full_dislike_like_count);
	add_int("hide_pointer", var_hide_pointer);
	add_int("dark_theme", var_night_mode);
	add_int("color_theme", var_theme);
	add_int("resume_videos", var_resume_videos);
	add_int("mark_watched", var_mark_watched);
	add_int("offline_autodelete", var_offline_autodelete);
	add_int("hide_shorts", var_hide_shorts);
	add_int("incognito", var_incognito);
	add_int("tour_done", var_tour_done);
	add_int("autoplay_delay", var_autoplay_delay);
	add_int("download_window", var_download_window);
	add_int("hide_studio", var_hide_studio);
	add_int("wl_autoremove", var_wl_autoremove);
	add_int("default_speed", var_default_speed);
	add_int("auto_chat", var_auto_chat);
	add_int("download_quality", var_download_quality);
	add_int("autoplay_skip_watched", var_autoplay_skip_watched);
	add_int("studio_category", var_studio_category);
	add_int("hide_explore", var_hide_explore);
	add_int("channels_unsorted", var_channels_unsorted);
	add_int("chat_slow", var_chat_slow);
	add_int("studio_privacy", var_studio_privacy);
	add_int("sponsorblock", var_sponsorblock);
	add_int("hide_watched", var_hide_watched);
	add_int("welcome_done", var_welcome_done);
	add_int("custom_hue", var_custom_hue);
	add_int("high_contrast", var_high_contrast);
	add_int("button_x", var_button_x);
	add_int("button_y", var_button_y);
	add_int("left_handed", var_left_handed);
	add_int("button_l", var_button_l);
	add_int("button_r", var_button_r);
	add_int("data_saver", var_data_saver);
	add_int("data_limit_index", var_data_limit_index);
	add_double("total_play_seconds", var_total_play_seconds);
	add_int("grid_view", var_grid_view);
	add_int("start_scene", var_start_scene);
	add_int("auto_theme", var_auto_theme);
	add_int("text_scale", var_text_scale);
	add_int("auto_dim", var_auto_dim);
	add_int("auto_quality", var_auto_quality);
	add_int("auto_quality_cap", var_auto_quality_cap);
	add_int("caption_size", var_caption_size);
	add_int("caption_color", var_caption_color);
	add_int("large_thumbnails", var_large_thumbnails);
	add_int("disable_pull_to_refresh", var_disable_pull_to_refresh);
	add_int("community_image_size", var_community_image_size);
	add_int("autoplay_level_v2", var_autoplay_level);
	add_int("loop_mode", var_loop_mode);
	add_int("video_quality", var_video_quality);
	add_double("forward_buffer_ratio", var_forward_buffer_ratio);
	add_double("av_sync_ms", var_av_sync_ms);
	add_int("show_devbuilds", var_show_devbuilds);
	add_int("audio_screens_off", var_audio_screens_off);
	add_double("data_total_bytes", var_data_total_bytes + (double)var_data_session_bytes);
	add_int("history_enabled", var_history_enabled);
	add_int("video_show_debug_info", var_video_show_debug_info);
	add_int("player_response", var_player_response);
	add_int("linear_filter", var_video_linear_filter);
	add_double("dpad_scroll_speed0", var_dpad_scroll_speed0);
	add_double("dpad_scroll_speed1", var_dpad_scroll_speed1);
	add_double("dpad_scroll_speed1_threshold", var_dpad_scroll_speed1_threshold);

	Result_with_string result = Path(SETTINGS_FILE_PATH).write_file((u8 *)data.c_str(), data.size());
	logger.info("settings/save", "write_file()..." + result.string + result.error_description, result.code);
}
