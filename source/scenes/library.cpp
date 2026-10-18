#include "headers.hpp"
#include "ui/qr_popup.hpp"
#include "data_io/watch_later.hpp"
#include "data_io/offline.hpp"
#include "scenes/offline_tasks.hpp"
#include <sys/stat.h>
#include "oauth/oauth.hpp"
#include "util/async_task.hpp"
#include "ui/notice.hpp"
#include "youtube_parser/account.hpp"
#include <vector>
#include <string>
#include <cstdlib>

#include "scenes/library.hpp"
#include "scenes/video_player.hpp"
#include "youtube_parser/parser.hpp"
#include "ui/ui.hpp"
#include "ui/overlay.hpp"
#include "network_decoder/thumbnail_loader.hpp"
#include "data_io/favorites.hpp"
#include "data_io/user_playlists.hpp"
#include "util/play_queue.hpp"

#define MAX_THUMBNAIL_LOAD_REQUEST 12

// Biblioteca : the play queue (reorder / clear), the favorites and the named playlists
namespace Library {
bool thread_suspend = false;
bool already_init = false;
bool exiting = false;

int mode = 0;           // 0 : queue, 1 : favorites, 2 : playlists, 3 : watch later, 4 : offline audio
int open_playlist = -1; // playlists mode : index of the opened playlist (-1 : list of playlists)
bool playlist_repeat = false;
bool rebuild_request = false;
bool new_playlist_request = false;
std::string clicked_url;
std::function<void()> pending_action; // runs after the update (e.g. opening a second dialog)

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
	return (new TextView(0, 0, 200, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))
	    ->set_text(text)
	    ->set_x_alignment(TextView::XAlign::CENTER)
	    ->set_y_alignment(TextView::YAlign::CENTER)
	    ->set_text_offset(0, -1)
	    ->set_on_view_released([action](View &) {
		    main_view->reset_holding_status();
		    on_long_tap_dialog->set_is_visible(false);
		    action();
		    rebuild_request = true;
		    var_need_refresh = true;
	    })
	    ->set_get_background_color(button_background);
}

static void show_dialog(const std::vector<View *> &options) {
	on_long_tap_dialog->recursive_delete_subviews();
	on_long_tap_dialog
	    ->set_subview((new VerticalListView(0, 0, 200))->set_views(options))
	    ->set_on_cancel([](OverlayView &view) {
		    main_view->reset_holding_status();
		    view.set_is_visible(false);
		    var_need_refresh = true;
	    })
	    ->set_is_visible(true);
	var_need_refresh = true;
}

