#include "headers.hpp"
#include <vector>
#include <string>

#include "scenes/library.hpp"
#include "scenes/video_player.hpp"
#include "youtube_parser/parser.hpp"
#include "ui/ui.hpp"
#include "ui/overlay.hpp"
#include "network_decoder/thumbnail_loader.hpp"
#include "data_io/favorites.hpp"
#include "util/play_queue.hpp"

#define MAX_THUMBNAIL_LOAD_REQUEST 12

// "Biblioteca" : the play queue (reorder / clear) and the favorites list
namespace Library {
bool thread_suspend = false;
bool already_init = false;
bool exiting = false;

int mode = 0; // 0 : queue, 1 : favorites
bool rebuild_request = false;
std::string clicked_url;

int CONTENT_Y_HIGHT = 240; // changes according to whether the video playing bar is drawn or not

OverlayView *on_long_tap_dialog = NULL;
ScrollView *main_view = NULL;
}; // namespace Library
using namespace Library;

static void rebuild_views();

static u32 button_background(const View &view) {
	int darkness = std::min<int>(0xFF, 0xD0 + 0x30 * (1 - view.touch_darkness));
	if (var_night_mode) {
		darkness = 0xFF - darkness;
	}
	return COLOR_GRAY(darkness);
}

static View *make_option(const std::string &text, std::function<void()> action) {
	return (new TextView(0, 0, 180, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))
	    ->set_text(text)
	    ->set_x_alignment(TextView::XAlign::CENTER)
	    ->set_y_alignment(TextView::YAlign::CENTER)
	    ->set_text_offset(0, -1)
	    ->set_on_view_released([action](View &) {
		    action();
		    main_view->reset_holding_status();
		    on_long_tap_dialog->set_is_visible(false);
		    rebuild_request = true;
		    var_need_refresh = true;
	    })
	    ->set_get_background_color(button_background);
}

static void show_dialog(const std::vector<View *> &options) {
	on_long_tap_dialog->recursive_delete_subviews();
	on_long_tap_dialog
	    ->set_subview((new VerticalListView(0, 0, 180))->set_views(options))
	    ->set_on_cancel([](OverlayView &view) {
		    main_view->reset_holding_status();
		    view.set_is_visible(false);
		    var_need_refresh = true;
	    })
	    ->set_is_visible(true);
	var_need_refresh = true;
}

static SuccinctVideoView *make_item(const std::string &id, const std::string &title, const std::string &line2,
                                    const std::string &length) {
	return (new SuccinctVideoView(0, 0, 320, VIDEO_LIST_THUMBNAIL_HEIGHT))
	    ->set_title_lines(truncate_str(title, 320 - (VIDEO_LIST_THUMBNAIL_WIDTH + 3), 2, 0.5, 0.5))
	    ->set_auxiliary_lines({line2})
	    ->set_bottom_right_overlay(length)
	    ->set_thumbnail_url(youtube_get_video_thumbnail_url_by_id(id));
}

void Library_init(void) {
	logger.info("library/init", "Initializing...");
	on_long_tap_dialog = new OverlayView(0, 0, 320, 240);
	on_long_tap_dialog->set_is_visible(false);
	favorites_load();
	Library_resume("");
	already_init = true;
}
void Library_exit(void) {
	already_init = false;
	thread_suspend = false;
	exiting = true;
	logger.info("library/exit", "Exited.");
}
void Library_suspend(void) { thread_suspend = true; }
void Library_resume(std::string arg) {
	if (arg == "favorites") {
		mode = 1;
	} else if (arg == "queue") {
		mode = 0;
	}
	if (main_view) {
		main_view->on_resume();
	}
	overlay_menu_on_resume();
	thread_suspend = false;
	var_need_refresh = true;
	rebuild_views();
}

