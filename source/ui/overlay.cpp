#include "ui/overlay.hpp"
#include "ui/nav_draw.hpp"
#include "variables.hpp"
#include "headers.hpp"
#include "ui/ui.hpp"
#include "data_io/string_resource.hpp"
#include "util/play_queue.hpp"
#include "scenes/library.hpp"
#include "scenes/offline_tasks.hpp"
#include "util/misc_tasks.hpp"
#include "ui/notice.hpp"
#include "data_io/watch_later.hpp"
#include "data_io/favorites.hpp"
#include "util/result_filter.hpp"
#include "data_io/user_playlists.hpp"
#include "util/pinned_channels.hpp"
#include "scenes/home.hpp"
#include "data_io/history.hpp"
#include "data_io/subscription_util.hpp"
#include "scenes/account_tasks.hpp"
#include "youtube_parser/parser.hpp"
#include "oauth/oauth.hpp"
#include <algorithm>

#define ICON_LINE_HEIGHT 3
#define ICON_LINE_Y_MARGIN 4
#define ICON_LINE_X_MARGIN 5

#define CONTENT_HEIGHT 25
#define CONTENT_WIDTH 120

struct Content {
	std::string string;
	enum class Type {
		SEARCH,
		HISTORY,
		LIBRARY,
		HOME,
		EXIT,
		SETTINGS,
		ABOUT,
		INCOGNITO,
	};
	Type type;
};
static std::vector<Content> contents;

enum MenuStatus {
	CLOSED,
	OPEN,
	CONFIRMING_CLOSE,
};
static int menu_status = CLOSED;
static bool is_menu_drawn() { return menu_status == OPEN || menu_status == CONFIRMING_CLOSE; }

static int menu_icon_x = 320 - OVERLAY_MENU_ICON_SIZE;
static int menu_icon_y;

static float menu_anim = 0; // 0 : closed, 1 : fully open (slide-in animation)
static int last_touch_x = -1;
static int last_touch_y = -1;
static bool holding_touch = false;

static OverlayDialogView *dialog_view = (new OverlayDialogView(0, 0, 320, 240));

#define menu_icon_background_color COLOR_GRAY(var_night_mode ? 0x3F : 0xC0)
#define menu_icon_background_color_selected COLOR_GRAY(var_night_mode ? 0x5F : 0xA0)
#define menu_icon_line_color COLOR_GRAY(var_night_mode ? 0xCC : 0x33)
#define menu_content_background_color menu_icon_background_color
#define menu_content_background_color_selected COLOR_GRAY(var_night_mode ? 0x5F : 0xA0)
#define menu_content_string_color DEFAULT_TEXT_COLOR
#define menu_content_border_color COLOR_GRAY(var_night_mode ? 0x22 : 0xDD)

