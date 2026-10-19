#include "headers.hpp"
#include "data_io/settings.hpp"
#include <vector>
#include <string>
#include <set>
#include "util/badges.hpp"
#include <map>
#include <numeric>

#include "scenes/watch_history.hpp"
#include "scenes/video_player.hpp"
#include "youtube_parser/parser.hpp"
#include "ui/ui.hpp"
#include "ui/notice.hpp"
#include "ui/overlay.hpp"
#include "network_decoder/thumbnail_loader.hpp"
#include "data_io/history.hpp"
#include "util/misc_tasks.hpp"

#define MAX_THUMBNAIL_LOAD_REQUEST 12

namespace WatchHistory {
bool thread_suspend = false;
bool already_init = false;
bool exiting = false;

std::vector<HistoryVideo> watch_history;
std::string clicked_url;
std::string erase_request;

int cur_sort_type = 0;
int sort_request = -1;
static bool wrapped_request = false; // the My Wrapped button was pressed (the summary opens after the views update)
static bool top_channels_request = false; // the Top channels button was pressed (the menu opens after the views update)
static long long delete_day_request = -1; // a day header was long pressed and Delete this day chosen (done after the views update)
static bool clear_old_request = false;   // Clear what is older than 30 days (handled after the views update)
static bool filter_request = false;      // the search-in-history button was pressed (the keyboard opens after the views update)
static std::string history_filter;       // only the videos whose title or channel contain this text are listed

int CONTENT_Y_HIGHT = 240; // changes according to whether the video playing bar is drawn or not

OverlayView *on_long_tap_dialog;
ScrollView *main_view = NULL;
VerticalListView *video_list_view = NULL;
}; // namespace WatchHistory
using namespace WatchHistory;

static void update_watch_history(const std::vector<HistoryVideo> &new_watch_history);

void History_init(void) {
	delete_day_request = -1; // (a request of a previous visit must not run)
	clear_old_request = false;
	top_channels_request = false;
	logger.info("history/init", "Initializing...");

	on_long_tap_dialog = new OverlayView(0, 0, 320, 240);
	on_long_tap_dialog->set_is_visible(false);

	load_watch_history();

	History_resume("");
	already_init = true;
}
void History_exit(void) {
	already_init = false;
	thread_suspend = false;
	exiting = true;

	logger.info("history/exit", "Exited.");
}
void History_suspend(void) { thread_suspend = true; }
void History_resume(std::string arg) {
	(void)arg;

	if (main_view) {
		main_view->on_resume();
	}
	overlay_menu_on_resume();
	thread_suspend = false;
	var_need_refresh = true;

	update_watch_history(get_valid_watch_history());
}