// asks a name with the system keyboard. Returns false if the user cancelled.
static bool ask_text(const char *hint, std::string &out) {
	SwkbdState keyboard;
	swkbdInit(&keyboard, SWKBD_TYPE_NORMAL, 2, 40);
	swkbdSetFeatures(&keyboard, SWKBD_DEFAULT_QWERTY);
	swkbdSetValidation(&keyboard, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
	swkbdSetHintText(&keyboard, hint);
	swkbdSetButton(&keyboard, SWKBD_BUTTON_LEFT, LOCALIZED(CANCEL).c_str(), false);
	swkbdSetButton(&keyboard, SWKBD_BUTTON_RIGHT, LOCALIZED(OK).c_str(), true);
	char text[64] = {0};
	video_set_skip_drawing(true);
	auto pressed = swkbdInputText(&keyboard, text, sizeof text);
	video_set_skip_drawing(false);
	if (pressed != SWKBD_BUTTON_RIGHT) {
		return false;
	}
	out = text;
	return true;
}

// dialog : choose the playlist where the video goes (or create one)
static void add_to_playlist_dialog(const FavoriteVideo &video) {
	std::vector<View *> options;
	auto lists = playlists_get();
	for (size_t i = 0; i < lists.size(); i++) {
		options.push_back(make_option(lists[i].name, [i, video]() { playlist_add(i, video); }));
	}
	options.push_back(make_option(tr("+ New playlist", "+ Nueva lista"), [video]() {
		std::string name;
		if (ask_text(tr("Playlist name", "Nombre de la lista").c_str(), name) && playlist_create(name)) {
			auto all = playlists_get();
			for (size_t i = 0; i < all.size(); i++) {
				if (all[i].name == name) {
					playlist_add(i, video);
				}
			}
		}
	}));
	show_dialog(options);
}

static std::string share_url(const std::string &url_or_id) {
	return url_or_id.find("://") != std::string::npos ? url_or_id : "https://www.youtube.com/watch?v=" + url_or_id;
}

static FavoriteVideo video_from_url(const std::string &url, const std::string &title) {
	FavoriteVideo v;
	v.id = youtube_get_video_id_by_url(url);
	v.title = title;
	return v;
}

// sort and filter of the favorites / watch later lists (the filter matches title, channel or tag)
static int list_sort = 0; // 0 newest first, 1 title A-Z, 2 channel
static std::string list_filter;

static std::string lower_copy(std::string s) {
	for (auto &c : s) c = tolower((unsigned char)c);
	return s;
}
static std::vector<FavoriteVideo> filter_and_sort(std::vector<FavoriteVideo> videos) {
	if (list_filter != "") {
		std::string needle = lower_copy(list_filter);
		std::vector<FavoriteVideo> kept;
		for (auto &v : videos) {
			if (lower_copy(v.title).find(needle) != std::string::npos || lower_copy(v.author).find(needle) != std::string::npos ||
			    lower_copy(v.tag).find(needle) != std::string::npos) {
				kept.push_back(v);
			}
		}
		videos = kept;
	}
	if (list_sort == 1) {
		std::stable_sort(videos.begin(), videos.end(), [](const FavoriteVideo &a, const FavoriteVideo &b) { return lower_copy(a.title) < lower_copy(b.title); });
	} else if (list_sort == 2) {
		std::stable_sort(videos.begin(), videos.end(), [](const FavoriteVideo &a, const FavoriteVideo &b) { return lower_copy(a.author) < lower_copy(b.author); });
	}
	return videos;
}

static SuccinctVideoView *make_item(const std::string &id, const std::string &title, const std::string &line2,
                                    const std::string &length) {
	return (new SuccinctVideoView(0, 0, 320, VIDEO_LIST_THUMBNAIL_HEIGHT))
	    ->set_title_lines(truncate_str(title, 320 - (VIDEO_LIST_THUMBNAIL_WIDTH + 3), 2, 0.5, 0.5))
	    ->set_auxiliary_lines({line2})
	    ->set_bottom_right_overlay(length)
	    ->set_thumbnail_url(youtube_get_video_thumbnail_url_by_id(id));
}

static TextView *make_button(const std::string &text, int width, std::function<void()> action) {
	TextView *button = (new TextView(0, 0, width, 22));
	button->set_text(text)
	    ->set_x_alignment(TextView::XAlign::CENTER)
	    ->set_y_alignment(TextView::YAlign::CENTER)
	    ->set_get_background_color(button_background)
	    ->set_corner_radius(6)
	    ->set_on_view_released([action](View &) { action(); });
	return button;
}

// first row of the favorites / watch later lists : sort mode and text filter
static void add_sort_filter_row(VerticalListView *list) {
	static const char *sort_en[] = {"Newest", "Title A-Z", "Channel"};
	static const char *sort_es[] = {"Recientes", "Título A-Z", "Canal"};
	list->views.push_back((new HorizontalListView(0, 0, 22))
	                          ->set_views({make_button(tr("Sort: ", "Orden: ") + tr(sort_en[list_sort], sort_es[list_sort]), 160,
	                                                   []() {
		                                                   list_sort = (list_sort + 1) % 3;
		                                                   rebuild_request = true;
	                                                   }),
	                                       make_button(list_filter == "" ? tr("Filter: all", "Filtro: todo") : tr("Filter: ", "Filtro: ") + list_filter, 160,
	                                                   []() {
		                                                   if (list_filter != "") { // tapping an active filter clears it
			                                                   list_filter = "";
			                                                   rebuild_request = true;
			                                                   return;
		                                                   }
		                                                   pending_action = []() {
			                                                   std::string text = list_filter;
			                                                   if (ask_text(tr("Filter (title, channel or tag)", "Filtro (título, canal o etiqueta)").c_str(), text)) {
				                                                   list_filter = text;
				                                                   rebuild_request = true;
			                                                   }
		                                                   };
	                                                   })}));
}

// plays a playlist : the first video now, the others through the queue
static void start_playlist(const UserPlaylist &playlist, bool shuffle) {
	std::vector<std::pair<std::string, std::string>> items;
	for (auto &v : playlist.videos) {
		items.push_back({youtube_get_video_url_by_id(v.id), v.title});
	}
	if (!items.size()) {
		return;
	}
	if (shuffle) {
		for (size_t i = items.size(); i > 1; i--) {
			std::swap(items[i - 1], items[rand() % i]);
		}
	}
	play_queue_clear();
	for (size_t i = 1; i < items.size(); i++) {
		play_queue_toggle(items[i].first, items[i].second);
	}
	if (playlist_repeat) {
		play_queue_set_repeat(items, shuffle);
	}
	clicked_url = items[0].first;
}

void Library_init(void) {
	logger.info("library/init", "Initializing...");
	srand((unsigned)osGetTime());
	on_long_tap_dialog = new OverlayView(0, 0, 320, 240);
	on_long_tap_dialog->set_is_visible(false);
	favorites_load();
	watch_later_load();
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
bool Library_is_offline_mode(void) { return mode == 4; }
void Library_resume(std::string arg) {
	if (arg == "favorites") {
		mode = 1, open_playlist = -1;
	} else if (arg == "queue") {
		mode = 0;
	} else if (arg == "playlists") {
		mode = 2, open_playlist = -1;
	} else if (arg == "offline") {
		mode = 4, open_playlist = -1;
	} else if (arg == "later") {
		mode = 3, open_playlist = -1;
	}
	if (main_view) {
		main_view->on_resume();
	}
	overlay_menu_on_resume();
	thread_suspend = false;
	var_need_refresh = true;
	rebuild_views();
}

// the queue as a private playlist of the YouTube account
struct QueueSaveJob {
	std::string name;
	std::vector<std::string> ids;
};
static void save_queue_to_account_task(void *arg) {
	QueueSaveJob *job = (QueueSaveJob *)arg;
	std::string playlist_id;
	if (!youtube_account_playlist_create(job->name, "PRIVATE", playlist_id) || playlist_id == "") {
		notice_show(tr("Could not create the playlist", "No se pudo crear la lista") + std::string(" (") + youtube_account_last_error().substr(0, 50) + ")");
		delete job;
		return;
	}
	int added = 0;
	for (auto &id : job->ids) {
		if (youtube_account_playlist_edit(playlist_id, id, true)) added++;
	}
	notice_show(tr("Saved to YouTube: ", "Guardada en YouTube: ") + std::to_string(added) + "/" + std::to_string(job->ids.size()));
	delete job;
}

static void build_queue(VerticalListView *list) {
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
		    ->add_on_long_hold(35, [i, url, title](View &) {
			    show_dialog({make_option(tr("QR code", "Código QR"), [url]() { qr_popup_show(share_url(url)); }),
			                 make_option(tr("Play next", "Reproducir a continuación"), [url, title]() { play_queue_play_next(url, title); }),
			                 make_option(tr("Save video offline", "Guardar vídeo sin conexión"), [url, title]() { offline_save_by_id_async(youtube_get_video_id_by_url(url), title, "", true); }),
			                 make_option(tr("Save audio offline", "Guardar audio sin conexión"), [url, title]() { offline_save_by_id_async(youtube_get_video_id_by_url(url), title, "", false); }),
			                 make_option(tr("Move up", "Subir"), [i]() { play_queue_move(i, -1); }),
			                 make_option(tr("Move down", "Bajar"), [i]() { play_queue_move(i, 1); }),
			                 make_option(tr("Add to a list...", "Añadir a una lista..."), [url, title]() { pending_action = [url, title]() { add_to_playlist_dialog(video_from_url(url, title)); }; }),
			                 make_option(tr("Remove from queue", "Quitar de la cola"), [i]() { play_queue_remove_at(i); })});
		    });
		list->views.push_back(view);
	}
	if (!n) {
		list->views.push_back((new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL * 4))
		                          ->set_text_lines(std::vector<std::string>{tr("The queue is empty.", "La cola está vacía."), tr("Long press any video", "Mantén pulsado cualquier vídeo"), tr("to add it.", "para añadirlo.")})
		                          ->set_x_alignment(TextView::XAlign::CENTER)
		                          ->set_font_size(0.5, DEFAULT_FONT_INTERVAL));
	} else {
		list->views.insert(list->views.begin(), make_button(tr("Clear queue", "Vaciar cola"), 320, []() {
			                   play_queue_clear();
			                   rebuild_request = true;
		                   }));
		if (OAuth::is_authenticated()) {
			list->views.insert(list->views.begin(), make_button(tr("Save the queue to my YouTube", "Guardar la cola en mi YouTube"), 320, []() {
				                   std::string name;
				                   if (!ask_text(tr("Playlist name", "Nombre de la lista").c_str(), name) || name == "") return;
				                   QueueSaveJob *job = new QueueSaveJob();
				                   job->name = name;
				                   for (int i = 0; i < play_queue_size() && i < 40; i++) {
					                   std::string url, title;
					                   if (play_queue_get(i, url, title)) job->ids.push_back(youtube_get_video_id_by_url(url));
				                   }
				                   notice_show(tr("Saving the queue...", "Guardando la cola..."));
				                   queue_async_task(save_queue_to_account_task, job);
			                   }));
		}
	}
}