// ---- the video actions menu (long press on a video) ----
static bool waiting_account_playlists = false; // "Add to my YouTube playlist" asked for the lists : the menu opens when they arrive
static u64 waiting_account_playlists_since = 0;
static OverlayView *video_menu = NULL;
static void video_menu_close() {
	if (video_menu) video_menu->set_is_visible(false);
	var_need_refresh = true;
}
static bool menu_next_request = false; // "More options..." was tapped : the next page opens after the update (a menu cannot rebuild itself while it handles a tap)
static std::string menu_next_title;
static std::vector<std::pair<std::string, std::function<void()>>> menu_next_options;
static size_t menu_next_first = 0;
static void show_menu_rows(const std::string &title, const std::vector<std::pair<std::string, std::function<void()>>> &options, size_t first = 0);
void overlay_show_video_menu(const std::string &url, const std::string &title, const std::string &author,
                             const std::string &extra_label, std::function<void()> extra_action) {
	std::string id = youtube_get_video_id_by_url(url);
	std::vector<std::pair<std::string, std::function<void()>>> options;
	auto option = [&](const std::string &text, std::function<void()> action) { options.push_back({text, action}); };
	FavoriteVideo video;
	video.id = id;
	video.title = title;
	video.author = author;
	bool queued = play_queue_position(url) != 0;
	option(tr("Play next", "Reproducir a continuación"), [url, title]() { play_queue_play_next(url, title); notice_show(tr("Plays next", "Suena a continuación")); });
	option(queued ? tr("Remove from queue", "Quitar de la cola") : tr("Add to queue", "Añadir a la cola"), [url, title]() { play_queue_toggle(url, title); });
	option(watch_later_contains(id) ? tr("Remove from watch later", "Quitar de ver más tarde") : tr("Watch later", "Ver más tarde"), [video]() {
		bool added = watch_later_toggle(video);
		notice_show(added ? tr("Added to Watch later", "Añadido a Ver más tarde") : tr("Removed from Watch later", "Quitado de Ver más tarde"));
	});
	option(favorites_contains(id) ? tr("Remove from favorites", "Quitar de favoritos") : tr("+ Favorite", "+ Favorito"), [video]() {
		bool added = favorites_toggle(video);
		notice_show(added ? tr("Added to Favorites", "Añadido a Favoritos") : tr("Removed from Favorites", "Quitado de Favoritos"));
	});
	if (!var_incognito && resume_get(id) > 0) {
		option(tr("Forget where I stopped", "Olvidar dónde lo dejé"), [id]() {
			resume_set(id, 0, 0); // (positions under 15 s are not kept : this erases it)
			resume_save();
			notice_show(tr("Forgotten", "Olvidado"));
			var_need_refresh = true;
		});
	}
	option(tr("Add to a local list", "Añadir a una lista local"), [video]() {
		auto lists = playlists_get();
		if (lists.empty()) {
			notice_show(tr("You have no playlists. Create one in the Library.", "No tienes listas. Crea una en la Biblioteca."));
			return;
		}
		static FavoriteVideo pending_video;
		pending_video = video;
		std::vector<std::pair<std::string, std::function<void()>>> options;
		for (size_t i = 0; i < lists.size(); i++) {
			int index = (int)i;
			std::string name = lists[i].name;
			options.push_back({truncate_first_line(name, 200, 0.5) + " (" + std::to_string(lists[i].videos.size()) + ")", [index, name]() {
				notice_show(playlist_add(index, pending_video) ? tr("Added to ", "Añadido a ") + name : tr("Already in that list", "Ya está en esa lista"));
			}});
		}
		menu_next_request = true; // (opened after the update : this menu is closing right now)
		menu_next_title = tr("Add to a list", "Añadir a una lista");
		menu_next_options = options;
		menu_next_first = 0;
	});
	if (OAuth::is_authenticated()) {
		option(tr("Like on YouTube", "Me gusta en YouTube"), [id]() { account_like_async(id, 1); });
		option(tr("Add to my YouTube playlist", "Añadir a mi lista de YouTube"), [id]() {
			waiting_account_playlists = true;
			waiting_account_playlists_since = osGetTime();
			account_playlists_async(id);
		});
	}
	if (author != "") { // hide / show the videos of this channel in the lists
		bool hidden = hidden_channel_is(author);
		option(hidden ? tr("Show this channel in lists", "Mostrar este canal en las listas") : tr("Hide this channel in lists", "Ocultar este canal en las listas"), [author, hidden]() {
			hidden_channel_set(author, !hidden);
			notice_show(hidden ? tr("Channel shown again (reload the list)", "Canal visible otra vez (recarga la lista)") : tr("Channel hidden (it applies to lists loaded from now on)", "Canal oculto (vale para las listas que cargues desde ahora)"));
		});
	}
	if (author != "") {
		option(tr("More from this channel", "Más de este canal"), [author]() {
			global_intent.next_scene = SceneType::SEARCH;
			global_intent.arg = "q:" + author;
		});
	}
	option(tr("Search similar", "Buscar parecidos"), [title]() {
		global_intent.next_scene = SceneType::SEARCH;
		global_intent.arg = "q:" + title;
	});
	option(history_has_id(id) ? tr("Mark as not watched", "Marcar como no visto") : tr("Mark as watched", "Marcar como visto"), [id, title, author]() {
		if (var_incognito || !var_history_enabled) {
			notice_show(tr("The history is off (incognito or disabled)", "El historial está apagado (incógnito o desactivado)"));
			return;
		}
		if (history_has_id(id)) {
			history_erase_by_id(id);
			misc_tasks_request(TASK_SAVE_HISTORY);
		} else {
			HistoryVideo h;
			h.id = id;
			h.title = title;
			h.title_lines = {title};
			h.author_name = author;
			h.length_text = "";
			h.my_view_count = 1;
			h.last_watch_time = time(NULL);
			add_watched_video(h);
			misc_tasks_request(TASK_SAVE_HISTORY);
		}
		var_need_refresh = true;
	});
	option(tr("Save audio offline", "Guardar audio sin conexión"), [id, title, author]() { offline_save_by_id_async(id, title, author, false); });
	option(tr("Save video offline", "Guardar vídeo sin conexión"), [id, title, author]() { offline_save_by_id_async(id, title, author, true); });
	if (OAuth::is_authenticated()) {
		option(tr("Not interested", "No me interesa"), [id]() { account_not_interested_async(id, false); });
	}
	if (extra_label != "" && extra_action) option(extra_label, extra_action);
	show_menu_rows(title, options);
}