static void update_watch_history(const std::vector<HistoryVideo> &new_watch_history) {
	watch_history = new_watch_history;

	if (main_view) {
		main_view->recursive_delete_subviews();
	}
	delete main_view;

	// prepare new views
	video_list_view = (new VerticalListView(0, 0, 320))
	                      ->set_margin(SMALL_MARGIN)
	                      ->enable_thumbnail_request_update(MAX_THUMBNAIL_LOAD_REQUEST, SceneType::HISTORY);
	if (watch_history.size() > 1) { // listen to the history like a playlist
		std::vector<std::pair<std::string, std::string>> items;
		for (auto &v : watch_history) {
			if (items.size() < 40) items.push_back({youtube_get_video_url_by_id(v.id), v.title});
		}
		auto button = [](const std::string &text, int width, std::function<void()> action) {
			return (new TextView(0, 0, width, 22))
			    ->set_text(text)
			    ->set_x_alignment(TextView::XAlign::CENTER)
			    ->set_y_alignment(TextView::YAlign::CENTER)
			    ->set_get_background_color(View::STANDARD_BACKGROUND)
			    ->set_corner_radius(6)
			    ->set_on_view_released([action](View &) { action(); });
		};
		auto play = [items](bool shuffle) {
			std::vector<std::pair<std::string, std::string>> list = items;
			if (shuffle) {
				for (size_t k = list.size(); k > 1; k--) std::swap(list[k - 1], list[rand() % k]);
			}
			play_queue_clear();
			for (size_t k = 1; k < list.size(); k++) play_queue_toggle(list[k].first, list[k].second);
			clicked_url = list[0].first;
		};
		video_list_view->views.push_back((new HorizontalListView(0, 0, 22))
		                                     ->set_views({button(tr("Play all", "Reproducir todo"), 160, [play]() { play(false); }),
		                                                  button(tr("Shuffle", "Aleatorio"), 160, [play]() { play(true); })}));
	}
	{ // search in the history
		video_list_view->views.push_back((new TextView(0, 0, 320, 22))
		                                     ->set_text(history_filter == "" ? tr("Search in the history", "Buscar en el historial") : tr("Filter: ", "Filtro: ") + history_filter + tr(" (tap to change)", " (toca para cambiar)"))
		                                     ->set_x_alignment(TextView::XAlign::CENTER)
		                                     ->set_y_alignment(TextView::YAlign::CENTER)
		                                     ->set_get_background_color(View::STANDARD_BACKGROUND)
		                                     ->set_corner_radius(6)
		                                     ->set_on_view_released([](View &) { filter_request = true; }));
	}
	if (watch_history.size() > 5) { // the channels you watch most : tap one to search it
		video_list_view->views.push_back((new TextView(0, 0, 320, 22))
		                                     ->set_text(tr("Top channels", "Canales más vistos"))
		                                     ->set_x_alignment(TextView::XAlign::CENTER)
		                                     ->set_y_alignment(TextView::YAlign::CENTER)
		                                     ->set_get_background_color(View::STANDARD_BACKGROUND)
		                                     ->set_corner_radius(6)
		                                     ->set_on_view_released([](View &) { top_channels_request = true; }));
		video_list_view->views.push_back((new TextView(0, 0, 320, 22))
		                                     ->set_text(tr("My Wrapped (my numbers)", "Mi resumen (mis números)"))
		                                     ->set_x_alignment(TextView::XAlign::CENTER)
		                                     ->set_y_alignment(TextView::YAlign::CENTER)
		                                     ->set_get_background_color(View::STANDARD_BACKGROUND)
		                                     ->set_corner_radius(6)
		                                     ->set_on_view_released([](View &) { wrapped_request = true; }));
		{ // a memory: what you watched about a week (or a month) ago
			const time_t now = time(NULL);
			const HistoryVideo *memory = NULL;
			bool month = false;
			for (int pass = 0; pass < 2 && !memory; pass++) {
				const long long center = pass == 0 ? 7 * 86400 : 30 * 86400, slack = pass == 0 ? 86400 : 2 * 86400;
				for (auto &v : watch_history) {
					long long age = (long long)(now - v.last_watch_time);
					if (age < center - slack || age > center + slack || v.id == "") continue;
					if (!memory || v.my_view_count > memory->my_view_count) memory = &v, month = pass == 1;
				}
			}
			if (memory) {
				const std::string memory_url = youtube_get_video_url_by_id(memory->id);
				video_list_view->views.push_back((new TextView(0, 0, 320, 22))
				                                     ->set_text((month ? tr("A month ago: ", "Hace un mes: ") : tr("A week ago: ", "Hace una semana: ")) + truncate_first_line(memory->title, 230, 0.5))
				                                     ->set_x_alignment(TextView::XAlign::CENTER)
				                                     ->set_y_alignment(TextView::YAlign::CENTER)
				                                     ->set_get_background_color(View::STANDARD_BACKGROUND)
				                                     ->set_corner_radius(6)
				                                     ->set_on_view_released([memory_url](View &) { clicked_url = memory_url; }));
			}
		}
	}
	std::string needle = history_filter;
	for (auto &c : needle) c = tolower(c);
	if (watch_history.size() > 5) {
		video_list_view->views.push_back((new TextView(0, 0, 320, 22))
		                                     ->set_text(tr("Clear what is older than 30 days", "Borrar lo de hace más de 30 días"))
		                                     ->set_x_alignment(TextView::XAlign::CENTER)
		                                     ->set_y_alignment(TextView::YAlign::CENTER)
		                                     ->set_get_background_color(View::STANDARD_BACKGROUND)
		                                     ->set_corner_radius(6)
		                                     ->set_on_view_released([](View &) { clear_old_request = true; }));
	}
	std::map<long long, int> per_day; // how many videos each day
	for (auto &v : watch_history) per_day[(long long)v.last_watch_time / 86400]++;
	long long last_day = -1;
	const long long today = (long long)time(NULL) / 86400;
	for (auto i : watch_history) {
		if (needle != "") { // the filter
			std::string hay = i.title + " " + i.author_name;
			for (auto &c : hay) c = tolower(c);
			if (hay.find(needle) == std::string::npos) continue;
		}
		long long day = (long long)i.last_watch_time / 86400;
		if (day != last_day) { // a header for each day
			last_day = day;
			std::string title;
			if (today - day == 0) title = tr("Today", "Hoy");
			else if (today - day == 1) title = tr("Yesterday", "Ayer");
			else {
				char buf[40];
				time_t t = i.last_watch_time;
				strftime(buf, sizeof buf, "%Y/%m/%d", gmtime(&t));
				title = buf;
			}
			video_list_view->views.push_back((new TextView(0, 0, 320, 18))
			                                     ->set_text(title + " (" + std::to_string(per_day[day]) + ")")
			                                     ->set_font_size(0.5, DEFAULT_FONT_INTERVAL)
			                                     ->set_y_alignment(TextView::YAlign::CENTER)
			                                     ->set_text_offset(SMALL_MARGIN, 0)
			                                     ->set_get_text_color([]() { return (u32)COLOR_ACCENT; })
			                                     ->set_get_background_color([](const View &) { return LIGHT0_BACK_COLOR; })
			                                     ->add_on_long_hold(35, [day, title, count = per_day[day], filtered = needle != ""](View &v) { // long press : delete this whole day (asks again)
				                                     if (filtered) {
					                                     notice_show(tr("Clear the filter first", "Quita antes el filtro"));
				                                     } else {
					                                     overlay_show_menu(title, {{tr("Delete this day (", "Borrar este día (") + std::to_string(count) + ")", [day, count]() {
						                                     overlay_show_menu_deferred(tr("Sure?", "¿Seguro?"), {{tr("Yes, delete ", "Sí, borrar ") + std::to_string(count), [day]() { delete_day_request = day; }}});
					                                     }}});
				                                     }
				                                     v.reset_holding_status();
			                                     }));
		}
		std::string view_count_str;
		{
			std::string view_count_str_tmp = LOCALIZED(MY_VIEW_COUNT_WITH_NUMBER);
			for (size_t j = 0; j < view_count_str_tmp.size();) {
				if (j + 1 < view_count_str_tmp.size() && view_count_str_tmp[j] == '%' &&
				    view_count_str_tmp[j + 1] == '0') {
					view_count_str += std::to_string(i.my_view_count), j += 2;
				} else {
					view_count_str.push_back(view_count_str_tmp[j]), j++;
				}
			}
		}
		std::string last_watch_time_str;
		{
			char tmp[100];
			strftime(tmp, 100, "%Y/%m/%d %H:%M", gmtime(&i.last_watch_time));
			last_watch_time_str = tmp;
		}

		SuccinctVideoView *cur_view =
		    (new SuccinctVideoView(0, 0, 320, VIDEO_LIST_THUMBNAIL_HEIGHT))
		        ->set_title_lines(i.title_lines)
		        ->set_auxiliary_lines({i.author_name, view_count_str + " " + last_watch_time_str})
		        ->set_bottom_right_overlay(i.length_text)
		        ->set_thumbnail_url(youtube_get_video_thumbnail_url_by_id(i.id));

		cur_view
		    ->set_get_background_color([](const View &view) {
			    int darkness = std::min<int>(0xFF, 0xD0 + 0x30 * (1 - view.touch_darkness));
			    if (var_night_mode) {
				    darkness = 0xFF - darkness;
			    }
			    return COLOR_GRAY(darkness);
		    })
		    ->set_on_view_released([i](View &view) { clicked_url = youtube_get_video_url_by_id(i.id); })
		    ->add_on_long_hold(40, [i](View &view) {
			    overlay_show_video_menu(youtube_get_video_url_by_id(i.id), i.title, i.author_name, tr("Remove from history", "Quitar del historial"), [i]() {
				    erase_request = i.id;
				    var_need_refresh = true;
			    });
			    view.reset_holding_status(); // the release must not open the video
			    var_need_refresh = true;
		    });

		video_list_view->views.push_back(cur_view);
	}
	constexpr int selector_width = 180;
	main_view = (new ScrollView(0, 0, 320, 240))
		->set_views({
			(new HorizontalListView(0, 0, MIDDLE_FONT_INTERVAL))
				->set_views({
					(new TextView(0, 0, 320 - selector_width, MIDDLE_FONT_INTERVAL))
						->set_text((std::function<std::string()>) [] () {
		return LOCALIZED(WATCH_HISTORY); })
						->set_font_size(MIDDLE_FONT_SIZE, MIDDLE_FONT_INTERVAL),
					(new SelectorView(0, 0, selector_width, MIDDLE_FONT_INTERVAL, false))
						->set_texts({
							(std::function<std::string ()>) [] () { return LOCALIZED(BY_LAST_WATCH_TIME); },
							(std::function<std::string ()>) [] () {
		return LOCALIZED(BY_MY_VIEW_COUNT); }
						}, cur_sort_type)
						->set_on_change([](const SelectorView &view) {
		sort_request = cur_sort_type = view.selected_button; })
}),
			(new RuleView(0, 0, 320, 3)),
			video_list_view
});
}

