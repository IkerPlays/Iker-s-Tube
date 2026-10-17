#include "headers.hpp"
#include <vector>
#include <string>
#include <set>
#include <map>
#include <numeric>

#include "scenes/search.hpp"
#include "scenes/video_player.hpp"
#include "youtube_parser/parser.hpp"
#include "ui/overlay.hpp"
#include "ui/colors.hpp"
#include "ui/ui.hpp"
#include "network_decoder/thumbnail_loader.hpp"
#include "network_decoder/network_io.hpp"
#include "util/async_task.hpp"
#include "data_io/search_history.hpp"
#include "util/result_filter.hpp"
#include "data_io/last_played.hpp"
#include "data_io/history.hpp"
#include "util/play_queue.hpp"
#include "ui/qr_scan.hpp"
#include "ui/notice.hpp"
#include "scenes/account_tasks.hpp"
#include "youtube_parser/account.hpp"

#define SEARCH_BOX_MARGIN 4
#define SEARCH_ROW_HEIGHT 25
#define FILTER_ROW_HEIGHT 20

#define URL_BUTTON_WIDTH 60

#define MAX_THUMBNAIL_LOAD_REQUEST 12

namespace Search {
bool thread_suspend = false;
bool already_init = false;
bool exiting = false;

Mutex resource_lock;
std::string cur_search_word = "";

static size_t get_next_utf8_char_pos(const std::string &text, size_t pos) {
	if (pos >= text.length()) {
		return text.length();
	}

	unsigned char c = text[pos];
	if ((c & 0x80) == 0) {
		return pos + 1;
	} else if ((c & 0xE0) == 0xC0) {
		return pos + 2;
	} else if ((c & 0xF0) == 0xE0) {
		return pos + 3;
	} else if ((c & 0xF8) == 0xF0) {
		return pos + 4;
	}
	return pos + 1;
}

std::string truncate_search_text_for_display(const std::string &text, double max_width, double font_size) {
	float text_width = Draw_get_width(text, font_size);

	if (text_width <= max_width - SMALL_MARGIN * 2) {
		return text;
	}

	std::string ellipsis = "...";
	float ellipsis_width = Draw_get_width(ellipsis, font_size);
	float available_width = max_width - SMALL_MARGIN * 2 - ellipsis_width;

	size_t pos = 0;
	size_t last_valid_pos = 0;

	while (pos < text.length()) {
		size_t next_pos = get_next_utf8_char_pos(text, pos);
		if (next_pos > text.length()) {
			break;
		}

		std::string substr = text.substr(0, next_pos);
		float substr_width = Draw_get_width(substr, font_size);

		if (substr_width <= available_width) {
			last_valid_pos = next_pos;
			pos = next_pos;
		} else {
			break;
		}
	}

	if (last_valid_pos > 0 && last_valid_pos < text.length()) {
		return text.substr(0, last_valid_pos) + ellipsis;
	}

	return text;
}
YouTubeSearchResult search_result;
bool search_done = false;

std::string last_url_input = "https://m.youtube.com/watch?v=";

bool clicked_is_channel;
std::string clicked_url;

TextView *toast_view = (new TextView((320 - 150) / 2, 190, 150, DEFAULT_FONT_INTERVAL + SMALL_MARGIN));
int toast_view_visible_frames_left = 0;

const int RESULT_Y_LOW = SEARCH_ROW_HEIGHT + FILTER_ROW_HEIGHT; // search row + filter row
int RESULT_Y_HIGH = 240; // changes according to whether the video playing bar is drawn or not

VerticalListView *main_view;
VerticalListView *top_bar_view = new VerticalListView(0, 0, 320);
// annonymous HorizontalListView
TextView *search_box_view;
TextView *url_button_view;
// annonymous RuleView : height 1
ScrollView *result_view;
VerticalListView *result_list_view =
    (new VerticalListView(0, 0, 320))
        ->set_margin(SMALL_MARGIN)
        ->set_flow(true)
        ->enable_thumbnail_request_update(MAX_THUMBNAIL_LOAD_REQUEST, SceneType::SEARCH);
View *result_bottom_view = new EmptyView(0, 0, 320, 0);
}; // namespace Search
using namespace Search;

TextView *Search_get_toast_view() { return toast_view; }
View *Search_get_search_bar_view() { return top_bar_view; }

static void load_search_results(void *);
static void load_more_search_results(void *);