static void build_favorites(VerticalListView *list) {
	auto favs = filter_and_sort(favorites_get());
	add_sort_filter_row(list);
	for (auto &v : favs) {
		SuccinctVideoView *view = make_item(v.id, v.title, (v.tag != "" ? "[" + v.tag + "] " : std::string()) + v.author, v.length_text);
		std::string id = v.id, url = youtube_get_video_url_by_id(v.id);
		view->set_get_background_color(button_background)
		    ->set_on_view_released([url](View &) { clicked_url = url; })
		    ->add_on_long_hold(35, [id, url, v](View &) {
			    show_dialog({make_option(tr("Play next", "Reproducir a continuación"), [url, v]() { play_queue_play_next(url, v.title); }),
			                 make_option(tr("Add to queue", "Añadir a la cola"), [url, v]() { play_queue_toggle(url, v.title); }),
			                 make_option(tr("Add to a list...", "Añadir a una lista..."), [v]() { pending_action = [v]() { add_to_playlist_dialog(v); }; }),
			                 make_option(tr("Save video offline", "Guardar vídeo sin conexión"), [id, v]() { offline_save_by_id_async(id, v.title, "", true); }),
			                 make_option(tr("Save audio offline", "Guardar audio sin conexión"), [id, v]() { offline_save_by_id_async(id, v.title, "", false); }),
			                 make_option(tr("QR code", "Código QR"), [id]() { qr_popup_show(share_url(id)); }),
			                 make_option(tr("Set tag...", "Poner etiqueta..."), [id]() { pending_action = [id]() { std::string tag; if (ask_text(tr("Tag", "Etiqueta").c_str(), tag)) { favorites_set_tag(id, tag); rebuild_request = true; } }; }),
			                 make_option(tr("Remove tag", "Quitar etiqueta"), [id]() { favorites_set_tag(id, ""); rebuild_request = true; }),
			                 make_option(tr("Remove from favorites", "Quitar de favoritos"), [id]() { favorites_remove(id); })});
		    });
		list->views.push_back(view);
	}
	if (!favs.size()) {
		list->views.push_back((new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL * 4))
		                          ->set_text_lines(std::vector<std::string>{tr("You have no favorites yet.", "Aún no tienes favoritos."), tr("Use the Favorite button inside", "Usa el botón Favorito dentro"), tr("a video.", "de un vídeo.")})
		                          ->set_x_alignment(TextView::XAlign::CENTER)
		                          ->set_font_size(0.5, DEFAULT_FONT_INTERVAL));
	}
}