void History_draw(void) {
	Hid_info key;
	Util_hid_query_key_state(&key);

	thumbnail_set_active_scene(SceneType::HISTORY);

	bool video_playing_bar_show = video_is_playing();
	{
		int nav_bottom = video_playing_bar_show ? 240 - VIDEO_PLAYING_BAR_HEIGHT : 240;
		overlay_set_nav(true, nav_bottom);
		CONTENT_Y_HIGHT = nav_bottom - NAV_BAR_HEIGHT;
	}
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
			if (wrapped_request) { // a summary of what you watch, made from the history
				wrapped_request = false;
				std::map<std::string, int> counts;
				int hours[24] = {0};
				std::set<long long> days;
				int total_views = 0, this_week = 0;
				const time_t now = time(NULL);
				for (auto &video : watch_history) {
					if (video.author_name != "") counts[video.author_name] += std::max(1, video.my_view_count);
					total_views += std::max(1, video.my_view_count);
					time_t t = video.last_watch_time;
					struct tm *local = localtime(&t);
					if (local) hours[local->tm_hour]++;
					days.insert((long long)video.last_watch_time / 86400);
					if (now - video.last_watch_time < 7 * 86400) this_week++;
				}
				std::string top_channel = "-";
				int top_count = 0;
				for (auto &c : counts) {
					if (c.second > top_count) top_count = c.second, top_channel = c.first;
				}
				int busiest = 0;
				for (int h = 1; h < 24; h++) if (hours[h] > hours[busiest]) busiest = h;
				int streak = 0; // days in a row with something watched, counting back from today (or yesterday)
				long long day = (long long)now / 86400;
				if (!days.count(day)) day--;
				while (days.count(day)) streak++, day--;
				int this_month = 0; // videos watched in the last 30 days
					for (auto &video : watch_history) if (now - video.last_watch_time < 30 * 86400) this_month++;
					int best_streak = 0, run = 0; // the longest run of consecutive days with something watched
					long long previous_day = -10;
					for (long long d : days) { // std::set is sorted
						run = (d == previous_day + 1) ? run + 1 : 1;
						best_streak = std::max(best_streak, run);
						previous_day = d;
					}
					int play_seconds = (int)var_total_play_seconds;
				auto row = [](const std::string &text) { return std::pair<std::string, std::function<void()>>{text, []() {}}; };
				std::vector<std::pair<std::string, std::function<void()>>> rows = {
				    row(tr("Videos in the history: ", "Vídeos en el historial: ") + std::to_string(watch_history.size())),
				    row(tr("Times watched: ", "Veces vistos: ") + std::to_string(total_views)),
				    row(tr("Time playing: ", "Tiempo reproduciendo: ") + std::to_string(play_seconds / 3600) + " h " + std::to_string(play_seconds / 60 % 60) + " min"),
				    row(tr("Top channel: ", "Canal top: ") + truncate_first_line(top_channel, 150, 0.5)),
				    row(tr("Favorite hour: ", "Hora favorita: ") + std::to_string(busiest) + ":00"),
				    row(tr("This week: ", "Esta semana: ") + std::to_string(this_week)),
				    row(tr("Days in a row: ", "Días seguidos: ") + std::to_string(streak)),
					    row(tr("This month: ", "Este mes: ") + std::to_string(this_month)),
					    row(tr("Longest streak: ", "Mejor racha: ") + std::to_string(best_streak)),
				    row(tr("Playing today: ", "Hoy reproduciendo: ") + std::to_string(var_play_day == local_day_number() ? (int)var_play_today / 60 : 0) + " min")};
				auto badges = badges_compute();
				int unlocked = 0;
				for (auto &b : badges) unlocked += b.done;
				std::vector<std::pair<std::string, std::function<void()>>> badge_rows;
				for (auto &b : badges) badge_rows.push_back({std::string(b.done ? "[x] " : "[ ] ") + b.name, []() {}});
				const HistoryVideo *replayed = NULL;
				for (auto &video : watch_history) {
					if (video.my_view_count >= 2 && (!replayed || video.my_view_count > replayed->my_view_count)) replayed = &video;
				}
				if (replayed) {
					const std::string replay_url = youtube_get_video_url_by_id(replayed->id);
					rows.push_back({tr("Most replayed: ", "Más repetido: ") + truncate_first_line(replayed->title, 190, 0.5) + " (" + std::to_string(replayed->my_view_count) + ")", [replay_url]() { clicked_url = replay_url; }});
				}
				rows.push_back({tr("Badges: ", "Logros: ") + std::to_string(unlocked) + "/" + std::to_string(badges.size()), [badge_rows]() {
					overlay_show_menu_deferred(tr("Badges", "Logros"), badge_rows);
				}});
				overlay_show_menu(tr("My Wrapped", "Mi resumen"), rows);
			}
			if (top_channels_request) {
				top_channels_request = false;
				std::map<std::string, int> counts;
				for (auto &video : watch_history) {
					if (video.author_name != "") counts[video.author_name]++;
				}
				std::vector<std::pair<int, std::string>> ranking;
				for (auto &c : counts) ranking.push_back({c.second, c.first});
				std::sort(ranking.begin(), ranking.end(), [](const std::pair<int, std::string> &a, const std::pair<int, std::string> &b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
				std::vector<std::pair<std::string, std::function<void()>>> options;
				for (size_t k = 0; k < ranking.size() && k < 7; k++) {
					std::string name = ranking[k].second;
					options.push_back({truncate_first_line(name, 200, 0.5) + " (" + std::to_string(ranking[k].first) + ")", [name]() {
						global_intent.next_scene = SceneType::SEARCH;
						global_intent.arg = "q:" + name;
					}});
				}
				if (options.size()) overlay_show_menu(tr("Top channels", "Canales más vistos"), options);
			}
			if (delete_day_request >= 0) {
				long long day = delete_day_request;
				delete_day_request = -1;
				std::vector<HistoryVideo> kept;
				int removed = 0;
				for (auto &video : watch_history) {
					if ((long long)video.last_watch_time / 86400 == day) {
						history_erase_by_id(video.id);
						removed++;
					} else {
						kept.push_back(video);
					}
				}
				update_watch_history(kept);
				misc_tasks_request(TASK_SAVE_HISTORY);
				notice_show(tr("Removed: ", "Quitado: ") + std::to_string(removed));
			}
			if (clear_old_request) {
				clear_old_request = false;
				time_t limit = time(NULL) - 30 * 86400;
				std::vector<HistoryVideo> kept;
				int removed = 0;
				for (auto &video : watch_history) {
					if (video.last_watch_time < limit) {
						history_erase_by_id(video.id);
						removed++;
					} else {
						kept.push_back(video);
					}
				}
				update_watch_history(kept);
				misc_tasks_request(TASK_SAVE_HISTORY);
				notice_show(tr("Removed: ", "Quitado: ") + std::to_string(removed));
			}
			if (filter_request) {
				filter_request = false;
				SwkbdState keyboard;
				swkbdInit(&keyboard, SWKBD_TYPE_NORMAL, 2, 60);
				swkbdSetFeatures(&keyboard, SWKBD_DEFAULT_QWERTY);
				swkbdSetHintText(&keyboard, tr("Title or channel (empty : all)", "Título o canal (vacío: todo)").c_str());
				swkbdSetInitialText(&keyboard, history_filter.c_str());
				swkbdSetButton(&keyboard, SWKBD_BUTTON_LEFT, LOCALIZED(CANCEL).c_str(), false);
				swkbdSetButton(&keyboard, SWKBD_BUTTON_RIGHT, LOCALIZED(OK).c_str(), true);
				char buffer[128] = {0};
				auto pressed = swkbdInputText(&keyboard, buffer, sizeof(buffer));
				if (pressed == SWKBD_BUTTON_RIGHT) {
					history_filter = buffer;
					update_watch_history(watch_history);
				}
				var_need_refresh = true;
			}
			if (sort_request != -1) {
				auto tmp_watch_history = watch_history;
				std::sort(tmp_watch_history.begin(), tmp_watch_history.end(),
				          [](const HistoryVideo &i, const HistoryVideo &j) {
					          if (sort_request == 0) {
						          return i.last_watch_time > j.last_watch_time;
					          }
					          if (sort_request == 1) {
						          return i.my_view_count > j.my_view_count;
					          }
					          // should not reach here
					          return false;
				          });
				update_watch_history(tmp_watch_history);

				sort_request = -1;
			}
			if (erase_request != "") {
				// erase
				std::vector<HistoryVideo> tmp_watch_history;
				for (auto video : watch_history) {
					if (video.id != erase_request) {
						tmp_watch_history.push_back(video);
					}
				}
				update_watch_history(tmp_watch_history);
				history_erase_by_id(erase_request);
				misc_tasks_request(TASK_SAVE_HISTORY);

				erase_request = "";
			}
			if (video_playing_bar_show) {
				video_update_playing_bar(key);
			}
		}

		if (key.p_b) {
			global_intent.next_scene = SceneType::BACK;
		}
	}
}