// search filters : the button below the search box cycles through them ("params" of the search request)
static const struct {
	const char *name;
	const char *params;
} search_filters[] = {{"Todos", ""},
                      {"Solo videos", "EgIQAQ=="},
                      {"Hoy", "EgQIAhAB"},
                      {"Esta semana", "EgQIAxAB"},
                      {"Este mes", "EgQIBBAB"},
                      {"Cortos (< 4 min)", "EgQQARgB"},
                      {"Largos (> 20 min)", "EgQQARgD"}};
static std::string search_filter_name(int i) {
	static const char *en[] = {"All", "Videos only", "Today", "This week", "This month", "Short (< 4 min)", "Long (> 20 min)"};
	return tr(en[i], search_filters[i].name);
}
static int search_filter_index = 0;
static int account_feed_kind = 0; // 1 : recommendations, 2 : notifications of the YouTube account
static void load_account_feed(void *);
static bool history_clear_request = false;
static std::string history_remove_request; // a search to forget (long press), done after the update
static bool qr_scan_pending = false;

// starts a search. Must be called while resource_lock is held (from the update of this scene)
static bool shorts_feed = false; // the search is the Shorts feed : its videos keep playing one after another
static void start_search(const std::string &word, bool feed = false) {
	shorts_feed = feed;
	cur_search_word = word;
	double search_box_width = 320 - SEARCH_BOX_MARGIN * 3 - URL_BUTTON_WIDTH;
	search_box_view->set_text(truncate_search_text_for_display(word, search_box_width, 0.5));
	search_box_view->set_get_text_color([]() { return DEFAULT_TEXT_COLOR; });
	search_history_add(word);
	remove_all_async_tasks_with_type(load_search_results);
	remove_all_async_tasks_with_type(load_more_search_results);
	queue_async_task(load_search_results, NULL);
	var_need_refresh = true;
}