static void pull_watch_later_task(void *) {
	if (watch_later_pull_remote() > 0) {
		rebuild_request = true;
		var_need_refresh = true;
	}
}

// ---- liked videos of the account (read from YouTube) ----
static Mutex resource_lock_lib;
static std::vector<FavoriteVideo> liked_videos;
static bool liked_loaded = false, liked_failed = false, liked_loading = false;
static bool show_yt_history = false; // the same tab shows the YouTube history instead of the liked videos
static void load_liked_task(void *) {
	std::vector<FavoriteVideo> videos;
	bool ok = account_playlist_videos(show_yt_history ? "FEhistory" : "VLLL", videos);
	resource_lock_lib.lock();
	liked_videos = videos;
	liked_loaded = ok;
	liked_failed = !ok;
	liked_loading = false;
	resource_lock_lib.unlock();
	rebuild_request = true;
	var_need_refresh = true;
}

static void build_liked(VerticalListView *list) {
	if (!OAuth::is_authenticated()) {
		list->views.push_back((new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL * 3))
		                          ->set_text_lines(std::vector<std::string>{tr("Sign in to see the videos", "Inicia sesión para ver los vídeos"), tr("you liked on YouTube.", "que te gustan en YouTube.")})
		                          ->set_x_alignment(TextView::XAlign::CENTER)
		                          ->set_font_size(0.5, DEFAULT_FONT_INTERVAL));
		return;
	}
	resource_lock_lib.lock();
	if (!liked_loaded && !liked_failed && !liked_loading) {
		liked_loading = true;
		queue_async_task(load_liked_task, NULL);
	}
	std::vector<FavoriteVideo> videos = filter_and_sort(liked_videos);
	bool loading = liked_loading, failed = liked_failed;
	resource_lock_lib.unlock();
	list->views.push_back(make_button(show_yt_history ? tr("Showing: YouTube history (tap for liked)", "Viendo: historial de YouTube (toca para Me gusta)")
	                                                  : tr("Showing: liked (tap for YouTube history)", "Viendo: Me gusta (toca para el historial de YouTube)"),
	                                  320, []() {
		                                  show_yt_history = !show_yt_history;
		                                  resource_lock_lib.lock();
		                                  liked_videos.clear();
		                                  liked_loaded = liked_failed = liked_loading = false;
		                                  resource_lock_lib.unlock();
		                                  rebuild_request = true;
	                                  }));
	add_sort_filter_row(list);
	for (auto &v : videos) {
		SuccinctVideoView *view = make_item(v.id, v.title, v.author, v.length_text);
		std::string id = v.id, url = youtube_get_video_url_by_id(v.id);
		view->set_get_background_color(button_background)
		    ->set_on_view_released([url](View &) { clicked_url = url; })
		    ->add_on_long_hold(35, [id, url, v](View &) {
			    show_dialog({make_option(tr("Play next", "Reproducir a continuación"), [url, v]() { play_queue_play_next(url, v.title); }),
			                 make_option(tr("Add to queue", "Añadir a la cola"), [url, v]() { play_queue_toggle(url, v.title); }),
			                 make_option(tr("Save video offline", "Guardar vídeo sin conexión"), [id, v]() { offline_save_by_id_async(id, v.title, "", true); }),
			                 make_option(tr("Save audio offline", "Guardar audio sin conexión"), [id, v]() { offline_save_by_id_async(id, v.title, "", false); }),
			                 make_option(tr("QR code", "Código QR"), [id]() { qr_popup_show(share_url(id)); })});
		    });
		list->views.push_back(view);
	}
	if (!videos.size()) {
		list->views.push_back((new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL * 3))
		                          ->set_text(loading ? tr("Loading...", "Cargando...") : failed ? tr("Could not load your liked videos", "No se pudieron cargar tus Me gusta") : tr("No liked videos.", "Sin vídeos con Me gusta."))
		                          ->set_x_alignment(TextView::XAlign::CENTER)
		                          ->set_font_size(0.5, DEFAULT_FONT_INTERVAL));
	}
}