static void show_menu_rows(const std::string &title, const std::vector<std::pair<std::string, std::function<void()>>> &options, size_t first) {
	if (!video_menu) {
		video_menu = new OverlayView(0, 0, 320, 240);
		video_menu->set_on_cancel([](OverlayView &) { video_menu_close(); });
	}
	video_menu->recursive_delete_subviews();
	const int width = 250;
	const size_t per_page = 6; // 22 + 6 * 24 + More + Cancel stays inside the 240 pixels of the screen
	if (first >= options.size()) first = 0;
	std::vector<View *> rows;
	rows.push_back((new TextView(0, 0, width, 22))
	                   ->set_text(truncate_first_line(title, width - 8, 0.5))
	                   ->set_x_alignment(TextView::XAlign::CENTER)
	                   ->set_y_alignment(TextView::YAlign::CENTER)
	                   ->set_get_text_color([]() { return (u32)UI_ACCENT_TEXT; })
	                   ->set_get_background_color([](const View &) { return LIGHT0_BACK_COLOR; }));
	size_t last = std::min(options.size(), first + per_page);
	if (options.size() - first <= per_page + 1) last = options.size(); // (the last page may hold one more : no "More" for a single option)
	for (size_t i = first; i < last; i++) {
		auto action = options[i].second;
		rows.push_back((new TextView(0, 0, width, 24))
		                   ->set_text(options[i].first)
		                   ->set_x_alignment(TextView::XAlign::CENTER)
		                   ->set_y_alignment(TextView::YAlign::CENTER)
		                   ->set_get_background_color(View::STANDARD_BACKGROUND)
		                   ->set_on_view_released([action](View &) {
			                   video_menu_close();
			                   action();
		                   }));
	}
	if (first > 0) { // back to the page before
		rows.push_back((new TextView(0, 0, width, 24))
		                   ->set_text(tr("Previous options...", "Opciones anteriores..."))
		                   ->set_x_alignment(TextView::XAlign::CENTER)
		                   ->set_y_alignment(TextView::YAlign::CENTER)
		                   ->set_get_text_color([]() { return (u32)UI_ACCENT_TEXT; })
		                   ->set_get_background_color(View::STANDARD_BACKGROUND)
		                   ->set_on_view_released([title, options, first, per_page](View &) { menu_next_request = true; menu_next_title = title; menu_next_options = options; menu_next_first = first >= per_page ? first - per_page : 0; }));
	}
	if (last < options.size()) { // more options on the next page
		rows.push_back((new TextView(0, 0, width, 24))
		                   ->set_text(tr("More options...", "Más opciones..."))
		                   ->set_x_alignment(TextView::XAlign::CENTER)
		                   ->set_y_alignment(TextView::YAlign::CENTER)
		                   ->set_get_text_color([]() { return (u32)UI_ACCENT_TEXT; })
		                   ->set_get_background_color(View::STANDARD_BACKGROUND)
		                   ->set_on_view_released([title, options, last](View &) { menu_next_request = true; menu_next_title = title; menu_next_options = options; menu_next_first = last; }));
	}
	rows.push_back((new TextView(0, 0, width, 24))
	                   ->set_text(LOCALIZED(CANCEL))
	                   ->set_x_alignment(TextView::XAlign::CENTER)
	                   ->set_y_alignment(TextView::YAlign::CENTER)
	                   ->set_get_background_color(View::STANDARD_BACKGROUND)
	                   ->set_on_view_released([](View &) { video_menu_close(); }));
	video_menu->set_subview((new VerticalListView(0, 0, width))->set_views(rows)->set_get_background_color([](const View &) { return (u32)UI_PAGE_COLOR; })->set_corner_radius(10));
	video_menu->set_is_visible(true);
	var_need_refresh = true;
}
void overlay_show_menu(const std::string &title, const std::vector<std::pair<std::string, std::function<void()>>> &options) { show_menu_rows(title, options); }
void overlay_show_menu_deferred(const std::string &title, const std::vector<std::pair<std::string, std::function<void()>>> &options) {
	menu_next_request = true;
	menu_next_title = title;
	menu_next_options = options;
	menu_next_first = 0;
}