// before the first search the result list shows the recent searches (tap one to search it again)
static void show_search_history() {
	if (!result_list_view || search_done || is_async_task_running(load_search_results) || result_list_view->views.size()) {
		return;
	}
	auto words = search_history_get();
	// Explore : ready-made searches (YouTube no longer offers real Music / Trending feeds to this client)
	result_list_view->views.push_back((new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN))
	                                      ->set_text(tr("Explore", "Explorar"))
	                                      ->set_font_size(0.5, DEFAULT_FONT_INTERVAL)
	                                      ->set_get_text_color([]() { return LIGHT1_TEXT_COLOR; }));
	const std::string explore_labels[] = {tr("Music", "Música"), tr("Trending", "Tendencias"), tr("Gaming", "Videojuegos"), tr("Live", "Directos"), tr("Movie trailers", "Tráilers de películas"), tr("News", "Noticias"), tr("Shorts", "Shorts"), tr("Learn something", "Aprende algo"), tr("Funny", "Humor")};
	static const char *explore_queries[] = {"music 2026 official video", "trending today", "gaming highlights",
	                                        "live now", "new movie trailers", "world news today", "#shorts", "how it works explained", "funny moments"};
	for (int i = 0; i < 9; i++) {
		std::string query = explore_queries[i];
		bool feed = i == 6; // Shorts
		result_list_view->views.push_back((new TextView(SMALL_MARGIN * 2, 0, 320 - SMALL_MARGIN * 2, 26))
		                                      ->set_text(explore_labels[i])
		                                      ->set_font_size(0.55, DEFAULT_FONT_INTERVAL)
		                                      ->set_y_alignment(TextView::YAlign::CENTER)
		                                      ->set_get_text_color([]() { return COLOR_ACCENT; })
		                                      ->set_get_background_color(View::STANDARD_BACKGROUND)
		                                      ->set_on_view_released([query, feed](View &) { start_search(query, feed); }));
	}
	// Discover : more from the channels you watched most recently
	{
		auto watched = get_valid_watch_history();
		std::sort(watched.begin(), watched.end(),
		          [](const HistoryVideo &x, const HistoryVideo &y) { return x.last_watch_time > y.last_watch_time; });
		std::vector<std::string> authors;
		for (auto &v : watched) {
			if (v.author_name != "" && std::find(authors.begin(), authors.end(), v.author_name) == authors.end()) {
				authors.push_back(v.author_name);
			}
			if (authors.size() >= 3) break;
		}
		if (authors.size()) {
			result_list_view->views.push_back((new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN))
			                                      ->set_text(tr("Discover", "Descubrir"))
			                                      ->set_font_size(0.5, DEFAULT_FONT_INTERVAL)
			                                      ->set_get_text_color([]() { return LIGHT1_TEXT_COLOR; }));
			for (auto &author : authors) {
				result_list_view->views.push_back((new TextView(SMALL_MARGIN * 2, 0, 320 - SMALL_MARGIN * 2, 26))
				                                      ->set_text(truncate_search_text_for_display(tr("More from ", "Más de ") + author, 300, 0.55))
				                                      ->set_font_size(0.55, DEFAULT_FONT_INTERVAL)
				                                      ->set_y_alignment(TextView::YAlign::CENTER)
				                                      ->set_get_background_color(View::STANDARD_BACKGROUND)
				                                      ->set_on_view_released([author](View &) { start_search(author); }));
			}
		}
	}
	{ // your YouTube account : recommendations and notifications (OAuth login needed)
		result_list_view->views.push_back((new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN))
		                                      ->set_text(tr("Your account", "Tu cuenta"))
		                                      ->set_font_size(0.5, DEFAULT_FONT_INTERVAL)
		                                      ->set_get_text_color([]() { return LIGHT1_TEXT_COLOR; }));
		for (int kind = 1; kind <= 2; kind++) {
			result_list_view->views.push_back(
			    (new TextView(SMALL_MARGIN * 2, 0, 320 - SMALL_MARGIN * 2, 26))
			        ->set_text(kind == 1 ? tr("Recommended for you", "Recomendados para ti") : tr("Notifications", "Notificaciones"))
			        ->set_font_size(0.55, DEFAULT_FONT_INTERVAL)
			        ->set_y_alignment(TextView::YAlign::CENTER)
			        ->set_get_text_color([]() { return COLOR_ACCENT; })
			        ->set_get_background_color(View::STANDARD_BACKGROUND)
			        ->set_on_view_released([kind](View &) {
				        if (!account_ready()) return;
				        account_feed_kind = kind;
				        if (!is_async_task_running(load_account_feed)) {
					        notice_show(tr("Loading...", "Cargando..."));
					        queue_async_task(load_account_feed, NULL);
				        }
			        }));
		}
	}
	result_list_view->views.push_back((new TextView(SMALL_MARGIN * 2, 0, 320 - SMALL_MARGIN * 2, 26))
	                                      ->set_text(tr("Scan a QR code (camera)", "Escanear un código QR (cámara)"))
	                                      ->set_font_size(0.55, DEFAULT_FONT_INTERVAL)
	                                      ->set_y_alignment(TextView::YAlign::CENTER)
	                                      ->set_get_text_color([]() { return COLOR_ACCENT; })
	                                      ->set_get_background_color(View::STANDARD_BACKGROUND)
	                                      ->set_on_view_released([](View &) { qr_scan_pending = true; }));
	{ // continue watching : the last video that was played
		std::string last_id, last_title;
		if (last_played_get(last_id, last_title)) {
			std::string url = youtube_get_video_url_by_id(last_id);
			result_list_view->views.push_back((new TextView(SMALL_MARGIN * 2, 0, 320 - SMALL_MARGIN * 2, 26))
			                                      ->set_text(truncate_search_text_for_display(tr("Continue: ", "Seguir: ") + last_title, 300, 0.55))
			                                      ->set_font_size(0.55, DEFAULT_FONT_INTERVAL)
			                                      ->set_y_alignment(TextView::YAlign::CENTER)
			                                      ->set_get_text_color([]() { return COLOR_ACCENT; })
			                                      ->set_get_background_color(View::STANDARD_BACKGROUND)
			                                      ->set_on_view_released([url](View &) { clicked_url = url; clicked_is_channel = false; }));
		}
	}
	if (!words.size()) {
		return;
	}
	result_list_view->views.push_back((new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN))
	                                      ->set_text(tr("Recent searches", "Búsquedas recientes"))
	                                      ->set_font_size(0.5, DEFAULT_FONT_INTERVAL)
	                                      ->set_get_text_color([]() { return LIGHT1_TEXT_COLOR; }));
	for (auto &word : words) {
		result_list_view->views.push_back((new TextView(SMALL_MARGIN * 2, 0, 320 - SMALL_MARGIN * 2, 26))
		                                      ->set_text(truncate_search_text_for_display(word, 300, 0.55))
		                                      ->set_font_size(0.55, DEFAULT_FONT_INTERVAL)
		                                      ->set_y_alignment(TextView::YAlign::CENTER)
		                                      ->set_get_background_color(View::STANDARD_BACKGROUND)
		                                      ->set_on_view_released([word](View &) { start_search(word); })
		                                      ->add_on_long_hold(35, [word](View &) {
			                                      history_remove_request = word;
			                                      var_need_refresh = true;
		                                      }));
	}
	result_list_view->views.push_back((new TextView(0, 0, 320, 24))
	                                      ->set_text(tr("Clear history", "Borrar historial"))
	                                      ->set_x_alignment(TextView::XAlign::CENTER)
	                                      ->set_y_alignment(TextView::YAlign::CENTER)
	                                      ->set_get_background_color(View::STANDARD_BACKGROUND)
	                                      ->set_on_view_released([](View &) {
		                                      history_clear_request = true; // done after the update : the views can't be deleted while they are being updated
		                                      var_need_refresh = true;
	                                      }));
}