static void build_watch_later(VerticalListView *list) {
	static bool pulled_this_session = false; // YouTube's Watch later list is downloaded once per session (OAuth only)
	if (!pulled_this_session && OAuth::is_authenticated()) {
		pulled_this_session = true;
		queue_async_task(pull_watch_later_task, NULL);
	}
	auto favs = filter_and_sort(watch_later_get());
	add_sort_filter_row(list);
	for (auto &v : favs) {
		SuccinctVideoView *view = make_item(v.id, v.title, v.author, v.length_text);
		std::string id = v.id, url = youtube_get_video_url_by_id(v.id);
		view->set_get_background_color(button_background)
		    ->set_on_view_released([url](View &) { clicked_url = url; })
		    ->add_on_long_hold(35, [id, url, v](View &) {
			    show_dialog({make_option(tr("Play next", "Reproducir a continuación"), [url, v]() { play_queue_play_next(url, v.title); }),
			                 make_option(tr("Add to queue", "Añadir a la cola"), [url, v]() { play_queue_toggle(url, v.title); }),
			                 make_option(tr("Add to a list...", "Añadir a una lista..."), [v]() { pending_action = [v]() { add_to_playlist_dialog(v); }; }),
			                 make_option(tr("Save video offline", "Guardar vídeo sin conexión"), [id, v]() { offline_save_by_id_async(id, v.title, "", true); }),
			                 make_option(tr("Save audio offline", "Guardar audio sin conexión"), [id, v]() { offline_save_by_id_async(id, v.title, "", false); }),
			                 make_option(tr("QR code", "Código QR"), [id]() { qr_popup_show(share_url(id)); }),
			                 make_option(tr("Remove from watch later", "Quitar de ver más tarde"), [id]() { watch_later_remove(id); })});
		    });
		list->views.push_back(view);
	}
	if (!favs.size()) {
		list->views.push_back((new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL * 4))
		                          ->set_text_lines(std::vector<std::string>{tr("Nothing to watch later yet.", "Nada para ver más tarde."), tr("Use the Later button inside", "Usa el botón Luego dentro"), tr("a video.", "de un vídeo.")})
		                          ->set_x_alignment(TextView::XAlign::CENTER)
		                          ->set_font_size(0.5, DEFAULT_FONT_INTERVAL));
	}
}

static u64 file_size(const std::string &path) {
	struct stat st;
	return stat(path.c_str(), &st) == 0 ? (u64)st.st_size : 0;
}

static std::string size_text(u64 bytes) {
	char buf[32];
	if (bytes >= 1024ULL * 1024 * 1024) snprintf(buf, sizeof buf, "%.2f GB", bytes / 1073741824.0);
	else snprintf(buf, sizeof buf, "%llu MB", (unsigned long long)(bytes / 1048576));
	return buf;
}

static std::string offline_filter; // only the saved items whose title or author has this text
static int offline_sort = 0; // 0 newest first, 1 title A-Z, 2 biggest first

