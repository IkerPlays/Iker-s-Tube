#include "ui/ui.hpp"
#include "ui/nav_draw.hpp"
#include <cstdio>
void rec_begin(const char *path, int w, int h);
void rec_end();
static std::vector<std::string> L(const std::string &t, int n = 2) { return truncate_str(t, VIDEO_LIST_TEXT_WIDTH, n, 0.5, 0.5); }
static TextView *button(const std::string &text, int w) {
	TextView *b = new TextView(0, 0, w, 22);
	b->set_text(text)->set_x_alignment(TextView::XAlign::CENTER)->set_y_alignment(TextView::YAlign::CENTER)->set_get_background_color(View::STANDARD_BACKGROUND)->set_corner_radius(6)->set_on_view_released([](View &) {});
	return b;
}
static void scene_home(const char *path, bool night) {
	var_night_mode = night;
	rec_begin(path, 320, 240);
	Draw_texture(var_square_image[0], DEFAULT_BACK_COLOR, 0, 0, 320, 240);
	VerticalListView *list = (new VerticalListView(0, 0, 320))->set_margin(SMALL_MARGIN);
	const char *titles[] = {"Lofi hip hop radio - beats to relax/study to", "How the 3DS works (and why it still rocks in 2026)", "NCS: 10 Billion Views Mix - Copyright Free Music", "Top 10 Most Popular Songs by NoCopyrightSounds"};
	const char *chans[] = {"Lofi Girl", "Retro Deep Dive", "NoCopyrightSounds", "NCS Mix"};
	const char *lens[] = {"LIVE", "18:42", "1:02:33", "9:15"};
	for (int i = 0; i < 4; i++) {
		SuccinctVideoView *v = new SuccinctVideoView(0, 0, 320, VIDEO_LIST_THUMBNAIL_HEIGHT);
		v->thumbnail_handle = 100 + i; v->thumbnail_fade = 99; v->set_title_lines(L(titles[i]))->set_auxiliary_lines({chans[i], "1.2M views - 3 days ago"})->set_bottom_right_overlay(lens[i])->set_thumbnail_url("x" + std::to_string(i));
		v->set_get_background_color(View::STANDARD_BACKGROUND)->set_on_view_released([](View &) {});
		list->views.push_back(v);
	}
	ScrollView *scroll = (new ScrollView(0, 0, 320, 190))->set_views({list});
	TabView *tabs = (new TabView(0, 0, 320, 210))->set_views({scroll, new EmptyView(0, 0, 320, 0)})->set_tab_texts<std::string>({"Home", "Feed"});
	tabs->draw();
	NavPaint np; np.top = 210; np.selected = 0; np.queue_size = 3; np.download_running = true; np.incognito = false;
	const std::string labels[5] = {"Home", "Search", "Library", "Downloads", "More"};
	nav_paint(np, labels);
	rec_end();
}
static void scene_settings(const char *path, bool night) {
	var_night_mode = night;
	rec_begin(path, 320, 240);
	Draw_texture(var_square_image[0], DEFAULT_BACK_COLOR, 0, 0, 320, 240);
	VerticalListView *list = (new VerticalListView(0, 0, 320))->set_margin(SMALL_MARGIN);
	list->views.push_back((new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))->set_text("Appearance")->set_font_size(0.6, DEFAULT_FONT_INTERVAL)->set_get_text_color([]() { return (u32)COLOR_ACCENT; }));
	list->views.push_back((new SelectorView(0, 0, 320, 35, true))->set_texts({(std::function<std::string()>)[]() { return std::string("Off"); }, (std::function<std::string()>)[]() { return std::string("On"); }}, 1)->set_title([](const SelectorView &) { return std::string("Rounded buttons"); }));
	list->views.push_back((new SelectorView(0, 0, 320, 35, true))->set_texts({(std::function<std::string()>)[]() { return std::string("Small"); }, (std::function<std::string()>)[]() { return std::string("Normal"); }, (std::function<std::string()>)[]() { return std::string("Large"); }}, 1)->set_title([](const SelectorView &) { return std::string("Text size"); }));
	list->views.push_back(button("Export backup to the SD card", 220));
	{ static double v = 6.0; list->views.push_back((new BarView(0, 0, 320, 35))->set_values_sync(1.0, 15.0, &v)->set_title([](const BarView &) { return std::string("Scroll speed : 6.0"); })); }
	ScrollView *scroll = (new ScrollView(0, 0, 320, 200))->set_views({list});
	TabView *tabs = (new TabView(0, 0, 320, 240))->set_views({scroll, new EmptyView(0, 0, 320, 0), new EmptyView(0, 0, 320, 0), new EmptyView(0, 0, 320, 0)})->set_tab_texts<std::string>({"Display", "Playback", "Data", "Update"});
	tabs->draw();
	rec_end();
}
static void scene_dialog(const char *path, bool night) {
	var_night_mode = night;
	scene_home(path, night);
	rec_begin(path, 320, 240);
	var_night_mode = night;
	Draw_texture(var_square_image[0], DEFAULT_BACK_COLOR, 0, 0, 320, 240);
	OverlayDialogView *d = new OverlayDialogView(0, 0, 320, 240);
	d->get_message_view()->set_text_lines(std::vector<std::string>{"Delete all downloads?", "This cannot be undone."})->update_y_range(0, 40);
	d->set_buttons<std::string>({"Cancel", "Delete"}, [](OverlayDialogView &, int) { return true; });
	d->draw();
	rec_end();
}
static void scene_menu(const char *path, bool night) {
	var_night_mode = night;
	rec_begin(path, 320, 240);
	Draw_texture(var_square_image[0], DEFAULT_BACK_COLOR, 0, 0, 320, 240);
	OverlayView *m = new OverlayView(0, 0, 320, 240);
	std::vector<View *> rows;
	rows.push_back((new TextView(0, 0, 250, 22))->set_text("Lofi hip hop radio - beats to relax")->set_x_alignment(TextView::XAlign::CENTER)->set_y_alignment(TextView::YAlign::CENTER)->set_get_text_color([]() { return (u32)COLOR_ACCENT; })->set_get_background_color([](const View &) { return (u32)LIGHT0_BACK_COLOR; }));
	for (const char *t : {"Play next", "Add to queue", "Watch later", "+ Favorite", "Save video offline", "More options...", "Cancel"}) {
		TextView *r = new TextView(0, 0, 250, 24);
		r->set_text(t)->set_x_alignment(TextView::XAlign::CENTER)->set_y_alignment(TextView::YAlign::CENTER)->set_get_background_color(View::STANDARD_BACKGROUND)->set_on_view_released([](View &) {});
		rows.push_back(r);
	}
	m->set_subview((new VerticalListView(0, 0, 250))->set_views(rows)->set_get_background_color([](const View &) { return (u32)UI_PAGE_COLOR; })->set_corner_radius(10));
	m->draw();
	rec_end();
}
static void scene_channels(const char *path, bool night) {
	var_night_mode = night;
	rec_begin(path, 320, 240);
	Draw_texture(var_square_image[0], DEFAULT_BACK_COLOR, 0, 0, 320, 240);
	ChannelView *cv = new ChannelView(0, 0, 320, 64);
	cv->icon_handle = 7;
	cv->set_name("Lofi Girl")->set_handle("@LofiGirl")->set_subscriber_count("13.4M subscribers")->set_get_is_subscribed([]() { return false; });
	cv->draw();
	VerticalListView *list = (new VerticalListView(0, 70, 320))->set_margin(SMALL_MARGIN);
	const char *names[] = {"Retro Deep Dive", "NoCopyrightSounds", "Nintendo Life"};
	for (int i = 0; i < 3; i++) {
		SuccinctChannelView *c = new SuccinctChannelView(0, 0, 320, CHANNEL_ICON_HEIGHT);
		c->thumbnail_handle = 20 + i;
		c->set_name(names[i])->set_auxiliary_lines({"1.2M subscribers"});
		c->set_get_background_color(View::STANDARD_BACKGROUND)->set_on_view_released([](View &) {});
		list->views.push_back(c);
	}
	list->draw(0, 0);
	rec_end();
}
static void scene_player_info(const char *path, bool night) {
	var_night_mode = night;
	rec_begin(path, 320, 240);
	Draw_texture(var_square_image[0], DEFAULT_BACK_COLOR, 0, 0, 320, 240);
	VerticalListView *list = (new VerticalListView(0, 0, 320))->set_margin(SMALL_MARGIN);
	list->views.push_back((new TextView(0, 0, 320, 30))->set_text_lines(std::vector<std::string>{"Lofi hip hop radio - beats to", "relax/study to"})->set_font_size(0.55, 14));
	list->views.push_back((new TextView(0, 0, 320, 14))->set_text("Lofi Girl  -  13M subscribers  -  1.2M views")->set_font_size(0.45, 13)->set_get_text_color([]() { return (u32)LIGHT1_TEXT_COLOR; }));
	auto tool = [](const std::string &t, int w) { return (new TextView(0, 0, w, 20))->set_text(t)->set_x_alignment(TextView::XAlign::CENTER)->set_get_background_color(View::STANDARD_BACKGROUND)->set_on_view_released([](View &) {}); };
	list->views.push_back((new HorizontalListView(0, 0, 20))->set_views({new EmptyView(0, 0, 3, 20), tool("Play", 75), tool("Reload", 75), tool("+ Favorite", 85), tool("QR", 50)}));
	list->views.push_back((new HorizontalListView(0, 0, 20))->set_views({new EmptyView(0, 0, 3, 20), tool("-10 s", 70), tool("+10 s", 70), tool("A-B", 100)}));
	list->views.push_back((new HorizontalListView(0, 0, 20))->set_views({new EmptyView(0, 0, 3, 20), tool("1.0x", 60), tool("Chap >>", 70), tool("Sleep", 70), tool("Mute", 60)}));
	list->views.push_back((new RuleView(0, 0, 320, 6)));
	list->views.push_back((new TextView(0, 0, 320, 40))->set_text_lines(std::vector<std::string>{"Description: study beats 24/7. Thanks for listening", "and keep the vibes going."})->set_font_size(0.45, 13)->set_get_text_color([]() { return (u32)LIGHT0_TEXT_COLOR; }));
	ScrollView *scroll = (new ScrollView(0, 0, 320, 190))->set_views({list});
	TabView *tabs = (new TabView(0, 0, 320, 220))->set_views({scroll, new EmptyView(0, 0, 320, 0), new EmptyView(0, 0, 320, 0)})->set_tab_texts<std::string>({"Info", "Comments", "Suggested"});
	tabs->draw();
	rec_end();
}
int main() {
	var_round_buttons = 1;
	var_square_image[0].tex = (void *)1;
	scene_home("out/home_dark.json", true);
	scene_home("out/home_light.json", false);
	scene_settings("out/set_dark.json", true);
	scene_settings("out/set_light.json", false);
	var_theme = 4; scene_home("out/home_amber.json", true); scene_settings("out/set_amber.json", true); var_theme = 0;
	scene_dialog("out/dialog_dark.json", true);
	scene_menu("out/menu_dark.json", true);
	scene_player_info("out/info_dark.json", true);
	scene_channels("out/chan_dark.json", true);
	scene_menu("out/menu_light.json", false);
	return 0;
}