void overlay_show_channel_menu(const std::string &id, const std::string &name, const std::string &icon_url, const std::string &subscribers) {
	std::vector<std::pair<std::string, std::function<void()>>> options;
	bool subscribed = subscription_is_subscribed(id);
	options.push_back({subscribed ? tr("Unsubscribe", "Quitar suscripción") : tr("Subscribe", "Suscribirme"), [id, name, icon_url, subscribers, subscribed]() {
		if (subscribed) {
			subscription_unsubscribe(id);
		} else {
			SubscriptionChannel channel;
			channel.id = id;
			channel.url = "https://m.youtube.com/channel/" + id;
			channel.name = name;
			channel.icon_url = icon_url;
			channel.subscriber_count_str = subscribers;
			subscription_subscribe(channel);
		}
		if (OAuth::is_authenticated()) account_subscribe_async(id, !subscribed);
		notice_show(subscribed ? tr("Unsubscribed", "Suscripción quitada") : tr("Subscribed", "Suscrito"));
	}});
	if (subscribed) { // the channels pinned on top of the subscribed channels list
		bool pinned = pinned_channel_is(id);
		options.push_back({pinned ? tr("Unpin from the top", "Quitar de arriba") : tr("Pin on top", "Fijar arriba"), [id, pinned]() {
			pinned_channel_set(id, !pinned);
			Home_update_local_channels();
			notice_show(pinned ? tr("Unpinned", "Quitado de arriba") : tr("Pinned on top of your channels", "Fijado arriba en tus canales"));
		}});
	}
	bool hidden = hidden_channel_is(name);
	options.push_back({hidden ? tr("Show in lists again", "Mostrar en las listas") : tr("Hide in lists", "Ocultar en las listas"), [name, hidden]() {
		hidden_channel_set(name, !hidden);
		notice_show(hidden ? tr("Channel shown again (reload the list)", "Canal visible otra vez (recarga la lista)") : tr("Channel hidden (it applies to lists loaded from now on)", "Canal oculto (vale para las listas que cargues desde ahora)"));
	}});
	if (OAuth::is_authenticated()) {
		options.push_back({tr("Bell: all notifications", "Campana: todas"), [id]() { account_bell_async(id, 0); }});
		options.push_back({tr("Bell: personalized", "Campana: personalizadas"), [id]() { account_bell_async(id, 1); }});
		options.push_back({tr("Bell: none", "Campana: ninguna"), [id]() { account_bell_async(id, 2); }});
	}
	show_menu_rows(name, options);
}

static bool nav_enabled = false;
static int nav_top = 240 - NAV_BAR_HEIGHT;
#define NAV_ITEMS 5
#define NAV_MORE 4
void overlay_set_nav(bool enabled, int bottom) {
	nav_enabled = enabled;
	nav_top = bottom - NAV_BAR_HEIGHT;
}
static int nav_at(int x, int y) {
	if (!nav_enabled || x < 0 || y < nav_top || y >= nav_top + NAV_BAR_HEIGHT) {
		return -1;
	}
	return std::min(NAV_ITEMS - 1, x * NAV_ITEMS / 320);
}
// the item that matches the current scene (-1 : none)
static int nav_selected() {
	switch (global_current_scene) {
	case SceneType::HOME:
		return 0;
	case SceneType::SEARCH:
		return 1;
	case SceneType::LIBRARY:
		return Library_is_offline_mode() ? 3 : 2;
	default:
		return -1;
	}
}
static void draw_nav_bar() {
	const std::string labels[NAV_ITEMS] = {LOCALIZED(HOME), tr("Search", "Buscar"), tr("Library", "Biblioteca"),
	                                       tr("Downloads", "Descargas"), tr("More", "Más")};
	NavPaint paint;
	paint.top = nav_top;
	paint.height = NAV_BAR_HEIGHT;
	paint.selected = menu_status != CLOSED ? NAV_MORE : nav_selected();
	paint.pressed = holding_touch ? nav_at(last_touch_x, last_touch_y) : -1;
	paint.incognito = var_incognito;
	paint.offline = var_wifi_state != 2;
	paint.download_running = offline_download_running();
	paint.queue_size = play_queue_size();
	nav_paint(paint, labels);
}

static bool in_icon(int x, int y) {
	if (nav_enabled) {
		return nav_at(x, y) == NAV_MORE;
	}
	return x >= menu_icon_x && x < menu_icon_x + OVERLAY_MENU_ICON_SIZE && y >= menu_icon_y &&
	       y < menu_icon_y + OVERLAY_MENU_ICON_SIZE;
}
static int content_at(int x, int y) {
	if (x < 320 - CONTENT_WIDTH) {
		return -1;
	}
	int y_base = menu_icon_y - contents.size() * CONTENT_HEIGHT;
	if (y < y_base || y >= menu_icon_y) {
		return -1;
	}
	return (y - y_base) / CONTENT_HEIGHT;
}