void Search_init(void) {
	logger.info("search/init", "Initializing...");
	Result_with_string result;

	search_box_view = (new TextView(0, SEARCH_BOX_MARGIN, 320 - SEARCH_BOX_MARGIN * 3 - URL_BUTTON_WIDTH,
	                                SEARCH_ROW_HEIGHT - SEARCH_BOX_MARGIN * 2 - 1));
	search_box_view->set_text_offset(0, -1);
	search_box_view->set_text((std::function<std::string()>)[]() { return LOCALIZED(SEARCH_HINT); });
	search_box_view->set_get_background_color([](const View &) { return LIGHT0_BACK_COLOR; });
	search_box_view->set_get_text_color([]() { return LIGHT1_TEXT_COLOR; });
	search_box_view->set_on_view_released([](View &) {
		if (Search_show_search_keyboard()) {
			global_intent.next_scene = SceneType::SEARCH;
		}
	});

	url_button_view = (new TextView(0, SEARCH_BOX_MARGIN, URL_BUTTON_WIDTH, SEARCH_ROW_HEIGHT - SEARCH_BOX_MARGIN * 2 - 1));
	url_button_view->set_text_offset(0, -1);
	url_button_view->set_x_alignment(TextView::XAlign::CENTER);
	url_button_view->set_text((std::function<std::string()>)[]() { return LOCALIZED(URL); });
	url_button_view->set_get_background_color([](const View &) { return LIGHT1_BACK_COLOR; });
	url_button_view->set_on_view_released([](View &) {
		if (Search_show_url_input_keyboard()) {
			global_intent.next_scene = SceneType::SEARCH;
		}
	});

	toast_view->set_is_visible(false);
	toast_view->set_x_alignment(TextView::XAlign::CENTER);
	toast_view->set_text_offset(0, -1);
	toast_view->set_get_background_color([](const View &) { return 0x50000000; });
	toast_view->set_get_text_color([]() { return (u32)-1; });

	top_bar_view->set_views({(new HorizontalListView(0, 0, SEARCH_ROW_HEIGHT - 1))
	                             ->set_views({new EmptyView(0, 0, SEARCH_BOX_MARGIN, SEARCH_ROW_HEIGHT - 1), search_box_view,
	                                          new EmptyView(0, 0, SEARCH_BOX_MARGIN, SEARCH_ROW_HEIGHT - 1), url_button_view,
	                                          new EmptyView(0, 0, SEARCH_BOX_MARGIN, SEARCH_ROW_HEIGHT - 1)})
	                             ->set_get_background_color([](const View &) { return DEFAULT_BACK_COLOR; }),
	                         (new RuleView(0, 0, 320, 1))
	                             ->set_get_color([]() { return 0xFF000000; }) // always black
	                             ->set_margin(0),
	                         (new TextView(0, 0, 320, FILTER_ROW_HEIGHT - 1))
	                             ->set_text((std::function<std::string()>)[]() {
		                             return std::string(tr("Filter: ", "Filtro: ")) + search_filter_name(search_filter_index);
	                             })
	                             ->set_font_size(0.45, DEFAULT_FONT_INTERVAL)
	                             ->set_x_alignment(TextView::XAlign::CENTER)
	                             ->set_y_alignment(TextView::YAlign::CENTER)
	                             ->set_get_background_color([](const View &) { return LIGHT0_BACK_COLOR; })
	                             ->set_on_view_released([](View &) {
		                             search_filter_index = (search_filter_index + 1) % (sizeof(search_filters) / sizeof(search_filters[0]));
		                             youtube_search_filter_params = search_filters[search_filter_index].params;
		                             if (cur_search_word != "" && !is_async_task_running(load_search_results)) {
			                             start_search(cur_search_word); // search again with the new filter
		                             }
		                             var_need_refresh = true;
	                             }),
	                         (new RuleView(0, 0, 320, 1))
	                             ->set_get_color([]() { return 0xFF000000; })
	                             ->set_margin(0)});

	result_view =
	    (new ScrollView(0, 0, 320, RESULT_Y_HIGH - RESULT_Y_LOW))->set_views({result_list_view, result_bottom_view});
	main_view = (new VerticalListView(0, 0, 320))->set_views({top_bar_view, result_view})->set_draw_order({1, 0});

	Search_resume("");
	already_init = true;
}