static void play_offline(bool shuffle) {
	std::vector<std::pair<std::string, std::string>> items;
	for (auto &item : offline_list()) items.push_back({"offline:" + item.id, item.title});
	if (items.empty()) return;
	if (shuffle) {
		for (size_t i = items.size(); i > 1; i--) std::swap(items[i - 1], items[rand() % i]);
	}
	play_queue_clear();
	for (size_t i = 1; i < items.size(); i++) play_queue_toggle(items[i].first, items[i].second);
	clicked_url = items[0].first;
}

static void build_offline(VerticalListView *list) {
	auto items = offline_list();
	const size_t all_items = items.size();
	if (offline_filter != "") {
		auto lower = [](std::string s) { for (auto &c : s) c = tolower((unsigned char)c); return s; };
		std::string needle = lower(offline_filter);
		items.erase(std::remove_if(items.begin(), items.end(), [&](const OfflineItem &item) {
			            return lower(item.title).find(needle) == std::string::npos && lower(item.author).find(needle) == std::string::npos;
		            }),
		            items.end());
	}
	if (all_items > 3) { // search inside the downloads
		list->views.push_back((new HorizontalListView(0, 0, 22))
		                          ->set_views({make_button(offline_filter == "" ? tr("Search downloads", "Buscar en descargas") : tr("Search: ", "Búsqueda: ") + offline_filter,
		                                                   offline_filter == "" ? 320 : 240,
		                                                   []() {
			                                                   std::string text;
			                                                   if (ask_text(tr("Search downloads", "Buscar en descargas").c_str(), text)) {
				                                                   offline_filter = text;
				                                                   rebuild_request = true;
			                                                   }
		                                                   }),
		                                       offline_filter == "" ? (View *)new EmptyView(0, 0, 0, 22)
		                                                            : (View *)make_button(tr("Clear", "Quitar"), 80, []() {
			                                                              offline_filter = "";
			                                                              rebuild_request = true;
		                                                              })}));
	}
	if (offline_sort == 1) {
		std::sort(items.begin(), items.end(), [](const OfflineItem &a, const OfflineItem &b) { return a.title < b.title; });
	} else if (offline_sort == 2) {
		std::sort(items.begin(), items.end(), [](const OfflineItem &a, const OfflineItem &b) {
			return file_size(offline_file_path(a.id)) + file_size(offline_video_path(a.id)) >
			       file_size(offline_file_path(b.id)) + file_size(offline_video_path(b.id));
		});
	}
	if (items.size() > 1) {
		list->views.push_back(make_button(offline_sort == 0   ? tr("Sort: newest first", "Orden: más recientes")
		                                  : offline_sort == 1 ? tr("Sort: title A-Z", "Orden: título A-Z")
		                                                      : tr("Sort: biggest first", "Orden: más grandes"),
		                                  320, []() {
			                                  offline_sort = (offline_sort + 1) % 3;
			                                  rebuild_request = true;
		                                  }));
	}
	if (items.size() > 0) { // delete everything (asks first)
		list->views.push_back(make_button(tr("Delete all downloads", "Borrar todas las descargas"), 320, []() {
			show_dialog({make_option(tr("Yes, delete everything", "Sí, borrar todo"), []() {
				             for (auto &item : offline_list()) offline_remove(item.id);
				             rebuild_request = true;
			             }),
			             make_option(LOCALIZED(CANCEL), []() {})});
		}));
	}
	if (items.size() > 1) { // play everything that is saved, in order or shuffled
		list->views.push_back((new HorizontalListView(0, 0, 22))
		                          ->set_views({make_button(tr("Play all", "Reproducir todo"), 160, []() { play_offline(false); }),
		                                       make_button(tr("Shuffle", "Aleatorio"), 160, []() { play_offline(true); })}));
	}
	{ // summary : how many items, how much room they use and how much is free on the SD card
		u64 used = 0;
		for (auto &item : items) used += file_size(offline_file_path(item.id)) + file_size(offline_video_path(item.id));
		std::string text = std::to_string(items.size()) + " " + tr("items", "elementos") + " - " + size_text(used) + " " + tr("saved", "guardados");
		FS_ArchiveResource resource;
		if (R_SUCCEEDED(FSUSER_GetArchiveResource(&resource, SYSTEM_MEDIATYPE_SD))) {
			text += " - " + size_text((u64)resource.freeClusters * resource.clusterSize) + " " + tr("free", "libres");
		}
		list->views.push_back((new TextView(0, 0, 320, 22))
		                          ->set_text(text)
		                          ->set_font_size(0.5, DEFAULT_FONT_INTERVAL)
		                          ->set_y_alignment(TextView::YAlign::CENTER)
		                          ->set_text_offset(SMALL_MARGIN, 0)
		                          ->set_get_background_color([](const View &) { return LIGHT0_BACK_COLOR; }));
		// the running download : its progress, tap to cancel
		list->views.push_back((new TextView(0, 0, 320, 22))
		                          ->set_text((std::function<std::string()>)[]() {
			                          if (!offline_download_running()) return tr("No download running", "Ninguna descarga en curso");
			                          int queued = offline_download_queued();
			                          return tr("Downloading ", "Descargando ") + std::to_string(offline_download_percent()) + "%" +
			                                 (queued ? " (+" + std::to_string(queued) + ")" : "") + " - " + tr("tap to cancel", "toca para cancelar");
		                          })
		                          ->set_font_size(0.5, DEFAULT_FONT_INTERVAL)
		                          ->set_y_alignment(TextView::YAlign::CENTER)
		                          ->set_text_offset(SMALL_MARGIN, 0)
		                          ->set_get_background_color(View::STANDARD_BACKGROUND)
		                          ->set_on_view_released([](View &) {
			                          if (offline_download_running()) offline_download_cancel();
			                          var_need_refresh = true;
		                          }));
	}
	for (auto &item : items) {
		SuccinctVideoView *view = make_item(item.id, item.title, item.author, item.quality > 0 ? std::to_string(item.quality) + "p" : tr("audio", "audio"));
		std::string id = item.id, url = "offline:" + item.id, title = item.title;
		view->set_get_background_color(button_background)
		    ->set_on_view_released([url](View &) { clicked_url = url; })
		    ->add_on_long_hold(35, [id, url, title](View &) {
			    show_dialog({make_option(tr("Play next", "Reproducir a continuación"), [url, title]() { play_queue_play_next(url, title); }),
			                 make_option(tr("Delete from the SD card", "Borrar de la SD"), [id]() { offline_remove(id); rebuild_request = true; })});
		    });
		list->views.push_back(view);
	}
	if (!items.size()) {
		list->views.push_back((new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL * 4))
		                          ->set_text_lines(all_items > 0 ? std::vector<std::string>{tr("No downloads match.", "Ninguna descarga coincide."), "", ""}
		                                                                         : std::vector<std::string>{tr("Nothing saved yet.", "Aún no hay nada guardado."), tr("Use Save video or Save audio inside a video.", "Usa Guardar vídeo o Guardar audio dentro de un vídeo."), ""})
		                          ->set_x_alignment(TextView::XAlign::CENTER)
		                          ->set_font_size(0.5, DEFAULT_FONT_INTERVAL));
	}
}