static void rebuild_views() {
	if (main_view) {
		main_view->recursive_delete_subviews();
	}
	delete main_view;

	VerticalListView *list = (new VerticalListView(0, 0, 320))
	                             ->set_margin(SMALL_MARGIN)
	                             ->enable_thumbnail_request_update(MAX_THUMBNAIL_LOAD_REQUEST, SceneType::LIBRARY);

	if (mode == 0) {
		int n = play_queue_size();
		for (int i = 0; i < n; i++) {
			std::string url, title;
			if (!play_queue_get(i, url, title)) {
				break;
			}
			std::string id = youtube_get_video_id_by_url(url);
			SuccinctVideoView *view = make_item(id, title, "#" + std::to_string(i + 1), "");
			view->set_get_background_color(button_background)
			    ->set_on_view_released([i, url](View &) {
				    play_queue_remove_at(i); // it is playing now : it leaves the queue
				    clicked_url = url;
				    rebuild_request = true;
			    })
			    ->add_on_long_hold(35, [i](View &) {
				    show_dialog({make_option("Subir", [i]() { play_queue_move(i, -1); }),
				                 make_option("Bajar", [i]() { play_queue_move(i, 1); }),
				                 make_option("Quitar de la cola", [i]() { play_queue_remove_at(i); })});
			    });
			list->views.push_back(view);
		}
		if (!n) {
			list->views.push_back((new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL * 4))
			                          ->set_text_lines(std::vector<std::string>{"La cola esta vacia.", "Manten pulsado cualquier video", "para anadirlo."})
			                          ->set_x_alignment(TextView::XAlign::CENTER)
			                          ->set_font_size(0.5, DEFAULT_FONT_INTERVAL));
		} else {
			list->views.insert(list->views.begin(),
			                   (new TextView(0, 0, 320, 22))
			                       ->set_text("Vaciar cola")
			                       ->set_x_alignment(TextView::XAlign::CENTER)
			                       ->set_y_alignment(TextView::YAlign::CENTER)
			                       ->set_get_background_color(button_background)
			                       ->set_on_view_released([](View &) {
				                       play_queue_clear();
				                       rebuild_request = true;
			                       }));
		}
	} else {
		auto favs = favorites_get();
		for (auto &v : favs) {
			SuccinctVideoView *view = make_item(v.id, v.title, v.author, v.length_text);
			std::string id = v.id, url = youtube_get_video_url_by_id(v.id);
			view->set_get_background_color(button_background)
			    ->set_on_view_released([url](View &) { clicked_url = url; })
			    ->add_on_long_hold(35, [id, url, v](View &) {
				    show_dialog({make_option("Anadir a la cola", [url, v]() { play_queue_toggle(url, v.title); }),
				                 make_option("Quitar de favoritos", [id]() { favorites_remove(id); })});
			    });
			list->views.push_back(view);
		}
		if (!favs.size()) {
			list->views.push_back((new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL * 4))
			                          ->set_text_lines(std::vector<std::string>{"Aun no tienes favoritos.", "Usa el boton Favorito dentro", "de un video."})
			                          ->set_x_alignment(TextView::XAlign::CENTER)
			                          ->set_font_size(0.5, DEFAULT_FONT_INTERVAL));
		}
	}

	constexpr int selector_width = 180;
	main_view = (new ScrollView(0, 0, 320, 240))
	                ->set_views({(new HorizontalListView(0, 0, MIDDLE_FONT_INTERVAL))
	                                 ->set_views({(new TextView(0, 0, 320 - selector_width, MIDDLE_FONT_INTERVAL))
	                                                  ->set_text("Biblioteca")
	                                                  ->set_font_size(MIDDLE_FONT_SIZE, MIDDLE_FONT_INTERVAL),
	                                              (new SelectorView(0, 0, selector_width, MIDDLE_FONT_INTERVAL, false))
	                                                  ->set_texts({(std::function<std::string()>)[]() { return std::string("Cola"); },
	                                                               (std::function<std::string()>)[]() { return std::string("Favoritos"); }},
	                                                              mode)
	                                                  ->set_on_change([](const SelectorView &view) {
		                                                  mode = view.selected_button;
		                                                  rebuild_request = true;
	                                                  })}),
	                             (new RuleView(0, 0, 320, 3)), list});
	var_need_refresh = true;
}

void Library_draw(void) {
	Hid_info key;
	Util_hid_query_key_state(&key);

	thumbnail_set_active_scene(SceneType::LIBRARY);

	bool video_playing_bar_show = video_is_playing();
	CONTENT_Y_HIGHT = video_playing_bar_show ? 240 - VIDEO_PLAYING_BAR_HEIGHT : 240;
	main_view->update_y_range(0, CONTENT_Y_HIGHT);

	if (var_need_refresh || !var_eco_mode) {
		var_need_refresh = false;
		Draw_frame_ready();
		video_draw_top_screen();

		Draw_screen_ready(2, DEFAULT_BACK_COLOR);

		main_view->draw();
		on_long_tap_dialog->draw();

		if (video_playing_bar_show) {
			video_draw_playing_bar();
		}
		draw_overlay_menu(video_playing_bar_show ? 240 - OVERLAY_MENU_ICON_SIZE - VIDEO_PLAYING_BAR_HEIGHT
		                                         : 240 - OVERLAY_MENU_ICON_SIZE);

		if (Util_expl_query_show_flag()) {
			Util_expl_draw();
		}
		if (Util_err_query_error_show_flag()) {
			Util_err_draw();
		}
		Draw_touch_pos();
		Draw_apply_draw();
	} else {
		gspWaitForVBlank();
	}

	if (Util_err_query_error_show_flag()) {
		Util_err_main(key);
	} else if (Util_expl_query_show_flag()) {
		Util_expl_main(key);
	} else {
		if (on_long_tap_dialog->is_visible) {
			on_long_tap_dialog->update(key);
		} else {
			update_overlay_menu(&key);

			main_view->update(key);
			if (clicked_url != "") {
				global_intent.next_scene = SceneType::VIDEO_PLAYER;
				global_intent.arg = clicked_url;
				clicked_url = "";
			}
			if (video_playing_bar_show) {
				video_update_playing_bar(key);
			}
		}
		if (rebuild_request) { // after the update : the views of the list may have just been used
			rebuild_request = false;
			rebuild_views();
		}
		if (key.p_b) {
			global_intent.next_scene = SceneType::BACK;
		}
	}
}