void Search_exit(void) {
	already_init = false;
	thread_suspend = false;
	exiting = true;

	resource_lock.lock();

	main_view->recursive_delete_subviews();
	delete main_view;
	main_view = NULL;
	result_view = NULL;
	result_list_view = NULL;
	result_bottom_view = NULL;
	top_bar_view = NULL;
	search_box_view = NULL;
	url_button_view = NULL;

	toast_view->recursive_delete_subviews();
	delete toast_view;
	toast_view = NULL;

	resource_lock.unlock();

	logger.info("search/exit", "Exited.");
}

void Search_suspend(void) { thread_suspend = true; }

void Search_resume(std::string arg) {
	(void)arg;
	overlay_menu_on_resume();
	resource_lock.lock();
	show_search_history();
	resource_lock.unlock();

	thread_suspend = false;
	var_need_refresh = true;
}

// async functions
static void set_loading_bottom_view() {
	delete result_bottom_view;
	result_bottom_view = (new TextView(0, 0, 320, 30))
	                         ->set_text((std::function<std::string()>)[]() { return loading_text(); })
	                         ->set_x_alignment(TextView::XAlign::CENTER);
	result_view->views[1] = result_bottom_view;
}
static void update_result_bottom_view() {
	delete result_bottom_view;
	if (search_result.error != "" || search_result.has_more_results()) {
		TextView *cur_view =
		    (new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL * 2))->set_x_alignment(TextView::XAlign::CENTER);
		if (search_result.error != "") {
			cur_view->set_text(search_result.error);
		} else {
			cur_view->set_text((std::function<std::string()>)[]() { return loading_text(); });
		}
		result_bottom_view = cur_view;
	} else {
		result_bottom_view = new EmptyView(0, 0, 320, 0);
	}
	result_view->views[1] = result_bottom_view;
	result_view->set_on_child_drawn(1, [](const ScrollView &, int) {
		if (search_result.has_more_results() && search_result.error == "") {
			if (!is_async_task_running(load_search_results) && !is_async_task_running(load_more_search_results)) {
				queue_async_task(load_more_search_results, NULL);
			}
		}
	});
}
static View *result_item_to_view(const YouTubeSuccinctItem &item) {
	View *res_view;
	if (item.type == YouTubeSuccinctItem::CHANNEL) {
		SuccinctChannelView *cur_view = new SuccinctChannelView(0, 0, 320, VIDEO_LIST_THUMBNAIL_HEIGHT);
		cur_view->set_name(item.channel.name);
		cur_view->set_auxiliary_lines({item.channel.subscribers, item.channel.video_num});
		cur_view->set_thumbnail_url(item.channel.icon_url);
		res_view = cur_view;
	} else {
		const bool grid = var_grid_view;
		const int cell_w = 158;
		SuccinctVideoView *cur_view = grid ? new SuccinctVideoView(0, 0, cell_w, cell_w * 9 / 16 + (DEFAULT_FONT_INTERVAL - 1) * 2 + 3)
		                                   : new SuccinctVideoView(0, 0, 320, VIDEO_LIST_THUMBNAIL_HEIGHT);
		cur_view->set_grid(grid);
		cur_view->set_title_lines(grid ? truncate_str(item.get_name(), cell_w - 4, 2, 0.45, 0.45)
		                               : truncate_str(item.get_name(), 320 - (VIDEO_LIST_THUMBNAIL_WIDTH + 3), 2, 0.5, 0.5));
		if (item.type == YouTubeSuccinctItem::VIDEO) {
			cur_view->set_auxiliary_lines({item.video.views_str, item.video.publish_date});
			cur_view->set_bottom_right_overlay(item.video.duration_text);
			cur_view->set_queue_item(item.get_url(), item.get_name());
		} else if (item.type == YouTubeSuccinctItem::PLAYLIST) {
			cur_view->set_auxiliary_lines({item.playlist.video_count_str});
		}
		cur_view->set_thumbnail_url(item.get_thumbnail_url());
		cur_view->set_is_playlist(item.type == YouTubeSuccinctItem::PLAYLIST);
		res_view = cur_view;
	}
	res_view->set_get_background_color(View::STANDARD_BACKGROUND);
	res_view->set_on_view_released([item](View &view) {
		clicked_url = item.get_url();
		clicked_is_channel = item.type == YouTubeSuccinctItem::CHANNEL;
	});
	return res_view;
}
static std::pair<int *, std::string *> get_thumbnail_info_from_view(View *view) {
	SuccinctVideoView *cur_view0 = dynamic_cast<SuccinctVideoView *>(view);
	if (cur_view0) {
		return {&cur_view0->thumbnail_handle, &cur_view0->thumbnail_url};
	}
	SuccinctChannelView *cur_view1 = dynamic_cast<SuccinctChannelView *>(view);
	if (cur_view1) {
		return {&cur_view1->thumbnail_handle, &cur_view1->thumbnail_url};
	}
	usleep(1000000);
	return {NULL, NULL};
}
static void load_account_feed(void *) {
	std::vector<YouTubeVideoSuccinct> videos;
	bool ok = account_feed_kind == 1 ? youtube_account_recommendations(videos) : youtube_account_notifications(videos);
	std::vector<View *> views;
	for (auto &video : videos) {
		views.push_back(result_item_to_view(YouTubeSuccinctItem(video)));
	}
	resource_lock.lock();
	if (exiting) {
		resource_lock.unlock();
		return;
	}
	if (views.size()) {
		result_list_view->recursive_delete_subviews();
		result_list_view->views = views;
		search_done = true; // the history list must not be rebuilt on top of it
	}
	var_need_refresh = true;
	resource_lock.unlock();
	if (!ok) {
		notice_show(tr("Could not load it from your account", "No se pudo cargar de tu cuenta"));
	} else if (videos.empty()) {
		notice_show(tr("No videos found", "No se encontraron vídeos"));
	}
}