static void build_playlists(VerticalListView *list) {
	auto lists = playlists_get();
	if (open_playlist >= (int)lists.size()) {
		open_playlist = -1;
	}

	if (open_playlist < 0) { // all the playlists
		list->views.push_back(make_button(tr("+ New playlist", "+ Nueva lista"), 320, []() { new_playlist_request = true; }));
		for (size_t i = 0; i < lists.size(); i++) {
			auto &pl = lists[i];
			View *row = (new TextView(0, 0, 320, 30))
			                ->set_text(pl.name + "  (" + std::to_string(pl.videos.size()) + ")")
			                ->set_font_size(0.55, DEFAULT_FONT_INTERVAL)
			                ->set_y_alignment(TextView::YAlign::CENTER)
			                ->set_get_background_color(button_background)
			                ->set_on_view_released([i](View &) {
				                open_playlist = i;
				                rebuild_request = true;
			                })
			                ->add_on_long_hold(35, [i, pl](View &) {
				                show_dialog({make_option(tr("Play", "Reproducir"), [pl]() { start_playlist(pl, false); }),
				                             make_option(tr("Shuffle", "Aleatorio"), [pl]() { start_playlist(pl, true); }),
				                             make_option("Eliminar lista", [i]() { playlist_delete(i); })});
			                });
			list->views.push_back(row);
		}
		if (!lists.size()) {
			list->views.push_back((new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL * 4))
			                          ->set_text_lines(std::vector<std::string>{tr("You have no playlists.", "No tienes listas."), tr("Create one and add videos from", "Crea una y añade vídeos desde"), tr("Favorites or the queue (long press).", "Favoritos o la cola (pulsación larga).")})
			                          ->set_x_alignment(TextView::XAlign::CENTER)
			                          ->set_font_size(0.5, DEFAULT_FONT_INTERVAL));
		}
		return;
	}

	// one playlist
	UserPlaylist pl = lists[open_playlist];
	int index = open_playlist;
	list->views.push_back(make_button("< " + pl.name, 320, []() {
		open_playlist = -1;
		rebuild_request = true;
	}));
	list->views.push_back((new HorizontalListView(0, 0, 22))
	                          ->set_views({make_button(tr("Play", "Reproducir"), 106, [pl]() { start_playlist(pl, false); }),
	                                       make_button(tr("Shuffle", "Aleatorio"), 106, [pl]() { start_playlist(pl, true); }),
	                                       make_button(playlist_repeat ? tr("Repeat: on", "Repetir: sí") : tr("Repeat: off", "Repetir: no"), 108, []() {
		                                       playlist_repeat = !playlist_repeat;
		                                       rebuild_request = true;
	                                       })}));
	for (auto &v : pl.videos) {
		SuccinctVideoView *view = make_item(v.id, v.title, v.author, v.length_text);
		std::string id = v.id, url = youtube_get_video_url_by_id(v.id);
		view->set_get_background_color(button_background)
		    ->set_on_view_released([url](View &) { clicked_url = url; })
		    ->add_on_long_hold(35, [index, id, url, v](View &) {
			    show_dialog({make_option(tr("Play next", "Reproducir a continuación"), [url, v]() { play_queue_play_next(url, v.title); }),
			                 make_option(tr("Add to queue", "Añadir a la cola"), [url, v]() { play_queue_toggle(url, v.title); }),
			                 make_option(tr("Save video offline", "Guardar vídeo sin conexión"), [id, v]() { offline_save_by_id_async(id, v.title, "", true); }),
			                 make_option(tr("Save audio offline", "Guardar audio sin conexión"), [id, v]() { offline_save_by_id_async(id, v.title, "", false); }),
			                 make_option(tr("QR code", "Código QR"), [id]() { qr_popup_show(share_url(id)); }),
			                 make_option(tr("Remove from list", "Quitar de la lista"), [index, id]() { playlist_remove(index, id); })});
		    });
		list->views.push_back(view);
	}
	if (!pl.videos.size()) {
		list->views.push_back((new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL * 4))
		                          ->set_text_lines(std::vector<std::string>{tr("Empty list.", "Lista vacía."), tr("Add videos from Favorites", "Añade vídeos desde Favoritos"), tr("or the queue (long press).", "o la cola (pulsación larga).")})
		                          ->set_x_alignment(TextView::XAlign::CENTER)
		                          ->set_font_size(0.5, DEFAULT_FONT_INTERVAL));
	}
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
		build_queue(list);
	} else if (mode == 3) {
		build_watch_later(list);
	} else if (mode == 5) {
		build_liked(list);
	} else if (mode == 4) {
		build_offline(list);
	} else if (mode == 1) {
		build_favorites(list);
	} else {
		build_playlists(list);
	}

	constexpr int selector_width = 320; // the whole width : the six tabs need it (the nav bar already says Library)
	main_view = (new ScrollView(0, 0, 320, 240))
	                ->set_views({(new HorizontalListView(0, 0, MIDDLE_FONT_INTERVAL))
	                                 ->set_views({(new SelectorView(0, 0, selector_width, MIDDLE_FONT_INTERVAL, false))
	                                                  ->set_texts({(std::function<std::string()>)[]() { return std::string(tr("Queue", "Cola")); },
	                                                               (std::function<std::string()>)[]() { return std::string(tr("Favorites", "Favoritos")); },
	                                                               (std::function<std::string()>)[]() { return std::string(tr("Playlists", "Listas")); },
	                                                               (std::function<std::string()>)[]() { return std::string(tr("Later", "Luego")); },
	                                                               (std::function<std::string()>)[]() { return std::string("Offline"); },
	                                                               (std::function<std::string()>)[]() { return std::string(tr("Liked", "Me gusta")); }},
	                                                              mode)
	                                                  ->set_on_change([](const SelectorView &view) {
		                                                  mode = view.selected_button;
		                                                  open_playlist = -1;
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
		qr_popup_draw();

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
		if (qr_popup_update(key)) {
			// the QR popup takes the input while it is open
		} else if (on_long_tap_dialog->is_visible) {
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
		if (pending_action) { // e.g. a second dialog : opened here because it replaces the views that were just updated
			auto action = pending_action;
			pending_action = nullptr;
			action();
		}
		if (new_playlist_request) { // the keyboard is opened after the update, never from inside a view callback
			new_playlist_request = false;
			std::string name;
			if (ask_text(tr("Playlist name", "Nombre de la lista").c_str(), name)) {
				playlist_create(name);
			}
			rebuild_request = true;
		}
		if (rebuild_request) { // after the update : the views of the list may have just been used
			rebuild_request = false;
			rebuild_views();
		}
		if (key.p_b) {
			if (mode == 2 && open_playlist >= 0) {
				open_playlist = -1; // back to the list of playlists
				rebuild_views();
			} else {
				global_intent.next_scene = SceneType::BACK;
			}
		}
	}
}