void draw_overlay_menu(int y) {
	menu_icon_y = nav_enabled ? nav_top : y;

	u32 color;
	if (nav_enabled) {
		draw_nav_bar();
		if (var_update_available || var_update_banner || var_new_feed_videos > 0) { // red dot on "More"
			Draw_texture(var_square_image[0], DEF_DRAW_RED, 320 - 22, nav_top + 3, 7, 7);
		}
	} else {
		color = holding_touch && in_icon(last_touch_x, last_touch_y) ? menu_icon_background_color_selected
		                                                             : menu_icon_background_color;
		Draw_texture(var_square_image[0], color, menu_icon_x, menu_icon_y, OVERLAY_MENU_ICON_SIZE, OVERLAY_MENU_ICON_SIZE);
		for (int i = 0; i < 3; i++) {
			int y = menu_icon_y + (float)(OVERLAY_MENU_ICON_SIZE - ICON_LINE_HEIGHT * 3 - ICON_LINE_Y_MARGIN * 2) / 2 +
			        i * (ICON_LINE_HEIGHT + ICON_LINE_Y_MARGIN);
			int x = menu_icon_x + ICON_LINE_X_MARGIN;
			int width = OVERLAY_MENU_ICON_SIZE - ICON_LINE_X_MARGIN * 2;
			Draw_texture(var_square_image[0], menu_icon_line_color, x, y, width, ICON_LINE_HEIGHT);
		}

		if (var_update_available || var_update_banner || var_new_feed_videos > 0) { // red dot : an update is available / news about an update
			Draw_texture(var_square_image[0], DEF_DRAW_RED, menu_icon_x + OVERLAY_MENU_ICON_SIZE - 10, menu_icon_y + 2, 8, 8);
		}
	}

	// ease the menu in/out (entries slide in from the right edge)
	float anim_target = is_menu_drawn() ? 1 : 0;
	menu_anim += (anim_target - menu_anim) * 0.4f;
	if (std::abs(anim_target - menu_anim) < 0.02f) {
		menu_anim = anim_target;
	} else {
		var_need_refresh = true; // keep redrawing until the animation ends
	}
	if (menu_anim > 0) {
		int y_base = menu_icon_y - contents.size() * CONTENT_HEIGHT;
		int slide = (int)((1 - menu_anim) * CONTENT_WIDTH);
		for (int i = 0; i < (int)contents.size(); i++) {
			u32 color = holding_touch && content_at(last_touch_x, last_touch_y) == i
			            ? menu_content_background_color_selected
			            : menu_content_background_color;
			Draw_texture(var_square_image[0], color, 320 - CONTENT_WIDTH + slide, y_base + i * CONTENT_HEIGHT,
			             CONTENT_WIDTH, CONTENT_HEIGHT);
			Draw_texture(var_square_image[0], COLOR_ACCENT, 320 - CONTENT_WIDTH + slide, y_base + i * CONTENT_HEIGHT, 2,
			             CONTENT_HEIGHT);
			int string_width = Draw_get_width(contents[i].string, 0.5);
			int string_x = 320 - CONTENT_WIDTH + slide + (CONTENT_WIDTH - string_width) / 2;
			Draw(contents[i].string, string_x, y_base + i * CONTENT_HEIGHT + 5, 0.5, 0.5, menu_content_string_color);
		}
	}
	if (menu_status == CONFIRMING_CLOSE) {
		dialog_view->draw();
	}
	if (video_menu && video_menu->is_visible) video_menu->draw();
}
// the news of a new version : shown once after an update (the version is kept in sdmc:/3ds/IkersTube/last_version.txt)
static std::string core_version() {
	std::string v = DEF_CURRENT_APP_VER;
	return v.substr(0, v.find('-'));
}
static bool whats_new_pending() {
	static bool checked = false, pending = false;
	if (checked) return pending;
	checked = true;
	if (var_safe_mode) return pending = false; // (safe mode: no dialogs)
	char buf[40] = {0};
	FILE *f = fopen("sdmc:/3ds/IkersTube/last_version.txt", "r");
	if (f) {
		if (!fgets(buf, sizeof buf, f)) buf[0] = 0;
		fclose(f);
	}
	std::string last = buf;
	while (last.size() && (last.back() == '\n' || last.back() == '\r')) last.pop_back();
	pending = last != core_version();
	return pending;
}
static void whats_new_done() {
	FILE *w = fopen("sdmc:/3ds/IkersTube/last_version.txt", "w");
	if (w) {
		fprintf(w, "%s\n", core_version().c_str());
		fclose(w);
	}
}
static void show_whats_new() {
	dialog_view->get_message_view()
	    ->set_text_lines(std::vector<std::string>{tr("What's new in ", "Novedades de ") + core_version(),
	                                              tr("Feed: instant start, NEW badges, play the new ones", "Feed: arranque al instante, etiquetas NEW, ver los nuevos"),
	                                              tr("Player: all tools reachable, skip intro, go to time", "Reproductor: todas las herramientas, saltar intro, ir al minuto"),
	                                              tr("My Wrapped: badges, weekly chart, share as QR", "Mi resumen: logros, gráfico semanal, compartir en QR"),
	                                              tr("Equalizer and lyrics on the audio-only screen", "Ecualizador y letras en la pantalla de solo audio"),
	                                              tr("Safe mode if the app fails to start", "Modo seguro si la app falla al arrancar"),
	                                              tr("Details: Documentation/GUIA_DE_USO.md", "Detalles: Documentation/GUIA_DE_USO.md")})
	    ->update_y_range(0, DEFAULT_FONT_INTERVAL * 7 + 6);
	dialog_view->set_buttons<std::function<std::string()>>({[]() { return LOCALIZED(OK); }}, [](OverlayDialogView &, int) {
		menu_status = CLOSED;
		whats_new_done();
		var_need_refresh = true;
		return true;
	});
	dialog_view->set_on_cancel([](OverlayView &view) {
		menu_status = CLOSED;
		whats_new_done();
		view.set_is_visible(false);
		var_need_refresh = true;
	});
	dialog_view->set_is_visible(true);
	menu_status = CONFIRMING_CLOSE;
	var_need_refresh = true;
}