static void load_search_results(void *) {
	// pre-access processing
	resource_lock.lock();
	std::string search_word = cur_search_word;
	bool feed = shorts_feed;
	search_done = false;

	result_list_view->recursive_delete_subviews();
	set_loading_bottom_view();
	search_result = YouTubeSearchResult();
	var_need_refresh = true;
	resource_lock.unlock();

	// actual loading
	std::string search_url = "https://m.youtube.com/results?search_query=";
	for (auto c : search_word) {
		search_url.push_back('%');
		search_url.push_back("0123456789ABCDEF"[(u8)c / 16]);
		search_url.push_back("0123456789ABCDEF"[(u8)c % 16]);
	}
	add_cpu_limit(ADDITIONAL_CPU_LIMIT);
	YouTubeSearchResult new_result = youtube_load_search(search_url);
	remove_cpu_limit(ADDITIONAL_CPU_LIMIT);

	// the Shorts / watched videos that the settings hide
	if (!feed) {
		new_result.results.erase(std::remove_if(new_result.results.begin(), new_result.results.end(),
		                                        [](const YouTubeSuccinctItem &item) { return item.type == YouTubeSuccinctItem::VIDEO && result_video_hidden(item.video.url); }),
		                         new_result.results.end());
	} else { // Shorts feed : play them all in a shuffled loop once you open one
		std::vector<std::pair<std::string, std::string>> items;
		for (auto &item : new_result.results) {
			if (item.type == YouTubeSuccinctItem::VIDEO) items.push_back({item.video.url, item.video.title});
		}
		play_queue_set_repeat(items, true);
	}

	// wrap and truncate here
	logger.info("search", "truncate/view creation start");
	std::vector<View *> new_result_views;
	for (size_t i = 0; i < new_result.results.size(); i++) {
		new_result_views.push_back(result_item_to_view(new_result.results[i]));
	}
	logger.info("search", "truncate/view creation end");

	resource_lock.lock();
	if (exiting) { // app shut down while loading
		resource_lock.unlock();
		return;
	}
	search_result = new_result;
	result_list_view->views = new_result_views;
	update_result_bottom_view();

	search_done = true;
	var_need_refresh = true;
	resource_lock.unlock();
}
static void load_more_search_results(void *) {
	auto new_result = search_result;
	new_result.load_more_results();
	{ // hide what the settings say, only in the new part
		size_t old_size = std::min(search_result.results.size(), new_result.results.size());
		new_result.results.erase(std::remove_if(new_result.results.begin() + old_size, new_result.results.end(),
		                                        [](const YouTubeSuccinctItem &item) { return item.type == YouTubeSuccinctItem::VIDEO && result_video_hidden(item.video.url); }),
		                         new_result.results.end());
	}

	logger.info("search-c", "truncate/view creation start");
	std::vector<View *> new_result_views;
	for (size_t i = search_result.results.size(); i < new_result.results.size(); i++) {
		new_result_views.push_back(result_item_to_view(new_result.results[i]));
	}
	logger.info("search-c", "truncate/view creation end");

	resource_lock.lock();
	if (exiting) { // app shut down while loading
		resource_lock.unlock();
		return;
	}
	search_result = new_result;
	if (new_result.error == "") {
		result_list_view->views.insert(result_list_view->views.end(), new_result_views.begin(), new_result_views.end());
	}
	update_result_bottom_view();
	var_need_refresh = true;
	resource_lock.unlock();
}
static void access_input_url(void *) {
	resource_lock.lock();
	auto url = last_url_input;
	resource_lock.unlock();

	YouTubePageType page_type = youtube_get_page_type(url);

	if (page_type == YouTubePageType::INVALID) {
		static NetworkSessionList session_list;
		if (!session_list.inited) {
			session_list.init();
		}

		auto result = session_list.perform(HttpRequest::GET(url, {}));
		page_type = youtube_get_page_type(result.redirected_url);
		url = result.redirected_url;
	}

	resource_lock.lock();
	if (exiting) { // app shut down while loading
		resource_lock.unlock();
		return;
	}
	if (page_type != YouTubePageType::INVALID) {
		if (page_type == YouTubePageType::VIDEO) {
			global_intent.next_scene = SceneType::VIDEO_PLAYER;
		} else if (page_type == YouTubePageType::CHANNEL) {
			global_intent.next_scene = SceneType::CHANNEL;
		} else if (page_type == YouTubePageType::SEARCH) {
			global_intent.next_scene = SceneType::SEARCH;
		}
		global_intent.arg = url;
	} else {
		toast_view->set_text((std::function<std::string()>)[]() { return LOCALIZED(NOT_A_YOUTUBE_URL); });
		toast_view->set_is_visible(true);
		toast_view_visible_frames_left = 100;
	}
	resource_lock.unlock();
}