// the first-use tour : a dialog with the main things of the app, shown once on the Home screen
static void show_first_use_tour() {
	dialog_view->get_message_view()
	    ->set_text_lines(std::vector<std::string>{tr("Welcome to Iker's Tube!", "¡Bienvenido a Iker's Tube!"),
	                                              tr("Bottom bar: Home, Search, Library,", "Barra de abajo: Inicio, Buscar, Biblioteca,"),
	                                              tr("Downloads and More (settings, menu).", "Descargas y Más (ajustes, menú)."),
	                                              tr("Long press a video for its menu.", "Mantén pulsado un vídeo: su menú."),
	                                              tr("In a video: Save video / Save audio", "En un vídeo: Guardar vídeo / audio"),
	                                              tr("to watch it without internet.", "para verlo sin internet."),
	                                              tr("The Studio tab is for your channel.", "La pestaña Studio es para tu canal.")})
	    ->update_y_range(0, DEFAULT_FONT_INTERVAL * 7 + 6);
	dialog_view->set_buttons<std::function<std::string()>>({[]() { return LOCALIZED(OK); }}, [](OverlayDialogView &, int) {
		menu_status = CLOSED;
		var_tour_done = true;
		whats_new_done(); // (a new user does not need the news of this version)
		misc_tasks_request(TASK_SAVE_SETTINGS);
		var_need_refresh = true;
		return true;
	});
	dialog_view->set_on_cancel([](OverlayView &view) {
		menu_status = CLOSED;
		var_tour_done = true;
		whats_new_done();
		misc_tasks_request(TASK_SAVE_SETTINGS);
		view.set_is_visible(false);
		var_need_refresh = true;
	});
	dialog_view->set_is_visible(true);
	menu_status = CONFIRMING_CLOSE;
	var_need_refresh = true;
}