bool Search_show_search_keyboard() {
	if (!is_async_task_running(load_search_results)) {
		if (global_current_scene != SceneType::SEARCH) {
			resource_lock.lock();
		}
		SwkbdState keyboard;
		swkbdInit(&keyboard, SWKBD_TYPE_NORMAL, 2, 256);
		swkbdSetFeatures(&keyboard, SWKBD_DEFAULT_QWERTY | SWKBD_PREDICTIVE_INPUT);
		swkbdSetValidation(&keyboard, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
		swkbdSetButton(&keyboard, SWKBD_BUTTON_LEFT, LOCALIZED(CANCEL).c_str(), false);
		swkbdSetButton(&keyboard, SWKBD_BUTTON_RIGHT, LOCALIZED(OK).c_str(), true);
		swkbdSetInitialText(&keyboard, cur_search_word.c_str());
		// Suggestions while typing : the keyboard dictionary holds the search history and some common terms.
		static SwkbdDictWord dict_words[96];
		int dict_count = 0;
		auto add_dict_word = [&](const std::string &w) {
			if (w.empty() || w.size() > 38 || dict_count >= 96) {
				return;
			}
			swkbdSetDictWord(&dict_words[dict_count++], w.c_str(), w.c_str());
		};
		for (auto &h : search_history_get()) {
			add_dict_word(h);
		}
		static const char *common_terms[] = {"music",       "official video", "lyrics",  "live",     "remix",
		                                     "trailer",     "gameplay",       "walkthrough", "playlist", "podcast",
		                                     "nintendo 3ds", "tutorial",       "news",    "full album", "soundtrack"};
		for (auto t : common_terms) {
			add_dict_word(t);
		}
		swkbdSetDictionary(&keyboard, dict_words, dict_count);
		char search_word[257];
		add_cpu_limit(40);
		video_set_skip_drawing(true);
		auto button_pressed = swkbdInputText(&keyboard, search_word, 256);
		video_set_skip_drawing(false);
		remove_cpu_limit(40);

		if (button_pressed == SWKBD_BUTTON_RIGHT) {
			cur_search_word = search_word;
			search_history_add(search_word);
			// Truncate text to fit in search box and display
			double search_box_width = 320 - SEARCH_BOX_MARGIN * 3 - URL_BUTTON_WIDTH;
			std::string display_text = truncate_search_text_for_display(search_word, search_box_width, 0.5);
			search_box_view->set_text(display_text);
			search_box_view->set_get_text_color([]() { return DEFAULT_TEXT_COLOR; });

			remove_all_async_tasks_with_type(load_search_results);
			remove_all_async_tasks_with_type(load_more_search_results);
			queue_async_task(load_search_results, NULL);

			global_intent.next_scene = SceneType::SEARCH;
		}
		if (global_current_scene != SceneType::SEARCH) {
			resource_lock.unlock();
		}
		return button_pressed == SWKBD_BUTTON_RIGHT;
	} else {
		return false;
	}
}
bool Search_show_url_input_keyboard() {
	if (!is_async_task_running(access_input_url)) {
		if (global_current_scene != SceneType::SEARCH) {
			resource_lock.lock();
		}
		SwkbdState keyboard;
		swkbdInit(&keyboard, SWKBD_TYPE_NORMAL, 2, 255);
		swkbdSetFeatures(&keyboard, SWKBD_DEFAULT_QWERTY | SWKBD_PREDICTIVE_INPUT);
		swkbdSetValidation(&keyboard, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
		swkbdSetButton(&keyboard, SWKBD_BUTTON_LEFT, LOCALIZED(CANCEL).c_str(), false);
		swkbdSetButton(&keyboard, SWKBD_BUTTON_RIGHT, LOCALIZED(OK).c_str(), true);
		swkbdSetInitialText(&keyboard, last_url_input.c_str());
		char url[256];
		add_cpu_limit(40);
		video_set_skip_drawing(true);
		auto button_pressed = swkbdInputText(&keyboard, url, 256 - 1);
		video_set_skip_drawing(false);
		remove_cpu_limit(40);

		if (button_pressed == SWKBD_BUTTON_RIGHT) {
			last_url_input = url;

			remove_all_async_tasks_with_type(access_input_url);
			queue_async_task(access_input_url, NULL);
		}
		if (global_current_scene != SceneType::SEARCH) {
			resource_lock.unlock();
		}
		return button_pressed == SWKBD_BUTTON_RIGHT;
	} else {
		return false;
	}
}

void Search_draw(void) {
	Hid_info key;
	Util_hid_query_key_state(&key);

	thumbnail_set_active_scene(SceneType::SEARCH);

	bool video_playing_bar_show = video_is_playing();
	{
		int nav_bottom = video_playing_bar_show ? 240 - VIDEO_PLAYING_BAR_HEIGHT : 240;
		overlay_set_nav(true, nav_bottom);
		RESULT_Y_HIGH = nav_bottom - NAV_BAR_HEIGHT;
	}
	result_view->update_y_range(0, RESULT_Y_HIGH - RESULT_Y_LOW);

	if (toast_view_visible_frames_left > 0) {
		toast_view_visible_frames_left--;
		if (!toast_view_visible_frames_left) {
			toast_view->set_is_visible(false);
		}
	}
	if (var_need_refresh || !var_eco_mode) {
		var_need_refresh = false;
		Draw_frame_ready();
		video_draw_top_screen();

		Draw_screen_ready(2, DEFAULT_BACK_COLOR);

		// (!) : I don't know how to draw textures truncated, so I will just fill the margin with white again
		resource_lock.lock();
		main_view->draw();
		if (toast_view_visible_frames_left > 0) {
			toast_view->draw();
		}
		resource_lock.unlock();

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

		notice_draw(RESULT_Y_HIGH);
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
		update_overlay_menu(&key);

		resource_lock.lock();
		main_view->update(key);
		if (history_clear_request) {
			history_clear_request = false;
			search_history_clear();
			result_list_view->recursive_delete_subviews();
			var_need_refresh = true;
		}
		if (history_remove_request != "") {
			search_history_remove(history_remove_request);
			history_remove_request = "";
			result_list_view->recursive_delete_subviews(); // rebuilt without it
			var_need_refresh = true;
		}
		if (clicked_url != "") {
			global_intent.next_scene = clicked_is_channel ? SceneType::CHANNEL : SceneType::VIDEO_PLAYER;
			global_intent.arg = clicked_url;
			clicked_url = "";
		}
		resource_lock.unlock();

		if (qr_scan_pending) {
			qr_scan_pending = false;
			std::string scanned;
			video_set_skip_drawing(true);
			bool ok = qr_scan_run(scanned);
			video_set_skip_drawing(false);
			if (ok && !is_async_task_running(access_input_url)) {
				last_url_input = scanned;
				remove_all_async_tasks_with_type(access_input_url);
				queue_async_task(access_input_url, NULL);
			}
		}

		if (video_playing_bar_show) {
			video_update_playing_bar(key);
		}

		if (key.p_a) {
			Search_show_search_keyboard();
		}

		if (key.p_b) {
			global_intent.next_scene = SceneType::BACK;
		}
	}
}