void update_overlay_menu(Hid_info *key) {
	static bool exit_confirmed = false;
	if (menu_next_request) {
		menu_next_request = false;
		show_menu_rows(menu_next_title, menu_next_options, menu_next_first);
	}
	if (waiting_account_playlists && osGetTime() - waiting_account_playlists_since > 20000) { // (nothing came : stop waiting, and say so)
		waiting_account_playlists = false;
		notice_show(tr("Could not load your playlists", "No se pudieron cargar tus listas"));
	}
	if (waiting_account_playlists) {
		std::string playlists_video;
		std::vector<AccountPlaylist> lists;
		if (account_playlists_take(playlists_video, lists)) {
			waiting_account_playlists = false;
			std::vector<std::pair<std::string, std::function<void()>>> options;
			for (auto &pl : lists) {
				std::string playlist_id = pl.id;
				bool has = pl.has_video;
				options.push_back({(has ? "[x] " : "[ ] ") + truncate_first_line(pl.title, 200, 0.5), [playlist_id, playlists_video, has]() { account_playlist_edit_async(playlist_id, playlists_video, !has); }}); // (cut by width : a cut by bytes could split a character)
				if (options.size() >= 6) break; // (the menu has to fit on the screen)
			}
			show_menu_rows(tr("Add to a playlist", "Añadir a una lista"), options);
		}
	}
	if (video_menu && video_menu->is_visible) { // the video menu takes all the input while it is open
		video_menu->update(*key);
		key->touch_x = key->touch_y = -1;
		key->p_touch = false;
		key->p_a = key->p_b = key->p_x = key->p_y = false;
		var_need_refresh = true;
		return;
	}
	{ // the tour comes after the Home screen has been there for a moment (the very first start goes to Settings first)
		static int home_frames = 0;
		if (!var_tour_done && nav_enabled && global_current_scene == SceneType::HOME && menu_status == CLOSED) {
			if (++home_frames > 60) {
				home_frames = 0;
				show_first_use_tour();
			}
		} else if (var_tour_done && nav_enabled && global_current_scene == SceneType::HOME && menu_status == CLOSED && whats_new_pending()) {
			if (++home_frames > 90) {
				home_frames = 0;
				show_whats_new();
			}
		} else {
			home_frames = 0;
		}
	}
	static SceneType prev_scene = SceneType::SEARCH;
	if (prev_scene != global_current_scene) {
		var_need_refresh = true;
		prev_scene = global_current_scene;
	}
	contents.clear();
	if (var_update_banner && global_current_scene != SceneType::SETTINGS) {
		contents.push_back({var_update_banner, Content::Type::SETTINGS}); // "Actualizando..." / "Actualizado: reinicia la app"
	} else if (var_update_available && global_current_scene != SceneType::SETTINGS) {
		// takes the user straight to Settings where the "Update" button is
		contents.push_back({LOCALIZED(UPDATES_AVAILABLE), Content::Type::SETTINGS});
	}
	if (var_new_feed_videos > 0 && global_current_scene != SceneType::HOME) {
		contents.push_back({std::to_string(var_new_feed_videos) + " " + tr("new videos", "vídeos nuevos"), Content::Type::HOME});
	}
	if (!nav_enabled && global_current_scene != SceneType::HOME) {
		contents.push_back({LOCALIZED(HOME), Content::Type::HOME});
	}
	if (!nav_enabled && global_current_scene != SceneType::SEARCH) {
		contents.push_back({LOCALIZED(GOTO_SEARCH), Content::Type::SEARCH});
	}
	if (!nav_enabled && global_current_scene != SceneType::LIBRARY) {
		int queued = play_queue_size();
		contents.push_back({queued ? tr("Library", "Biblioteca") + " (" + std::to_string(queued) + ")" : tr("Library", "Biblioteca"),
		                    Content::Type::LIBRARY});
	}
	if (global_current_scene != SceneType::HISTORY) {
		contents.push_back({LOCALIZED(WATCH_HISTORY), Content::Type::HISTORY});
	}
	contents.push_back({var_incognito ? tr("Incognito: on", "Incógnito: activado") : tr("Incognito: off", "Incógnito: desactivado"), Content::Type::INCOGNITO});
	contents.push_back({LOCALIZED(EXIT_APP), Content::Type::EXIT});
	if (global_current_scene != SceneType::SETTINGS) {
		contents.push_back({LOCALIZED(SETTINGS), Content::Type::SETTINGS});
	}
	if (global_current_scene != SceneType::ABOUT) {
		contents.push_back({LOCALIZED(ABOUT), Content::Type::ABOUT});
	}

	if (menu_status == CONFIRMING_CLOSE) {
		holding_touch = false;
		var_need_refresh = true;

		dialog_view->update(*key);

		key->touch_x = key->touch_y = -1;
		key->p_touch = false;
	} else {
	// where a touch lands : the menu icon, an item of the bar or a row of the menu (-1 : nowhere). A tap counts only when the finger
	// comes up on the same place where it went down (dragging from Home to More no longer opens More).
	static int press_zone = -1;
	auto zone_at = [](int x, int y) -> int {
		if (in_icon(x, y)) return 1000;
		if (nav_at(x, y) != -1) return nav_at(x, y);
		if (menu_status != CLOSED && content_at(x, y) != -1) return 100 + content_at(x, y);
		return -1;
	};
	if (holding_touch && key->touch_x == -1 && last_touch_x != -1) {
	if (zone_at(last_touch_x, last_touch_y) != press_zone) {
		// released somewhere else : cancelled
	} else if (in_icon(last_touch_x, last_touch_y)) {
				if (menu_status == CLOSED) {
					menu_status = OPEN;
				} else {
					menu_status = CLOSED;
				}
			} else if (nav_at(last_touch_x, last_touch_y) != -1) { // an item of the navigation bar
				int item = nav_at(last_touch_x, last_touch_y);
				menu_status = CLOSED;
				global_intent_is_nav = true;
				if (item == 0) {
					global_intent.next_scene = SceneType::HOME;
					global_intent.arg = "";
				} else if (item == 1) {
					global_intent.next_scene = SceneType::SEARCH;
					global_intent.arg = "";
				} else if (item == 2) {
					global_intent.next_scene = SceneType::LIBRARY;
					global_intent.arg = Library_is_offline_mode() ? "queue" : "";
				} else if (item == 3) {
					global_intent.next_scene = SceneType::LIBRARY;
					global_intent.arg = "offline";
				}
			} else if (menu_status != CLOSED && content_at(last_touch_x, last_touch_y) != -1) {
				int id = content_at(last_touch_x, last_touch_y);
				if (contents[id].type == Content::Type::SEARCH) {
					global_intent.next_scene = SceneType::SEARCH;
					global_intent.arg = "";
				} else if (contents[id].type == Content::Type::EXIT) {
					dialog_view->get_message_view()
					    ->set_text([]() { return LOCALIZED(EXIT_CONFIRM); })
					    ->update_y_range(0, 30);
					dialog_view->set_buttons<std::function<std::string()>>(
					    {[]() { return LOCALIZED(CANCEL); }, []() { return LOCALIZED(EXIT_APP); }},
					    [](OverlayDialogView &, int button_pressed) {
						    if (button_pressed == 0) {
							    menu_status = OPEN;
						    } else if (button_pressed == 1) {
							    exit_confirmed = true;
						    }
						    return true; // close
					    });
					dialog_view->set_is_visible(true);
					dialog_view->set_on_cancel([](OverlayView &view) {
						menu_status = OPEN;
						view.set_is_visible(false);
						var_need_refresh = true;
					});
					var_need_refresh = true;
					menu_status = CONFIRMING_CLOSE;
				} else if (contents[id].type == Content::Type::INCOGNITO) {
					var_incognito = !var_incognito;
					misc_tasks_request(TASK_SAVE_SETTINGS);
					notice_show(var_incognito ? tr("Incognito on: nothing is remembered", "Incógnito activado: no se recuerda nada")
					                          : tr("Incognito off", "Incógnito desactivado"));
				} else if (contents[id].type == Content::Type::ABOUT) {
					global_intent.next_scene = SceneType::ABOUT;
					global_intent.arg = "";
				} else if (contents[id].type == Content::Type::SETTINGS) {
					global_intent.next_scene = SceneType::SETTINGS;
					global_intent.arg = "";
				} else if (contents[id].type == Content::Type::LIBRARY) {
					global_intent.next_scene = SceneType::LIBRARY;
					global_intent.arg = "";
				} else if (contents[id].type == Content::Type::HISTORY) {
					global_intent.next_scene = SceneType::HISTORY;
					global_intent.arg = "";
				} else if (contents[id].type == Content::Type::HOME) {
					global_intent.next_scene = SceneType::HOME;
					global_intent.arg = "";
				}
			} else {
				menu_status = CLOSED;
			}
			var_need_refresh = true;
		}

		if (holding_touch && last_touch_x != -1 && key->touch_x == -1) {
			var_need_refresh = true;
		}

		if (key->p_touch) {
			if (in_icon(key->touch_x, key->touch_y)) {
			holding_touch = true;
			press_zone = zone_at(key->touch_x, key->touch_y);
			} else if (nav_at(key->touch_x, key->touch_y) != -1) {
			holding_touch = true;
			press_zone = zone_at(key->touch_x, key->touch_y);
			} else if (menu_status != CLOSED && content_at(key->touch_x, key->touch_y) != -1) {
			holding_touch = true;
			press_zone = zone_at(key->touch_x, key->touch_y);
			} else {
				menu_status = CLOSED;
			}
		}

		last_touch_x = key->touch_x;
		last_touch_y = key->touch_y;

		if (key->touch_x == -1) {
			holding_touch = false;
		}
		if (holding_touch) {
			key->touch_x = key->touch_y = -1;
			key->p_touch = false;
		}
	}
	if (exit_confirmed) {
		exit_confirmed = false;
		global_intent.next_scene = SceneType::EXIT;
		global_intent.arg = "";
	}
}
void close_overlay_menu() {
	menu_status = CLOSED;
	var_need_refresh = true;
}
void overlay_menu_on_resume() {
	nav_enabled = false; // scenes that have the navigation bar turn it on again every frame
	last_touch_x = -1;
	last_touch_y = -1;
	menu_status = CLOSED;
	holding_touch = false;
	var_need_refresh = true;
}
