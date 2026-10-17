#include "headers.hpp"
#include <vector>
#include <numeric>
#include <math.h>
#include <regex>

#include "scenes/video_player.hpp"
#include "util/play_queue.hpp"
#include "data_io/favorites.hpp"
#include "ui/qr_popup.hpp"
#include "data_io/bookmarks.hpp"
#include "scenes/account_tasks.hpp"
#include "scenes/offline_tasks.hpp"
#include "youtube_parser/sponsorblock.hpp"
#include "oauth/oauth.hpp"
#include "data_io/offline.hpp"
#include "ui/notice.hpp"
#include "ui/ui.hpp"
#include "data_io/channel_prefs.hpp"
#include "data_io/last_played.hpp"
#include "data_io/channel_volume.hpp"
#include "data_io/watch_later.hpp"
#include "youtube_parser/live_chat.hpp"
#include <sstream>
#include "util/timestamp_parser.hpp"
#include "ui/logo_draw.hpp"
#include "data_io/resume_position.hpp"
#include "scenes/home.hpp"
#include "ui/overlay.hpp"
#include "ui/ui.hpp"
#include "network_decoder/network_io.hpp"
#include "network_decoder/network_decoder_multiple.hpp"
#include "network_decoder/thumbnail_loader.hpp"
#include "util/async_task.hpp"
#include "util/misc_tasks.hpp"
#include "util/util.hpp"
#include "data_io/subscription_util.hpp"

#define ICON_SIZE 55
#define TAB_SELECTOR_HEIGHT 20
#define TAB_SELECTOR_SELECTED_LINE_HEIGHT 3
#define SUGGESTIONS_VERTICAL_INTERVAL (VIDEO_LIST_THUMBNAIL_HEIGHT + SMALL_MARGIN)
#define SUGGESTION_LOAD_MORE_MARGIN 30
#define COMMENT_LOAD_MORE_MARGIN 30
#define CONTROL_BUTTON_HEIGHT 20
#define EQUALIZER_POPUP_WIDTH 280
#define EQUALIZER_POPUP_HEIGHT (240 - VIDEO_PLAYING_BAR_HEIGHT)

#define MAX_THUMBNAIL_LOAD_REQUEST 12
#define MAX_COMMENT_ICON_LOAD_REQUEST 18
#define MAX_RETRY_CNT 2 // 3 trials as a total

#define TAB_GENERAL 0
#define TAB_COMMENTS 1
#define TAB_SUGGESTIONS 2
#define TAB_CAPTIONS 3
#define TAB_PLAYBACK 4
#define TAB_PLAYLIST 5

#define TAB_MAX_NUM 6

namespace VideoPlayer {
bool vid_main_run = false;
bool vid_thread_run = false;
bool vid_already_init = false;
bool vid_thread_suspend = true;
volatile bool should_suspend_decoding = false;
volatile bool vid_play_request = false;
volatile bool vid_seek_request = false;
volatile bool vid_change_video_request = false;
volatile bool vid_pausing = false;
volatile bool vid_pausing_seek = false;
volatile bool eof_reached = false;
volatile bool audio_only_mode = false;
volatile bool video_skip_drawing = false; // for performance reason, enabled when opening keyboard
volatile int video_p_value = 360;
volatile double seek_at_init_request = -1;
static char vid_resume_id[16] = {0}; // id of the playing video, readable from the converter thread without a lock
double vid_time[2][320];
double vid_copy_time[2] = {
    0,
    0,
};
double vid_audio_time = 0;
double vid_video_time = 0;
double vid_convert_time = 0;
double vid_frametime = 0;
double vid_framerate = 0;
int vid_sample_rate = 0;
double vid_duration = 0;
double vid_zoom = 1;
double vid_x = 0;
double vid_y = 15;
double vid_current_pos = 0;
volatile double vid_seek_pos = 0;
double vid_min_time = 0;
double vid_max_time = 0;
double vid_total_time = 0;
double vid_recent_time[90];
double vid_recent_total_time = 0;
int vid_total_frames = 0;
int vid_width = 0;
int vid_width_org = 0;
int vid_height = 0;
int vid_height_org = 0;
int vid_tex_width[2] = {0, 0};
int vid_tex_height[2] = {0, 0};
int texture_index_head = 0; // write the texture alternately to avoid scattering
std::string cur_displaying_url = "";
std::string cur_playing_url = "";
std::string vid_video_format = "n/a";
std::string vid_audio_format = "n/a";
Image_data vid_image[2];
int icon_thumbnail_handle = -1;
C2D_Image vid_banner[2];
C2D_Image vid_control[2];
C2D_Image play_button_texture[2];
Thread vid_decode_thread, vid_convert_thread;

constexpr int CONTENT_Y_HIGH =
    240 - TAB_SELECTOR_HEIGHT - VIDEO_PLAYING_BAR_HEIGHT; // the bar is always shown here, so this is a constant

Thread stream_downloader_thread;
NetworkStreamDownloader stream_downloader;

Thread livestream_initer_thread;
NetworkMultipleDecoder network_decoder;
Mutex network_decoder_critical_lock; // locked when seeking or deiniting

Mutex small_resource_lock; // locking basically all std::vector, std::string, etc
YouTubeVideoDetail cur_video_info;
YouTubeVideoDetail playing_video_info;
std::map<std::string, YouTubeVideoDetail> video_info_cache;
int video_retry_left = 0;

std::set<PostView *> comment_thumbnail_loaded_list;

std::string channel_id_pressed;
std::string suggestion_clicked_url; // also used for playlist

TabView *main_view = NULL;

// main tab
ScrollView *main_tab_view = (new ScrollView(0, 0, 320, CONTENT_Y_HIGH));
ImageView *main_icon_view = (new ImageView(0, 0, ICON_SIZE, ICON_SIZE));

// suggestion tab
VerticalListView *suggestion_main_view =
    (new VerticalListView(0, 0, 320))
        ->set_margin(SMALL_MARGIN)
        ->enable_thumbnail_request_update(MAX_THUMBNAIL_LOAD_REQUEST, SceneType::VIDEO_PLAYER);
View *suggestion_bottom_view = new EmptyView(0, 0, 320, 0);
ScrollView *suggestion_tab_view;

// comment tab
static void comment_sort_clicked();
static bool comment_write_request = false; // set by the "+ Comment" button, handled after the view update
View *comments_top_view = (new HorizontalListView(0, 0, 20))
                              ->set_views({(new TextView(0, 0, 200, 20))
                                               ->set_text((std::function<std::string()>)[]() -> std::string {
	                                               if (cur_video_info.comment_sort_tokens[0] == "" && cur_video_info.comment_sort_tokens[1] == "") {
		                                               return "";
	                                               }
	                                               return tr("Sort: ", "Orden: ") + (cur_video_info.comment_sort_type == 0 ? tr("Top", "Populares") : tr("Newest", "Recientes")) + "  >";
                                               })
                                               ->set_x_alignment(TextView::XAlign::CENTER)
                                               ->set_y_alignment(TextView::YAlign::CENTER)
                                               ->set_get_background_color(View::STANDARD_BACKGROUND)
                                               ->set_on_view_released([](View &) { comment_sort_clicked(); }),
                                           (new TextView(0, 0, 120, 20))
                                               ->set_text((std::function<std::string()>)[]() -> std::string { return tr("+ Comment", "+ Comentar"); })
                                               ->set_x_alignment(TextView::XAlign::CENTER)
                                               ->set_y_alignment(TextView::YAlign::CENTER)
                                               ->set_get_background_color(View::STANDARD_BACKGROUND)
                                               ->set_on_view_released([](View &) { comment_write_request = true; })});
VerticalListView *comments_main_view = new VerticalListView(0, 0, 320);
View *comments_bottom_view = new EmptyView(0, 0, 320, 0);
ScrollView *comment_tab_view = NULL;

// caption tab
ScrollView *caption_main_view;
VerticalListView *caption_language_select_view;
ViewReferenceView *captions_tab_view = new ViewReferenceView();
CaptionOverlayView *caption_overlay_view;

// list tab
constexpr int PLAYLIST_TOP_HEIGHT = 30;
VerticalListView *playlist_tab_view;
ScrollView *playlist_list_view;
VerticalListView *playlist_list_videos_view;
TextView *playlist_title_view;
TextView *playlist_author_view;

// playback tab
SuccinctVideoView *cur_playing_video_view;
CustomView *download_progress_view = NULL;
SelectorView *video_quality_selector_view;
SelectorView *video_loop_view;
SelectorView *video_3d_selector_view;
VerticalListView *debug_info_view = NULL;
ScrollView *playback_tab_view = NULL;

OverlayView *equalizer_popup_view = (new OverlayView(0, 0, 320, 240 - VIDEO_PLAYING_BAR_HEIGHT));
}; // namespace VideoPlayer
using namespace VideoPlayer;

static const char *volatile network_waiting_status = NULL;
const char *get_network_waiting_status() {
	if (network_waiting_status) {
		return network_waiting_status;
	}
	return network_decoder.get_network_waiting_status();
}

void video_set_should_suspend_decoding(bool should_suspend) { should_suspend_decoding = should_suspend; }

static void send_seek_request_wo_lock(double pos);
static void send_seek_request(double pos);
static void send_change_video_request(std::string url, bool update_player, bool update_view, bool force_load);
static void send_change_video_request_wo_lock(std::string url, bool update_player, bool update_view, bool force_load);

static void load_video_page(void *);
static void load_more_comments(void *);
static void change_comment_sort(void *);
namespace VideoPlayer {
static void comment_sort_clicked() {
	if (!is_async_task_running(load_video_page) && !is_async_task_running(load_more_comments) &&
	    !is_async_task_running(change_comment_sort)) {
		queue_async_task(change_comment_sort, NULL);
	}
}
} // namespace VideoPlayer
static void load_more_suggestions(void *);
static void load_more_replies(void *);
static void load_caption(void *);

static bool net_waiting_snapshot = false; // get_network_waiting_status() taken outside small_resource_lock (lock order)

// Automatic quality : if playback stalls repeatedly the quality cap for the next videos goes one step down,
// after a few minutes of smooth playback it goes one step up again.
static void auto_quality_tick() {
	static const int steps[4] = {144, 240, 360, 480};
	static std::string tracked_id;
	static time_t smooth_since = 0;
	static time_t stall_since = 0;
	static bool lowered_for_this_video = false;
	if (!var_auto_quality || var_video_quality == 0 || audio_only_mode) {
		return;
	}
	if (playing_video_info.id != tracked_id) {
		tracked_id = playing_video_info.id;
		smooth_since = time(NULL);
		stall_since = 0;
		lowered_for_this_video = false;
	}
	time_t now = time(NULL);
	bool waiting = net_waiting_snapshot; // read before the lock : get_network_waiting_status() takes the decoder lock
	if (!vid_play_request || vid_pausing || !waiting) {
		stall_since = 0;
		if (smooth_since && now - smooth_since >= 240 && var_auto_quality_cap > 0) {
			// 4 minutes without trouble : allow one step more
			int idx = 0;
			for (int i = 0; i < 4; i++) {
				if (steps[i] <= var_auto_quality_cap) idx = i;
			}
			var_auto_quality_cap = idx >= 3 ? 0 : steps[idx + 1];
			if (var_auto_quality_cap >= var_video_quality) var_auto_quality_cap = 0;
			misc_tasks_request(TASK_SAVE_SETTINGS);
			smooth_since = now;
		}
		return;
	}
	if (!stall_since) stall_since = now;
	if (now - stall_since >= 6 && !lowered_for_this_video && now - smooth_since >= 12) {
		int cur = var_auto_quality_cap > 0 ? var_auto_quality_cap : var_video_quality;
		int idx = 0;
		for (int i = 0; i < 4; i++) {
			if (steps[i] <= cur) idx = i;
		}
		if (idx > 0) {
			var_auto_quality_cap = steps[idx - 1];
			misc_tasks_request(TASK_SAVE_SETTINGS);
		}
		lowered_for_this_video = true;
		smooth_since = now;
	}
}

static std::vector<std::pair<double, std::string>> current_chapters; // chapters of the displayed video (from its description)
static VerticalListView *bookmarks_list_view = NULL; // the list shown in the page; rebuilt outside of the view updates
static VerticalListView *pending_bookmarks_list_view = NULL; // created by the loader, becomes bookmarks_list_view when the page is installed
static bool bookmarks_rebuild_request = false, bookmark_add_request = false;
static std::string bookmarks_video_id;
static std::vector<std::pair<double, std::string>> pending_chapters; // same, built by the loader thread
static std::string pending_bookmarks_video_id;
static double quick_speed = 1.0;
static BarView *speed_bar_view = NULL;
static bool mini_visible = false;
static OverlayView *text_popup_view = NULL; // scrollable text (translations)
static OverlayView *account_pl_popup_view = NULL; // list of the playlists of the YouTube account
static OverlayDialogView *comment_dialog = NULL; // Like / Reply / Cancel for a comment
static std::string comment_menu_request; // id of the comment that was long pressed (handled after the view update)
static std::string comment_target_id; // comment chosen in the dialog
static bool comment_reply_request = false;
// the subtitle language of the last time (sdmc:/3ds/IkersTube/caption_lang.txt)
static std::string caption_lang_remembered() {
	char buf[32] = {0};
	FILE *f = fopen("sdmc:/3ds/IkersTube/caption_lang.txt", "r");
	if (!f) return "";
	if (!fgets(buf, sizeof buf, f)) buf[0] = 0;
	fclose(f);
	std::string s = buf;
	while (s.size() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
	return s;
}
static void caption_lang_remember(const std::string &id) {
	if (id == "" || id.size() > 30) return;
	FILE *f = fopen("sdmc:/3ds/IkersTube/caption_lang.txt", "w");
	if (!f) return;
	fprintf(f, "%s\n", id.c_str());
	fclose(f);
}

static std::string autoplay_pending_url; // the next video waits for the countdown (guarded by small_resource_lock)
static std::string autoplay_pending_for; // the id of the video that ended
static u64 autoplay_due_ms = 0;
static std::string pending_offline_delete; // a saved video that was watched to the end : deleted when another video starts
static bool stop_after_video = false; // do not continue with the queue / autoplay when the current video ends
static void mini_player_draw();
static bool mini_player_update(Hid_info key);

static double mini_volume = 1.0;
static void mini_volume_apply(double v) {
	mini_volume = v;
	network_decoder.set_preamp(v);
}
static double *eq_values_ptr = NULL; // the equalizer bar values (set up in VideoPlayer_init)

// A-B loop : repeat a section of the playing video (a < 0 : not set, b < 0 : only the start is set)
static double ab_loop_a = -1, ab_loop_b = -1;

static void rebuild_bookmarks_list() {
	if (!bookmarks_list_view) return;
	bookmarks_list_view->recursive_delete_subviews();
	auto marks = bookmarks_get(bookmarks_video_id);
	if (marks.empty()) return;
	bookmarks_list_view->views.push_back((new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL))
	                                         ->set_text(tr("Bookmarks (long press to delete)", "Marcadores (pulsación larga: borrar)"))
	                                         ->set_font_size(0.5, DEFAULT_FONT_INTERVAL)
	                                         ->set_get_text_color([]() { return LIGHT1_TEXT_COLOR; }));
	for (size_t i = 0; i < marks.size(); i++) {
		double t = marks[i].time;
		std::string id = bookmarks_video_id;
		bookmarks_list_view->views.push_back(
		    (new TextView(SMALL_MARGIN, 0, 320 - SMALL_MARGIN * 2, 22))
		        ->set_text(Util_convert_seconds_to_time(t) + "  " + marks[i].name)
		        ->set_font_size(0.5, DEFAULT_FONT_INTERVAL)
		        ->set_y_alignment(TextView::YAlign::CENTER)
		        ->set_get_background_color(View::STANDARD_BACKGROUND)
		        ->set_on_view_released([t](View &) {
			        if (network_decoder.ready && cur_video_info.id == playing_video_info.id) send_seek_request_wo_lock(t);
		        })
		        ->add_on_long_hold(35, [id, i](View &view) {
			        bookmark_remove(id, i);
			        view.reset_holding_status();
			        bookmarks_rebuild_request = true;
			        var_need_refresh = true;
		        }));
	}
	bookmarks_list_view->views.push_back((new RuleView(0, 0, 320, SMALL_MARGIN * 2))->set_get_color([]() { return DEF_DRAW_GRAY; }));
}

// applies the speed remembered for the channel of the playing video once its decoder is ready
static void channel_speed_tick() {
	static std::string applied_for;
	if (!network_decoder.ready || playing_video_info.id == "" || applied_for == playing_video_info.id) return;
	applied_for = playing_video_info.id;
	double volume = channel_volume_get(playing_video_info.author.id);
	if (volume <= 0) volume = 1.0;
	mini_volume_apply(volume);
	double speed = channel_speed_get(playing_video_info.author.id);
	if (speed <= 0) speed = 1.0;
	if (speed != quick_speed) {
		quick_speed = speed;
		network_decoder.set_tempo(quick_speed);
		if (speed_bar_view) speed_bar_view->set_value(std::min(1.5, quick_speed));
		var_need_refresh = true;
	}
}

// SponsorBlock : the marked parts of the playing video are skipped (setting in the playback options)
static Mutex sb_lock;
static std::vector<std::pair<double, double>> sb_segments;
static std::vector<bool> sb_done; // the segment was already skipped (it can be skipped again after seeking back)
static std::string sb_for_id;
static volatile bool sb_loaded = false; // the segments of sb_for_id were received (even if there are none)

static void sb_load_task(void *arg) {
	std::string *video_id = (std::string *)arg;
	std::vector<std::pair<double, double>> segments;
	bool ok = sponsorblock_load(*video_id, var_sponsorblock, segments);
	sb_lock.lock();
	if (sb_for_id == *video_id && ok) {
		sb_segments = segments;
		sb_done.assign(segments.size(), false);
		sb_loaded = true;
	}
	sb_lock.unlock();
	if (ok && segments.size()) logger.info("player", "SponsorBlock : " + std::to_string(segments.size()) + " segment(s)");
	delete video_id;
}

static void sponsorblock_tick(const std::string &id) {
	static std::string loaded_id;
	static int requested_level = 0;
	static u64 last_request_ms = 0;
	if (id != loaded_id) { // another video : forget the segments
		loaded_id = id;
		requested_level = 0;
		sb_lock.lock();
		sb_segments.clear();
		sb_done.clear();
		sb_for_id = id;
		sb_loaded = false;
		sb_lock.unlock();
	}
	// ask for the segments when needed : the setting was turned on or changed, or the last try failed (at most every 20 s)
	if (var_sponsorblock && id != "" && var_wifi_state == 2 && playing_video_info.url.compare(0, 8, "offline:") != 0 &&
	    !is_async_task_running(sb_load_task) && (requested_level != var_sponsorblock || (!sb_loaded && osGetTime() - last_request_ms > 20000))) {
		requested_level = var_sponsorblock;
		last_request_ms = osGetTime();
		queue_async_task(sb_load_task, new std::string(id));
	}
	if (!var_sponsorblock || !vid_play_request || vid_pausing) return;
	sb_lock.lock();
	for (size_t i = 0; i < sb_segments.size() && i < sb_done.size(); i++) {
		double start = sb_segments[i].first, stop = sb_segments[i].second;
		if (!sb_done[i] && vid_current_pos >= start && vid_current_pos < stop - 1) {
			sb_done[i] = true;
			send_seek_request_wo_lock(stop); // called while small_resource_lock is already held
			notice_show(tr("Skipped a marked part (SponsorBlock)", "Parte marcada saltada (SponsorBlock)"));
		} else if (sb_done[i] && vid_current_pos < start - 2) {
			sb_done[i] = false; // the user went back : it may be skipped again
		}
	}
	sb_lock.unlock();
}

static void ab_loop_tick() {
	sponsorblock_tick(playing_video_info.id);
	static std::string tracked_id;
	if (playing_video_info.id != tracked_id) {
		tracked_id = playing_video_info.id;
		ab_loop_a = ab_loop_b = -1;
		if (vid_play_request) last_played_set(playing_video_info.id, playing_video_info.title); // for "continue watching"
	}
	if (ab_loop_a >= 0 && ab_loop_b > ab_loop_a && vid_play_request && !vid_pausing && vid_current_pos >= ab_loop_b) {
		send_seek_request_wo_lock(ab_loop_a); // called while small_resource_lock is already held
	}
}


static void apply_eq_preset(const double (&gains)[18]) {
	for (int i = 0; i < 18; i++) {
		if (eq_values_ptr) eq_values_ptr[i] = gains[i];
		network_decoder.set_equalizer_value(i, gains[i]);
	}
	var_need_refresh = true;
}

static std::vector<View *> make_eq_preset_buttons() {
	static const double flat[18] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
	static const double bass[18] = {1.8, 1.7, 1.6, 1.5, 1.3, 1.15, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
	static const double voice[18] = {0.5, 0.55, 0.65, 0.8, 1, 1.2, 1.4, 1.5, 1.5, 1.4, 1.3, 1.1, 1, 0.8, 0.7, 0.6, 0.5, 0.5};
	static const double treble[18] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1.1, 1.25, 1.4, 1.55, 1.7, 1.75, 1.75, 1.7, 1.6};
	auto button = [](std::function<std::string()> name, const double *gains) {
		return (new TextView(0, 0, EQUALIZER_POPUP_WIDTH / 4, 19))
		    ->set_text(name)
		    ->set_x_alignment(TextView::XAlign::CENTER)
		    ->set_get_background_color(View::STANDARD_BACKGROUND)
		    ->set_on_view_released([gains](View &) {
			    double copy[18];
			    for (int i = 0; i < 18; i++) copy[i] = gains[i];
			    apply_eq_preset(reinterpret_cast<const double(&)[18]>(copy));
		    });
	};
	return {button([]() { return tr("Flat", "Plano"); }, flat), button([]() { return std::string("Bass"); }, bass),
	        button([]() { return tr("Voice", "Voz"); }, voice), button([]() { return std::string("Treble"); }, treble)};
}

// builds the video info of an offline item (its audio is read from the SD card through the "local:" stream prefix)
static YouTubeVideoDetail offline_video_info(const std::string &id) {
	YouTubeVideoDetail info;
	OfflineItem item;
	info.id = id;
	info.url = "offline:" + id;
	if (!offline_get(id, item)) {
		info.error = "offline item not found";
		return info;
	}
	info.title = item.title;
	info.author.name = item.author;
	info.playability_status = "OK";
	if (item.quality > 0) { // saved video : played from the SD card like a normal one
		std::string video = "local:" + offline_video_path(id);
		info.video_stream_urls[item.quality] = video;
		if (item.combined) {
			info.both_stream_url = video; // video and sound in one file
		} else {
			info.audio_stream_url = "local:" + offline_file_path(id);
		}
	} else {
		info.audio_stream_url = "local:" + offline_file_path(id);
		info.both_stream_url = info.audio_stream_url;
	}
	info.succinct_thumbnail_url = youtube_get_video_thumbnail_url_by_id(id);
	info.comment_continue_type = -1;
	info.comments_disabled = true;
	return info;
}

// popup with a long text in a scroll view (used for translations)
static void show_text_popup(const std::string &title, const std::string &text) {
	const int width = 290;
	std::vector<View *> lines;
	size_t pos = 0;
	while (pos <= text.size() && lines.size() < 120) {
		size_t nl = text.find('\n', pos);
		std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
		for (auto &part : truncate_str(line, width - 8, 6, 0.5, 0.5)) {
			lines.push_back((new TextView(4, 0, width - 8, DEFAULT_FONT_INTERVAL))->set_text(part));
		}
		if (line.empty()) lines.push_back(new EmptyView(0, 0, width, DEFAULT_FONT_INTERVAL / 2));
		if (nl == std::string::npos) break;
		pos = nl + 1;
	}
	int body_height = 170;
	text_popup_view->recursive_delete_subviews();
	text_popup_view->set_subview(
	    (new VerticalListView(0, 0, width))
	        ->set_views({(new TextView(0, 0, width, 22))
	                         ->set_text(title)
	                         ->set_x_alignment(TextView::XAlign::CENTER)
	                         ->set_get_background_color([](const View &) { return LIGHT0_BACK_COLOR; }),
	                     (new ScrollView(0, 0, width, body_height))
	                         ->set_views({(new VerticalListView(0, 0, width))->set_views(lines)})
	                         ->set_get_background_color([](const View &) { return DEFAULT_BACK_COLOR; }),
	                     (new TextView(0, 0, width, 22))
	                         ->set_text(LOCALIZED(CLOSE))
	                         ->set_x_alignment(TextView::XAlign::CENTER)
	                         ->set_get_background_color(View::STANDARD_BACKGROUND)
	                         ->set_on_view_released([](View &) {
		                         text_popup_view->set_is_visible(false);
		                         var_need_refresh = true;
	                         })})
	        ->set_draw_order({1, 0, 2}));
	text_popup_view->set_is_visible(true);
	var_need_refresh = true;
}

// popup with the playlists of the YouTube account : tapping one adds / removes the video
static void show_account_playlists(const std::string &video_id, const std::vector<AccountPlaylist> &lists) {
	const int width = 260;
	std::vector<View *> items;
	for (auto &p : lists) {
		AccountPlaylist copy = p;
		items.push_back((new TextView(0, 0, width, 22))
		                    ->set_text(std::string(p.has_video ? "[x] " : "[ ] ") + truncate_first_line(p.title, width - 30, 0.5))
		                    ->set_y_alignment(TextView::YAlign::CENTER)
		                    ->set_get_background_color(View::STANDARD_BACKGROUND)
		                    ->set_on_view_released([copy, video_id](View &) {
			                    account_playlist_edit_async(copy.id, video_id, !copy.has_video);
			                    account_pl_popup_view->set_is_visible(false);
			                    var_need_refresh = true;
		                    }));
	}
	int list_height = std::min<int>(150, (int)items.size() * 22);
	account_pl_popup_view->recursive_delete_subviews();
	account_pl_popup_view->set_subview(
	    (new VerticalListView(0, 0, width))
	        ->set_views({(new TextView(0, 0, width, 22))
	                         ->set_text(tr("Save to my list", "Guardar en mi lista"))
	                         ->set_x_alignment(TextView::XAlign::CENTER)
	                         ->set_get_background_color([](const View &) { return LIGHT0_BACK_COLOR; }),
	                     (new ScrollView(0, 0, width, list_height))
	                         ->set_views({(new VerticalListView(0, 0, width))->set_views(items)})
	                         ->set_get_background_color([](const View &) { return DEFAULT_BACK_COLOR; }),
	                     (new TextView(0, 0, width, 22))
	                         ->set_text(LOCALIZED(CLOSE))
	                         ->set_x_alignment(TextView::XAlign::CENTER)
	                         ->set_get_background_color(View::STANDARD_BACKGROUND)
	                         ->set_on_view_released([](View &) {
		                         account_pl_popup_view->set_is_visible(false);
		                         var_need_refresh = true;
	                         })})
	        ->set_draw_order({1, 0, 2}));
	account_pl_popup_view->set_is_visible(true);
	var_need_refresh = true;
}

// ---- collaboration videos : a screen with every channel of the video ----
static std::string collab_request_id; // set by the author row of a collaboration video, handled outside of the view update
static Mutex collab_lock;
static std::vector<YouTubeChannelSuccinct> collab_result;
static bool collab_ready = false;
static bool chat_request = false, chat_write_request = false; // live chat : open the screen / write a message
static unsigned chat_shown_version = 0;
static bool collab_refresh_request = false; // a channel of the screen was (un)subscribed : redraw its texts
static std::vector<YouTubeChannelSuccinct> collab_shown; // the channels of the screen that is open

static void load_collab_task(void *arg) {
	std::string *video_id = (std::string *)arg;
	std::vector<YouTubeChannelSuccinct> list;
	std::string error;
	if (youtube_load_collaborators(*video_id, list, error)) {
		collab_lock.lock();
		collab_result = list;
		collab_ready = true;
		collab_lock.unlock();
		var_need_refresh = true;
	} else {
		logger.warning("player", "collaborators : " + error);
		notice_show(tr("Could not load the channels", "No se pudieron cargar los canales"));
	}
	delete video_id;
}

static bool collab_take(std::vector<YouTubeChannelSuccinct> &out) {
	bool ready;
	collab_lock.lock();
	ready = collab_ready;
	if (ready) {
		out = collab_result;
		collab_ready = false;
	}
	collab_lock.unlock();
	return ready && out.size();
}

// the live chat on the popup : newest message first (so no scrolling is needed)
static void show_live_chat() {
	const int width = 300;
	chat_shown_version = live_chat_version();
	VerticalListView *list = new VerticalListView(0, 0, width);
	std::string status = live_chat_status();
	if (status != "") {
		list->views.push_back((new TextView(0, 0, width, 22))->set_text(status)->set_font_size(0.5, DEFAULT_FONT_INTERVAL));
	}
	for (auto &m : live_chat_latest(14)) {
		std::vector<std::string> lines = truncate_str(m.author + ": " + m.text, width - 8, 2, 0.5, 0.5);
		list->views.push_back((new TextView(4, 0, width - 4, DEFAULT_FONT_INTERVAL * (int)std::max<size_t>(1, lines.size()) + 2))
		                          ->set_text_lines(lines)
		                          ->set_font_size(0.5, DEFAULT_FONT_INTERVAL));
	}
	int list_height = 240 - VIDEO_PLAYING_BAR_HEIGHT - 80;
	account_pl_popup_view->recursive_delete_subviews();
	account_pl_popup_view->set_subview(
	    (new VerticalListView(0, 0, width))
	        ->set_views({(new TextView(0, 0, width, 22))
	                         ->set_text(tr("Live chat", "Chat en directo"))
	                         ->set_x_alignment(TextView::XAlign::CENTER)
	                         ->set_get_background_color([](const View &) { return LIGHT0_BACK_COLOR; }),
	                     (new ScrollView(0, 0, width, list_height))
	                         ->set_views({list})
	                         ->set_get_background_color([](const View &) { return DEFAULT_BACK_COLOR; }),
	                     (new HorizontalListView(0, 0, 22))
	                         ->set_views({(new TextView(0, 0, width / 2, 22))
	                                          ->set_text(tr("Write", "Escribir"))
	                                          ->set_x_alignment(TextView::XAlign::CENTER)
	                                          ->set_get_background_color(View::STANDARD_BACKGROUND)
	                                          ->set_on_view_released([](View &) { chat_write_request = true; }),
	                                      (new TextView(0, 0, width / 2, 22))
	                                          ->set_text(LOCALIZED(CLOSE))
	                                          ->set_x_alignment(TextView::XAlign::CENTER)
	                                          ->set_get_background_color(View::STANDARD_BACKGROUND)
	                                          ->set_on_view_released([](View &) {
		                                          live_chat_close();
		                                          account_pl_popup_view->set_is_visible(false);
		                                          var_need_refresh = true;
	                                          })})})
	        ->set_draw_order({1, 0, 2}));
	account_pl_popup_view->set_is_visible(true);
	var_need_refresh = true;
}

static void show_collab_channels(const std::vector<YouTubeChannelSuccinct> &channels) {
	collab_shown = channels;
	const int width = 300;
	VerticalListView *list = (new VerticalListView(0, 0, width))->enable_thumbnail_request_update(6, SceneType::VIDEO_PLAYER);
	for (auto &channel : channels) {
		SuccinctChannelView *view = new SuccinctChannelView(0, 0, width, CHANNEL_ICON_HEIGHT);
		view->set_name(truncate_first_line(channel.name, width - CHANNEL_ICON_WIDTH - 6, 0.6));
		bool subscribed = subscription_is_subscribed(channel.id);
		view->set_auxiliary_lines({truncate_first_line(channel.subscribers, width - CHANNEL_ICON_WIDTH - 6, 0.5),
		                           subscribed ? tr("Subscribed - long press to unsubscribe", "Suscrito - mantén para quitar")
		                                      : tr("Long press to subscribe", "Mantén para suscribirte")});
		view->set_thumbnail_url(channel.icon_url);
		view->set_get_background_color(View::STANDARD_BACKGROUND);
		std::string id = channel.id;
		view->set_on_view_released([id](View &) {
			channel_id_pressed = id; // opens the channel
			account_pl_popup_view->set_is_visible(false);
			var_need_refresh = true;
		});
		view->add_on_long_hold(35, [channel](View &v) { // subscribe / unsubscribe this channel (and on the account when signed in)
			bool now = subscription_is_subscribed(channel.id);
			if (now) {
				subscription_unsubscribe(channel.id);
			} else {
				SubscriptionChannel new_channel;
				new_channel.id = channel.id;
				new_channel.url = "https://m.youtube.com/channel/" + channel.id;
				new_channel.name = channel.name;
				new_channel.icon_url = channel.icon_url;
				new_channel.subscriber_count_str = channel.subscribers;
				subscription_subscribe(new_channel);
			}
			account_subscribe_async(channel.id, !now);
			notice_show(now ? tr("Unsubscribed", "Suscripción quitada") : tr("Subscribed", "Suscrito"));
			v.reset_holding_status(); // the release must not open the channel
			collab_refresh_request = true;
			var_need_refresh = true;
		});
		list->views.push_back(view);
	}
	int list_height = std::min<int>(CHANNEL_ICON_HEIGHT * 3, (int)channels.size() * CHANNEL_ICON_HEIGHT);
	account_pl_popup_view->recursive_delete_subviews();
	account_pl_popup_view->set_subview(
	    (new VerticalListView(0, 0, width))
	        ->set_views({(new TextView(0, 0, width, 22))
	                         ->set_text(tr("Channels in this video", "Canales de este vídeo"))
	                         ->set_x_alignment(TextView::XAlign::CENTER)
	                         ->set_get_background_color([](const View &) { return LIGHT0_BACK_COLOR; }),
	                     (new ScrollView(0, 0, width, list_height))
	                         ->set_views({list})
	                         ->set_get_background_color([](const View &) { return DEFAULT_BACK_COLOR; }),
	                     (new TextView(0, 0, width, 22))
	                         ->set_text(LOCALIZED(CLOSE))
	                         ->set_x_alignment(TextView::XAlign::CENTER)
	                         ->set_get_background_color(View::STANDARD_BACKGROUND)
	                         ->set_on_view_released([](View &) {
		                         account_pl_popup_view->set_is_visible(false);
		                         var_need_refresh = true;
	                         })})
	        ->set_draw_order({1, 0, 2}));
	account_pl_popup_view->set_is_visible(true);
	var_need_refresh = true;
}

// Sound profiles : an equalizer curve plus a volume, for one tap
static std::vector<View *> make_sound_profile_buttons() {
	static const double music[18] = {1.4, 1.35, 1.25, 1.1, 1, 1, 1, 1, 1, 1.05, 1.15, 1.25, 1.3, 1.3, 1.25, 1.2, 1.15, 1.1};
	static const double movie[18] = {1.5, 1.4, 1.25, 1.1, 1, 1, 1.1, 1.25, 1.3, 1.25, 1.15, 1.05, 1, 1, 1, 1, 1, 1};
	static const double speech[18] = {0.4, 0.45, 0.55, 0.75, 1, 1.25, 1.5, 1.6, 1.6, 1.45, 1.3, 1.1, 0.9, 0.7, 0.6, 0.5, 0.4, 0.4};
	struct Profile {
		const double *gains;
		double volume;
	};
	static const Profile profiles[3] = {{music, 1.0}, {movie, 1.3}, {speech, 1.2}};
	auto button = [](std::function<std::string()> name, int index) {
		return (new TextView(0, 0, EQUALIZER_POPUP_WIDTH / 3, 19))
		    ->set_text(name)
		    ->set_x_alignment(TextView::XAlign::CENTER)
		    ->set_get_background_color(View::STANDARD_BACKGROUND)
		    ->set_on_view_released([index](View &) {
			    double copy[18];
			    for (int i = 0; i < 18; i++) copy[i] = profiles[index].gains[i];
			    apply_eq_preset(reinterpret_cast<const double(&)[18]>(copy));
			    mini_volume_apply(profiles[index].volume);
		    });
	};
	return {button([]() { return tr("Music", "Música"); }, 0), button([]() { return tr("Movie", "Película"); }, 1),
	        button([]() { return tr("Speech", "Voz hablada"); }, 2)};
}

static void decode_thread(void *arg);
static void convert_thread(void *arg);


// Equalizer presets : 18 gains (1.0 = unchanged) for the same bands as equalizer_hz_list
static void apply_eq_preset(const double (&gains)[18]);
static std::vector<View *> make_eq_preset_buttons();
static std::vector<View *> make_sound_profile_buttons();

void VideoPlayer_init(void) {
	logger.info(DEF_SAPP0_INIT_STR, "Initializing...");
	bool new_3ds = false;
	Result_with_string result;

	vid_thread_run = true;

	suggestion_tab_view =
	    (new ScrollView(0, 0, 320, CONTENT_Y_HIGH))->set_views({suggestion_main_view, suggestion_bottom_view});
	comment_tab_view = (new ScrollView(0, 0, 320, CONTENT_Y_HIGH))
	                       ->set_views({comments_top_view, comments_main_view, comments_bottom_view});

	caption_language_select_view = new VerticalListView(0, 0, 320);
	captions_tab_view->set_subview(caption_language_select_view);
	caption_main_view = new ScrollView(0, 0, 320, CONTENT_Y_HIGH);
	caption_overlay_view = new CaptionOverlayView(0, 0, 400, 240);
	cur_playing_video_view =
	    (new SuccinctVideoView(SMALL_MARGIN, 0, 320 - SMALL_MARGIN * 2, VIDEO_LIST_THUMBNAIL_HEIGHT));
	download_progress_view = (new CustomView(0, 0, 320, SMALL_MARGIN * 2))->set_draw([](const CustomView &view) {
		int y = view.y0;
		int sample = 50;
		auto progress_bars = network_decoder.get_buffering_progress_bars(sample);
		for (size_t i = 0; i < 2; i++) {
			if (i < progress_bars.size() && progress_bars[i].second.size()) {
				auto &cur_bar = progress_bars[i].second;
				auto cur_pos = progress_bars[i].first;

				int sample = 50;
				int bar_len = 310;
				for (int i = 0; i < sample; i++) {
					int a = 0xFF;
					int r = 0x00 * cur_bar[i] / 100 + 0x80 * (1 - cur_bar[i] / 100);
					int g = 0x00 * cur_bar[i] / 100 + 0x80 * (1 - cur_bar[i] / 100);
					int b = 0xA0 * cur_bar[i] / 100 + 0x80 * (1 - cur_bar[i] / 100);
					int xl = 5 + bar_len * i / sample;
					int xr = 5 + bar_len * (i + 1) / sample;
					Draw_texture(var_square_image[0], a << 24 | b << 16 | g << 8 | r, xl, y, xr - xl, SMALL_MARGIN);
				}
				float head_x = 5 + bar_len * cur_pos;
				Draw_texture(var_square_image[0], DEF_DRAW_RED, head_x - 1, y, SMALL_MARGIN, SMALL_MARGIN);
			}
			y += SMALL_MARGIN;
		}
		if (vid_duration > 0) { // chapter marks on the bar
			for (auto &chapter : current_chapters) {
				if (chapter.first <= 0 || chapter.first >= vid_duration) continue;
				Draw_texture(var_square_image[0], 0xFFFFFFFF, 5 + 310 * chapter.first / vid_duration, view.y0, 1, SMALL_MARGIN * 2);
			}
		}
	});
	video_quality_selector_view = (new SelectorView(0, 0, 320, 35, false))
	                                  ->set_texts({(std::function<std::string()>)[]() { return LOCALIZED(OFF);
}
}, 0)
	                                  ->set_title([](const SelectorView &view) {
	return LOCALIZED(VIDEO); });
debug_info_view =
    (new VerticalListView(0, 0, 320))
        ->set_views({(new TextView(SMALL_MARGIN, 0, 320, DEFAULT_FONT_INTERVAL * 8))
                         ->set_text_lines<std::function<std::string()>>(
                             {[]() { return vid_video_format; }, []() { return vid_audio_format; },
                              []() {
	                              return std::to_string(vid_width_org) + "x" + std::to_string(vid_height_org) + "@" +
	                                     std::to_string(vid_framerate).substr(0, 5) + "FPS";
                              },
                              []() {
	                              auto decoder_type = network_decoder.get_decoder_type();
	                              std::string decoder_type_str =
	                                  decoder_type == NetworkMultipleDecoder::DecoderType::NA   ? "N/A"
	                                  : decoder_type == NetworkMultipleDecoder::DecoderType::HW ? LOCALIZED(HW_DECODER)
	                                  : decoder_type == NetworkMultipleDecoder::DecoderType::MT_SLICE
	                                      ? LOCALIZED(MULTITHREAD_SLICE)
	                                  : decoder_type == NetworkMultipleDecoder::DecoderType::MT_FRAME
	                                      ? LOCALIZED(MULTITHREAD_FRAME)
	                                      : LOCALIZED(SINGLE_THREAD);
	                              return LOCALIZED(DECODER_TYPE) + " : " + decoder_type_str;
                              },
                              []() {
	                              const char *message = get_network_waiting_status();
	                              return LOCALIZED(WAITING_STATUS) + " : " + std::string(message ? message : "");
                              },
                              []() {
	                              u32 cpu_limit;
	                              APT_GetAppCpuTimeLimit(&cpu_limit);
	                              return LOCALIZED(CPU_LIMIT) + " : " + std::to_string(cpu_limit) + "%";
                              },
                              []() {
	                              return LOCALIZED(FORWARD_BUFFER) + " : " +
	                                     (cur_video_info.is_livestream && network_decoder.ready
	                                          ? std::to_string(network_decoder.get_forward_buffer())
	                                          : "N/A");
                              },
                              []() {
	                              return LOCALIZED(RAW_FRAME_BUFFER) + " : " +
	                                     std::to_string(network_decoder.get_raw_buffer_num()) + "/" +
	                                     std::to_string(network_decoder.get_raw_buffer_num_max());
                              }}),
                     (new RuleView(0, 0, 320, SMALL_MARGIN * 2)),
                     (new CustomView(0, 0, 320, 160))->set_draw([](const CustomView &view) {
	                     int y = view.y0;

	                     // decoding time graph
	                     for (int i = 0; i < 319; i++) {
		                     Draw_line(i, y + 90 - vid_time[1][i], DEF_DRAW_BLUE, i + 1, y + 90 - vid_time[1][i + 1],
		                               DEF_DRAW_BLUE, 1); // Thread 1
		                     Draw_line(i, y + 90 - vid_time[0][i], DEF_DRAW_RED, i + 1, y + 90 - vid_time[0][i + 1],
		                               DEF_DRAW_RED, 1); // Thread 0
	                     }

	                     Draw_line(0, y + 90, DEFAULT_TEXT_COLOR, 320, y + 90, DEFAULT_TEXT_COLOR, 2);
	                     Draw_line(0, y + 90 - vid_frametime, 0xFFFFFF00, 320, y + 90 - vid_frametime, 0xFFFFFF00, 2);
	                     if (vid_total_frames != 0 && vid_min_time != 0 && vid_recent_total_time != 0) {
		                     Draw("Avg: " + std::to_string(1000 / (vid_total_time / vid_total_frames)).substr(0, 5) +
		                              " Min: " + std::to_string(1000 / vid_max_time).substr(0, 5) +
		                              " Max: " + std::to_string(1000 / vid_min_time).substr(0, 5) + " Recent Avg: " +
		                              std::to_string(1000 / (vid_recent_total_time / 90)).substr(0, 5) + " FPS",
		                          0, y + 90, 0.4, 0.4, DEFAULT_TEXT_COLOR);
	                     }

	                     Draw("Deadline : " + std::to_string(vid_frametime).substr(0, 5) + "ms", 0, y + 100, 0.4, 0.4,
	                          0xFFFFFF00);
	                     Draw("Video decode : " + std::to_string(vid_video_time).substr(0, 5) + "ms", 0, y + 110, 0.4,
	                          0.4, DEF_DRAW_RED);
	                     Draw("Audio decode : " + std::to_string(vid_audio_time).substr(0, 5) + "ms", 0, y + 120, 0.4,
	                          0.4, DEF_DRAW_RED);
	                     // Draw("Data copy 0 : " + std::to_string(vid_copy_time[0]).substr(0, 5) + "ms", 160, 120, 0.4,
	                     // 0.4, DEF_DRAW_BLUE);
	                     Draw("Color convert : " + std::to_string(vid_convert_time).substr(0, 5) + "ms", 160, y + 110,
	                          0.4, 0.4, DEF_DRAW_BLUE);
	                     Draw("Data copy 1 : " + std::to_string(vid_copy_time[1]).substr(0, 5) + "ms", 160, y + 120,
	                          0.4, 0.4, DEF_DRAW_BLUE);
	                     Draw("Thread 0 : " + std::to_string(vid_time[0][319]).substr(0, 6) + "ms", 0, y + 130, 0.5,
	                          0.5, DEF_DRAW_RED);
	                     Draw("Thread 1 : " + std::to_string(vid_time[1][319]).substr(0, 6) + "ms", 160, y + 130, 0.5,
	                          0.5, DEF_DRAW_BLUE);
	                     Draw("Zoom : x" + std::to_string(vid_zoom).substr(0, 5) +
	                              " X : " + std::to_string((int)vid_x) + " Y : " + std::to_string((int)vid_y),
	                          0, y + 140, 0.5, 0.5, DEFAULT_TEXT_COLOR);
                     })});
    playback_tab_view =
	    (new ScrollView(0, 0, 320, CONTENT_Y_HIGH))
	        ->set_margin(SMALL_MARGIN)
	        ->set_views(
	            {(new EmptyView(0, 0, 320, 0)), // margin at the top(using ScrollView's auto margin)
	             (new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL))->set_text((std::function<std::string()>)[]() {
	return LOCALIZED(CUR_PLAYING_VIDEO);
	             }),
	             cur_playing_video_view,
	             (new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL))->set_text((std::function<std::string()>)[]() {
	return LOCALIZED(BUFFERING_PROGRESS);
	             }),
	             download_progress_view,
	             (new TextView(SMALL_MARGIN * 2, 0, 100, CONTROL_BUTTON_HEIGHT))
	                 ->set_text((std::function<std::string()>)[]() {
	return LOCALIZED(RELOAD); })
	                 ->set_x_alignment(TextView::XAlign::CENTER)
	                 ->set_get_background_color([](const View &) {
	return is_async_task_running(load_video_page) ? DEF_DRAW_LIGHT_GRAY : DEF_DRAW_WEAK_GREEN;
	                 })
	                 ->set_on_view_released([](View &view) {
	if (!is_async_task_running(load_video_page)) {
		seek_at_init_request = vid_current_pos;
		send_change_video_request_wo_lock(cur_playing_url, true, false, true);
		video_retry_left = MAX_RETRY_CNT;
	}
	                 }),
	             video_quality_selector_view,
				 video_3d_selector_view = (new SelectorView(0, 0, 320, 35, true))
											  ->set_texts({(std::function<std::string()>)[]() { return LOCALIZED(OFF); },
														   (std::function<std::string()>)[]() { return "Side by Side"; }
	},
														  var_3d_mode)
											  ->set_title([](const SelectorView &) { return "3D"; })
											  ->set_on_change([](const SelectorView &view) {
	if (var_3d_mode != view.selected_button) {
		var_3d_mode = view.selected_button;
		if (var_3d_mode) {
			gfxSet3D(true);
		} else {
			gfxSetWide(true);
		}
		misc_tasks_request(TASK_SAVE_SETTINGS);
	}
											  }),
	             video_loop_view = (new SelectorView(0, 0, 320, 35, true))
	                                   ->set_texts({(std::function<std::string()>)[]() { return LOCALIZED(OFF); },
	                                                (std::function<std::string()>)[]() {
	return LOCALIZED(ON); }
    },
	                                               var_loop_mode)
	                                   ->set_title([](const SelectorView &) {
	return LOCALIZED(LOOP); })
	                                   ->set_on_change([](const SelectorView &view) {
	if (var_loop_mode != view.selected_button) {
		var_loop_mode = view.selected_button;
		misc_tasks_request(TASK_SAVE_SETTINGS);
	}
	                                   }),
	             (new BarView(0, 0, 260, 30))                     // preamp
	                 ->set_values(std::log(0.25), std::log(4), 0) // exponential scale
	                 ->set_title([](const BarView &view) {
	return LOCALIZED(PREAMP) + " : " + std::to_string((int)std::round(std::exp(view.get_value()) * 100)) + "%";
	                 })
	                 ->set_while_holding([](BarView &view) {
	double volume = std::exp(view.get_value());
	if (volume >= 0.95 && volume <= 1.05) {
		volume = 1.0, view.set_value(0);
	}
	// volume change needs a reconstruction of the filter graph, so don't update volume too often
	static int cnt = 0;
	if (++cnt >= 15) {
		cnt = 0, network_decoder.set_preamp(volume);
	}
	                 })
	                 ->set_on_release([](BarView &view) {
	double volume = std::exp(view.get_value());
	if (volume >= 0.95 && volume <= 1.05) {
		volume = 1.0, view.set_value(0);
	}
	network_decoder.set_preamp(volume);
	                 }),
	             (speed_bar_view = new BarView(0, 0, 260, 30)) // speed
	                 ->set_values(0.25, 1.5, 1.0)
	                 ->set_title([](const BarView &view) {
	return LOCALIZED(SPEED) + " : " + std::to_string((int)std::round(view.get_value() * 100)) + "%";
	                 })
	                 ->set_while_holding([](BarView &view) {
	view.fix_close_values(0.97, 1.0, 1.03);
	static int cnt = 0;
	if (++cnt >= 15) {
		cnt = 0, network_decoder.set_tempo(view.get_value());
	}
	                 })
	                 ->set_on_release([](BarView &view) {
	view.fix_close_values(0.97, 1.0, 1.03);
	network_decoder.set_tempo(view.get_value());
	                 }),
	             (new BarView(0, 0, 260, 30)) // pitch
	                 ->set_values(0.5, 2.0, 1.0)
	                 ->set_title([](const BarView &view) {
	return LOCALIZED(PITCH) + " : " + std::to_string((int)std::round(view.get_value() * 100)) + "%";
	                 })
	                 ->set_while_holding([](BarView &view) {
	view.fix_close_values(0.97, 1.0, 1.03);
	static int cnt = 0;
	if (++cnt >= 15) {
		cnt = 0, network_decoder.set_pitch(view.get_value());
	}
	                 })
	                 ->set_on_release([](BarView &view) {
	view.fix_close_values(0.97, 1.0, 1.03);
	network_decoder.set_pitch(view.get_value());
	                 }),
	             (new BarView(0, 0, 260, 30)) // sleep timer
	                 ->set_values(0, 120, 0)
	                 ->set_title([](const BarView &view) {
	u64 now = osGetTime();
	if (var_sleep_timer_end > now) {
		return std::string("Sleep timer : ") + std::to_string((var_sleep_timer_end - now) / 60000 + 1) + " min";
	}
	return std::string("Sleep timer : off");
	                 })
	                 ->set_on_release([](BarView &view) {
	int minutes = (int)std::round(view.get_value());
	var_sleep_timer_end = minutes > 0 ? osGetTime() + (u64)minutes * 60000ULL : 0;
	                 }),
	             (new TextView(SMALL_MARGIN * 2, 0, 100, 20))
	                 ->set_text([]() {
	return LOCALIZED(OPEN_EQUALIZER); })
	                 ->set_x_alignment(TextView::XAlign::CENTER)
	                 ->set_text_offset(0, -1)
	                 ->set_get_background_color([](const View &) {
	return DEF_DRAW_WEAK_YELLOW; })
	                 ->set_on_view_released([](View &view) {
	equalizer_popup_view->set_is_visible(true); }),
	             (new RuleView(0, 0, 320, SMALL_MARGIN)), debug_info_view
    });
    std::vector<View *> equalizer_views;
    constexpr int equalizer_hz_list[18] = {65,   92,   131,  185,  262,  370,  523,   740,   1047,
                                           1480, 2093, 2960, 4186, 5920, 8372, 11840, 16744, 20000};
    static double equalizer_values[18];
    std::fill(equalizer_values, equalizer_values + 18, 1.0);
    eq_values_ptr = equalizer_values;
    for (int i = 0; i < 18; i++) {
	    BarView *bar_view = (new BarView(0, 0, 180, DEFAULT_FONT_INTERVAL))
	                            ->set_values_sync(0.0, 2.0, &equalizer_values[i])
	                            ->set_while_holding([i](BarView &view) {
		                            static int cnt = 0;
		                            if (++cnt >= 30) {
			                            cnt = 0;
			                            network_decoder.set_equalizer_value(i, view.get_value());
		                            }
	                            })
	                            ->set_on_release([i](BarView &view) {
		                            logger.info("debug", std::to_string(i) + " : " + std::to_string(view.get_value()));
		                            network_decoder.set_equalizer_value(i, view.get_value());
	                            });
	    equalizer_views.push_back((new HorizontalListView(0, 0, DEFAULT_FONT_INTERVAL))
	                                  ->set_views({(new TextView(0, 0, 60, DEFAULT_FONT_INTERVAL))
	                                                   ->set_text(std::to_string(equalizer_hz_list[i]) + " Hz"),
	                                               bar_view}));
    }
    equalizer_popup_view->set_subview(
        (new VerticalListView(0, 0, EQUALIZER_POPUP_WIDTH))
            ->set_views({(new HorizontalListView(0, 0, 19))
                             ->set_views(make_eq_preset_buttons()),
                         (new HorizontalListView(0, 0, 19))
                             ->set_views(make_sound_profile_buttons()),
                         (new RuleView(0, 0, EQUALIZER_POPUP_WIDTH, 1))->set_margin(0),
                         (new ScrollView(0, 0, EQUALIZER_POPUP_WIDTH, EQUALIZER_POPUP_HEIGHT - 59))
                             ->set_views(equalizer_views)
                             ->set_get_background_color([](const View &) { return DEFAULT_BACK_COLOR; }),
                         (new TextView(0, 0, EQUALIZER_POPUP_WIDTH, 20))
                             ->set_text([]() { return LOCALIZED(CLOSE); })
                             ->set_x_alignment(TextView::XAlign::CENTER)
                             ->set_on_view_released([](View &) {
	                             equalizer_popup_view->set_is_visible(false);
	                             var_need_refresh = true;
                             })
                             ->set_get_background_color(View::STANDARD_BACKGROUND)})
            ->set_draw_order({3, 0, 1, 2, 4}));
    equalizer_popup_view->set_is_visible(false);
    if (!text_popup_view) text_popup_view = new OverlayView(0, 0, 320, 240 - VIDEO_PLAYING_BAR_HEIGHT);
    text_popup_view->set_on_cancel([](OverlayView &view) {
	    view.set_is_visible(false);
	    var_need_refresh = true;
    });
    text_popup_view->set_is_visible(false);
    if (!account_pl_popup_view) account_pl_popup_view = new OverlayView(0, 0, 320, 240 - VIDEO_PLAYING_BAR_HEIGHT);
    account_pl_popup_view->set_on_cancel([](OverlayView &view) {
	    view.set_is_visible(false);
	    var_need_refresh = true;
    });
    account_pl_popup_view->set_is_visible(false);
    if (!comment_dialog) comment_dialog = new OverlayDialogView(0, 0, 320, 240 - VIDEO_PLAYING_BAR_HEIGHT);
    comment_dialog->set_is_visible(false);
    playlist_title_view =
        (new TextView(0, 0, 320, MIDDLE_FONT_INTERVAL))->set_font_size(MIDDLE_FONT_SIZE, MIDDLE_FONT_INTERVAL);
    playlist_author_view =
        (new TextView(0, 0, 320, PLAYLIST_TOP_HEIGHT - MIDDLE_FONT_INTERVAL))->set_get_text_color([]() {
	        return LIGHT0_TEXT_COLOR;
        });
    playlist_list_videos_view =
        (new VerticalListView(0, 0, 320))
            ->set_margin(SMALL_MARGIN)
            ->enable_thumbnail_request_update(MAX_THUMBNAIL_LOAD_REQUEST, SceneType::VIDEO_PLAYER);
    playlist_list_view =
        (new ScrollView(0, 0, 320, CONTENT_Y_HIGH - PLAYLIST_TOP_HEIGHT))->set_views({playlist_list_videos_view});
    playlist_tab_view = (new VerticalListView(0, 0, 320))
                            ->set_views({playlist_title_view, playlist_author_view,
                                         (new RuleView(0, 0, 320, SMALL_MARGIN)), playlist_list_view})
                            ->set_draw_order({3, 2, 1, 0});
    for (int i = 0; i < 3; i++) {
	    playlist_tab_view->views[i]->set_get_background_color([](const View &) { return DEFAULT_BACK_COLOR; });
    }

    // default : 5 tabs
    main_view =
        (new TabView(0, 0, 320, 240 - VIDEO_PLAYING_BAR_HEIGHT))
            ->set_stretch_subview(true)
            ->set_tab_font_size(0.4)
            ->set_views({main_tab_view, suggestion_tab_view, comment_tab_view, captions_tab_view, playback_tab_view},
                        TAB_GENERAL)
            ->set_tab_texts<std::function<std::string()>>(
                {[]() { return LOCALIZED(GENERAL); }, []() { return LOCALIZED(SUGGESTIONS); },
                 []() { return LOCALIZED(COMMENTS); }, []() { return LOCALIZED(CAPTIONS); },
                 []() { return LOCALIZED(PLAYBACK); }});

    add_cpu_limit(CPU_LIMIT);
    if (var_is_new3ds) {
	    bool frame_cores[4] = {false, true, var_core2_available, var_core3_available};
	    bool slice_cores[4] = {false, true, false, var_core3_available};
	    network_decoder.set_frame_cores_enabled(frame_cores);
	    network_decoder.set_slice_cores_enabled(slice_cores);

	    vid_decode_thread =
	        threadCreate(decode_thread, (void *)(""), DEF_STACKSIZE, DEF_THREAD_PRIORITY_HIGH, 2, false);
	    vid_convert_thread =
	        threadCreate(convert_thread, (void *)(""), DEF_STACKSIZE, DEF_THREAD_PRIORITY_NORMAL, 0, false);
	    livestream_initer_thread = threadCreate(livestream_initer_thread_func, &network_decoder, DEF_STACKSIZE,
	                                            DEF_THREAD_PRIORITY_NORMAL, 2, false);
    } else {
	    bool frame_cores[4] = {true, true, false, false};
	    bool slice_cores[4] = {false, true, false, false};
	    network_decoder.set_frame_cores_enabled(frame_cores);
	    network_decoder.set_slice_cores_enabled(slice_cores);

	    vid_decode_thread =
	        threadCreate(decode_thread, (void *)("1"), DEF_STACKSIZE, DEF_THREAD_PRIORITY_HIGH, 1, false);
	    vid_convert_thread =
	        threadCreate(convert_thread, (void *)(""), DEF_STACKSIZE, DEF_THREAD_PRIORITY_NORMAL, 0, false);
	    livestream_initer_thread = threadCreate(livestream_initer_thread_func, &network_decoder, DEF_STACKSIZE,
	                                            DEF_THREAD_PRIORITY_NORMAL, 1, false);
    }
    stream_downloader_thread = threadCreate(network_downloader_thread, &stream_downloader, DEF_STACKSIZE,
                                            DEF_THREAD_PRIORITY_NORMAL, 0, false);

    vid_total_time = 0;
    vid_total_frames = 0;
    vid_min_time = 99999999;
    vid_max_time = 0;
    vid_recent_total_time = 0;
    for (int i = 0; i < 90; i++) {
	    vid_recent_time[i] = 0;
    }

    for (int i = 0; i < 2; i++) {
	    vid_tex_width[i] = 0;
	    vid_tex_height[i] = 0;
    }

    for (int i = 0; i < 320; i++) {
	    vid_time[0][i] = 0;
	    vid_time[1][i] = 0;
    }

    vid_audio_time = 0;
    vid_video_time = 0;
    vid_copy_time[0] = 0;
    vid_copy_time[1] = 0;
    vid_convert_time = 0;

    for (int i = 0; i < 2; i++) {
	    result = Draw_c2d_image_init(&vid_image[i], 1024, 1024, GPU_RGB565);
	    if (result.code != 0) {
		    Util_err_set_error_message(DEF_ERR_OUT_OF_LINEAR_MEMORY_STR, "", DEF_SAPP0_INIT_STR,
		                               DEF_ERR_OUT_OF_LINEAR_MEMORY);
		    Util_err_set_error_show_flag(true);
		    vid_thread_run = false;
	    }
	    Draw_c2d_image_set_filter(&vid_image[i], var_video_linear_filter);
    }

    result = Draw_load_texture("romfs:/gfx/draw/video_player/banner.t3x", 61, vid_banner, 0, 2);
    if (result.code != 0) {
	    logger.error(DEF_SAPP0_INIT_STR, "Draw_load_texture()..." + result.string + result.error_description,
	                 result.code);
    }

    result = Draw_load_texture("romfs:/gfx/draw/video_player/control.t3x", 62, vid_control, 0, 2);
    if (result.code != 0) {
	    logger.error(DEF_SAPP0_INIT_STR, "Draw_load_texture()..." + result.string + result.error_description,
	                 result.code);
    }

    result = Draw_load_texture("romfs:/gfx/draw/thumb_up.t3x", 63, var_texture_thumb_up, 0, 2);
    if (result.code != 0) {
	    logger.error(DEF_SAPP0_INIT_STR, "Draw_load_texture()..." + result.string + result.error_description,
	                 result.code);
    }

    result = Draw_load_texture("romfs:/gfx/draw/thumb_down.t3x", 64, var_texture_thumb_down, 0, 2);
    if (result.code != 0) {
	    logger.error(DEF_SAPP0_INIT_STR, "Draw_load_texture()..." + result.string + result.error_description,
	                 result.code);
    }

    vid_x = 0;
    vid_y = 15;
    vid_frametime = 0;
    vid_framerate = 0;
    vid_current_pos = 0;
    vid_duration = 0;
    vid_zoom = 1;
    vid_width = 0;
    vid_height = 0;
    vid_video_format = "n/a";
    vid_audio_format = "n/a";
    video_p_value = var_is_new3ds ? 360 : 144;

    VideoPlayer_resume("");
    vid_already_init = true;

    video_set_show_debug_info(var_video_show_debug_info);
    video_set_linear_filter_enabled(var_video_linear_filter);
    logger.info(DEF_SAPP0_INIT_STR, "Initialized.");
    }
    void VideoPlayer_exit(void) {
	    logger.info(DEF_SAPP0_EXIT_STR, "Exiting...");
	    u64 time_out = 10000000000;
	    Result_with_string result;

	    vid_already_init = false;
	    vid_thread_suspend = false;
	    vid_thread_run = false;
	    vid_play_request = false;

	    stream_downloader.request_thread_exit();
	    network_decoder.interrupt = true;
	    network_decoder.request_thread_exit();
	    logger.info(DEF_SAPP0_EXIT_STR, "threadJoin()...", threadJoin(vid_decode_thread, time_out));
	    logger.info(DEF_SAPP0_EXIT_STR, "threadJoin()...", threadJoin(vid_convert_thread, time_out));
	    logger.info(DEF_SAPP0_EXIT_STR, "threadJoin()...", threadJoin(stream_downloader_thread, time_out));
	    logger.info(DEF_SAPP0_EXIT_STR, "threadJoin()...", threadJoin(livestream_initer_thread, time_out));
	    threadFree(vid_decode_thread);
	    threadFree(vid_convert_thread);
	    threadFree(stream_downloader_thread);
	    threadFree(livestream_initer_thread);
	    stream_downloader.delete_all();

	    small_resource_lock.lock();

	    // clean up views
	    suggestion_tab_view->recursive_delete_subviews();
	    delete suggestion_tab_view;
	    suggestion_tab_view = NULL;
	    suggestion_main_view = NULL;
	    suggestion_bottom_view = NULL;

	    comment_tab_view->recursive_delete_subviews();
	    delete comment_tab_view;
	    comments_top_view = NULL;
	    comments_main_view = NULL;
	    comments_bottom_view = NULL;
	    comment_tab_view = NULL;

	    delete caption_language_select_view;
	    delete caption_main_view;
	    delete caption_overlay_view;
	    caption_language_select_view = NULL;
	    caption_main_view = NULL;
	    caption_overlay_view = NULL;
	    captions_tab_view = NULL;

	    playback_tab_view->recursive_delete_subviews();
	    delete playback_tab_view;
	    playback_tab_view = NULL;
	    debug_info_view = NULL;
	    download_progress_view = NULL;

	    playlist_tab_view->recursive_delete_subviews();
	    delete playlist_tab_view;
	    playlist_tab_view = NULL;

	    small_resource_lock.unlock();

	    remove_cpu_limit(CPU_LIMIT);

	    Draw_free_texture(61);
	    Draw_free_texture(62);
	    Draw_free_texture(63);
	    Draw_free_texture(64);

	    for (int i = 0; i < 2; i++) {
		    Draw_c2d_image_free(vid_image[i]);
	    }

	    logger.info(DEF_SAPP0_EXIT_STR, "Exited.");
    }
    void VideoPlayer_suspend(void) {
	    vid_thread_suspend = true;
	    vid_main_run = false;
	    mini_visible = false;
    }
    void VideoPlayer_resume(std::string arg) {
	    if (arg != "") {
		    if (arg != cur_displaying_url) {
			    send_change_video_request(arg, !vid_play_request || (vid_pausing && eof_reached), true, false);
			    video_retry_left = MAX_RETRY_CNT;
		    }
	    }
	    main_view->reset_holding_status();
	    // reset scroll
	    main_tab_view->reset();
	    suggestion_tab_view->reset();
	    comment_tab_view->reset();
	    caption_main_view->reset();
	    playlist_list_view->reset();

	    overlay_menu_on_resume();
	    vid_thread_suspend = false;
	    vid_main_run = true;
	    var_need_refresh = true;
    }

    // START : functions called from async_task.cpp
    /* -------------------------------------------------------------------------------------------------------------- */
    /* -------------------------------------------------------------------------------------------------------------- */
    /* -------------------------------------------------------------------------------------------------------------- */

#define TITLE_MAX_WIDTH (320 - SMALL_MARGIN * 2)
#define DESC_MAX_WIDTH (320 - SMALL_MARGIN * 2)
#define SUGGESTION_TITLE_MAX_WIDTH (320 - (VIDEO_LIST_THUMBNAIL_WIDTH + SMALL_MARGIN))
#define COMMENT_MAX_WIDTH (320 - (POST_ICON_SIZE + 2 * SMALL_MARGIN))
#define REPLY_INDENT 25
#define REPLY_MAX_WIDTH (320 - REPLY_INDENT - (REPLY_ICON_SIZE + 2 * SMALL_MARGIN))
#define CAPTION_TIMESTAMP_WIDTH 60
#define CAPTION_CONTENT_MAX_WIDTH (320 - CAPTION_TIMESTAMP_WIDTH)

    static void update_suggestion_bottom_view() {
	    delete suggestion_bottom_view;
	    if (cur_video_info.error != "" || cur_video_info.has_more_suggestions() || !cur_video_info.suggestions.size()) {
		    TextView *bottom_view = (new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL * 2))
		                                ->set_text(cur_video_info.error != ""              ? cur_video_info.error
		                                           : cur_video_info.has_more_suggestions() ? loading_text()
		                                           : !cur_video_info.suggestions.size()    ? LOCALIZED(EMPTY)
		                                                                                   : "IE")
		                                ->set_font_size(0.5, DEFAULT_FONT_INTERVAL)
		                                ->set_x_alignment(TextView::XAlign::CENTER)
		                                ->set_y_alignment(TextView::YAlign::UP);

		    suggestion_tab_view->set_on_child_drawn(1, [](const ScrollView &, int) {
			    if (cur_video_info.has_more_suggestions() && cur_video_info.error == "") {
				    if (!is_async_task_running(load_video_page) && !is_async_task_running(load_more_suggestions)) {
					    queue_async_task(load_more_suggestions, &cur_video_info);
				    }
			    }
		    });
		    suggestion_bottom_view = bottom_view;
	    } else {
		    suggestion_bottom_view = new EmptyView(0, 0, 320, 0);
	    }
	    suggestion_tab_view->views[1] = suggestion_bottom_view;
    }
    // also used for playlist items
    static SuccinctVideoView *suggestion_to_view(const YouTubeSuccinctItem &item) {
	    SuccinctVideoView *cur_view = (new SuccinctVideoView(0, 0, 320, VIDEO_LIST_THUMBNAIL_HEIGHT));
	    cur_view->set_title_lines(truncate_str(item.get_name(), SUGGESTION_TITLE_MAX_WIDTH, 2, 0.5, 0.5));
	    cur_view->set_thumbnail_url(item.get_thumbnail_url());
	    if (item.type == YouTubeSuccinctItem::VIDEO) {
		    cur_view->set_auxiliary_lines({item.video.author});
		    cur_view->set_bottom_right_overlay(item.video.duration_text);
		    cur_view->set_queue_item(item.get_url(), item.get_name());
	    } else if (item.type == YouTubeSuccinctItem::PLAYLIST) {
		    cur_view->set_auxiliary_lines({item.playlist.video_count_str});
	    }
	    cur_view->set_get_background_color(View::STANDARD_BACKGROUND);
	    cur_view->set_on_view_released([item](View &view) { suggestion_clicked_url = item.get_url(); });
	    cur_view->set_is_playlist(item.type == YouTubeSuccinctItem::PLAYLIST);

	    return cur_view;
    }

    static void update_comment_bottom_view() {
	    delete comments_bottom_view;
	    if (cur_video_info.comments_disabled || cur_video_info.error != "" || cur_video_info.has_more_comments() ||
	        !cur_video_info.comments.size()) {
		    TextView *bottom_view = (new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL * 2))
		                                ->set_text(cur_video_info.comments_disabled     ? LOCALIZED(COMMENTS_DISABLED)
		                                           : cur_video_info.error != ""         ? cur_video_info.error
		                                           : cur_video_info.has_more_comments() ? loading_text()
		                                           : !cur_video_info.comments.size()    ? LOCALIZED(NO_COMMENTS)
		                                                                                : "IE")
		                                ->set_font_size(0.5, DEFAULT_FONT_INTERVAL)
		                                ->set_x_alignment(TextView::XAlign::CENTER)
		                                ->set_y_alignment(TextView::YAlign::UP);

		    comment_tab_view->set_on_child_drawn(2, [](const ScrollView &, int) {
			    if (cur_video_info.has_more_comments() && cur_video_info.error == "") {
				    if (!is_async_task_running(load_video_page) && !is_async_task_running(load_more_comments)) {
					    queue_async_task(load_more_comments, &cur_video_info);
				    }
			    }
		    });
		    comments_bottom_view = bottom_view;
	    } else {
		    comments_bottom_view = new EmptyView(0, 0, 320, 0);
	    }
	    comment_tab_view->views[2] = comments_bottom_view;
    }
#define COMMENT_MAX_LINE_NUM 1000 // this limit exists due to performance reason (TODO : more efficient truncating)
    // the token of the comment page that has this comment (the first page : the one of the section), for the account actions
    static std::string comment_actions_token(const std::string &comment_id) {
	    for (auto &c : cur_video_info.comments) {
		    if (c.id == comment_id && c.page_token != "") return c.page_token;
	    }
	    return cur_video_info.comments_account_token();
    }
    static PostView *comment_to_view(const YouTubeVideoDetail::Comment &comment, int comment_index) {
	    auto &cur_content = comment.content;
	    std::vector<std::string> cur_lines;
	    auto itr = cur_content.begin();
	    while (itr != cur_content.end()) {
		    if (cur_lines.size() >= COMMENT_MAX_LINE_NUM) {
			    break;
		    }
		    auto next_itr = std::find(itr, cur_content.end(), '\n');
		    auto tmp = truncate_str(std::string(itr, next_itr), COMMENT_MAX_WIDTH,
		                            COMMENT_MAX_LINE_NUM - cur_lines.size(), 0.5, 0.5);
		    cur_lines.insert(cur_lines.end(), tmp.begin(), tmp.end());

		    if (next_itr != cur_content.end()) {
			    itr = std::next(next_itr);
		    } else {
			    break;
		    }
	    }
	    std::string author_id = comment.author.id;
	    PostView *result =
	        (new PostView(0, 0, 320))
	            ->set_author_name(comment.author.name)
	            ->set_author_icon_url(comment.author.icon_url)
	            ->set_time_str((comment.pinned ? tr("Pinned", "Fijado") + " | " : std::string()) + (comment.hearted ? "<3 | " : "") + comment.publish_date)
	            ->set_upvote_str(comment.upvotes_str)
	            ->set_content_lines(cur_lines)
	            ->set_has_more_replies(
	                [comment_index]() { return cur_video_info.comments[comment_index].has_more_replies(); })
	            ->set_on_author_icon_pressed([author_id](const PostView &view) { channel_id_pressed = author_id; })
	            ->set_on_load_more_replies_pressed([comment_index](PostView &view) {
		            queue_async_task(load_more_replies, (void *)comment_index);
		            view.is_loading_replies = true;
	            })
	            ->set_on_timestamp_pressed([](double seconds) {
		            send_seek_request_wo_lock(seconds);
		            var_need_refresh = true;
	            });

	    {
		    std::string cid = comment.id; // long press : like / reply with the YouTube account
		    result->add_on_long_hold(35, [cid](View &view) {
			    comment_menu_request = cid;
			    view.reset_holding_status();
			    var_need_refresh = true;
		    });
	    }

	    for (size_t i = 0; i < comment.replies.size(); i++) {
		    auto &cur_reply = comment.replies[i];
		    auto &cur_content = cur_reply.content;
		    std::vector<std::string> cur_reply_lines;
		    auto itr = cur_content.begin();
		    while (itr != cur_content.end()) {
			    if (cur_reply_lines.size() >= COMMENT_MAX_LINE_NUM) {
				    break;
			    }
			    auto next_itr = std::find(itr, cur_content.end(), '\n');
			    auto tmp = truncate_str(std::string(itr, next_itr), REPLY_MAX_WIDTH,
			                            COMMENT_MAX_LINE_NUM - cur_reply_lines.size(), 0.5, 0.5);
			    cur_reply_lines.insert(cur_reply_lines.end(), tmp.begin(), tmp.end());

			    if (next_itr != cur_content.end()) {
				    itr = std::next(next_itr);
			    } else {
				    break;
			    }
		    }
		    std::string reply_author_id = cur_reply.author.id;
		    result->replies.push_back((new PostView(REPLY_INDENT, 0, 320 - REPLY_INDENT))
		                                  ->set_author_name(cur_reply.author.name)
		                                  ->set_author_icon_url(cur_reply.author.icon_url)
		                                  ->set_time_str(cur_reply.publish_date)
		                                  ->set_upvote_str(cur_reply.upvotes_str)
		                                  ->set_content_lines(cur_reply_lines)
		                                  ->set_has_more_replies([]() { return false; })
		                                  ->set_on_author_icon_pressed([reply_author_id](const PostView &view) {
			                                  channel_id_pressed = reply_author_id;
		                                  })
		                                  ->set_is_reply(true)
		                                  ->set_on_timestamp_pressed([](double seconds) {
			                                  send_seek_request_wo_lock(seconds);
			                                  var_need_refresh = true;
		                                  }));
	    }

	    return result;
    }

    // arg :
    //   cur_playing_url : only update the data for the player
    //   cur_displaying_url : only update the displayed data
    //   NULL : both
    static void load_video_page(void *arg) {
	    small_resource_lock.lock();
	    bool is_to_play = ((const std::string *)arg) == &cur_playing_url;
	    bool is_to_display = ((const std::string *)arg) == &cur_displaying_url;
	    std::string url;
	    if (!arg) {
		    is_to_play = is_to_display = true;
		    url = cur_playing_url;
	    } else {
		    url = *((const std::string *)arg);
	    }
	    YouTubeVideoDetail tmp_video_info;
	    bool need_loading = false;
	    if (video_info_cache.count(url)) {
		    tmp_video_info = video_info_cache[url];
	    } else {
		    need_loading = true;
	    }
	    if (need_loading && is_to_display) {
		    static TextView *loading_view = (new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL))
		                                        ->set_text([]() { return loading_text(); })
		                                        ->set_x_alignment(TextView::XAlign::CENTER)
		                                        ->set_y_alignment(TextView::YAlign::UP);
		    for (int i = 0; i < 4; i++) {
			    main_view->views[i] = loading_view;
		    }
		    // tab #4(Playback) and #5(Playlist) remain displayed
		    var_need_refresh = true;
	    }
	    small_resource_lock.unlock();

	    if (need_loading) {
		    logger.info("player/load-v", "request : " + url);
		    add_cpu_limit(ADDITIONAL_CPU_LIMIT);
		    if (url.compare(0, 8, "offline:") == 0) { // audio saved on the SD card : no network needed
			    tmp_video_info = offline_video_info(url.substr(8));
		    } else if (var_wifi_state != 2 && offline_has_id(youtube_get_video_id_by_url(url))) {
			    // no internet but this video is saved on the SD card : play that copy
			    tmp_video_info = offline_video_info(youtube_get_video_id_by_url(url));
			    notice_show(tr("No connection: playing the saved copy", "Sin conexión: se usa la copia guardada"));
		    } else {
			    tmp_video_info = youtube_load_video_page(url);
		    }
		    remove_cpu_limit(ADDITIONAL_CPU_LIMIT);

		    // the user asked for another video while this one was loading : drop this result, the new request is queued
		    small_resource_lock.lock();
		    bool stale = tmp_video_info.error == "cancelled" || (is_to_play && cur_playing_url != url) ||
		                 (is_to_display && cur_displaying_url != url);
		    small_resource_lock.unlock();
		    if (stale) {
			    logger.info("player/load-v", "stale/cancelled load dropped : " + url);
			    return;
		    }
	    }

	    if (is_to_display) {
		    logger.info("player/load-v", "truncate/view creation start");
		    // prepare views in the main tab
		    std::vector<View *> main_tab_views;
		    ImageView *new_main_icon_view = (new ImageView(0, 0, ICON_SIZE, ICON_SIZE));
		    if (tmp_video_info.title == "" && tmp_video_info.playability_reason != "") {
			    // Check if it's an age-restricted video
			    std::string display_message = tmp_video_info.playability_reason;
			    if (tmp_video_info.playability_status == "UNPLAYABLE" ||
			        tmp_video_info.playability_status == "LOGIN_REQUIRED" ||
			        tmp_video_info.playability_reason.find("age") != std::string::npos ||
			        tmp_video_info.playability_reason.find("restricted") != std::string::npos ||
			        tmp_video_info.playability_reason.find("sign in") != std::string::npos) {
				    display_message = LOCALIZED(AGE_RESTRICTED_VIDEO);
			    }
			    main_tab_views = {(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL * 6))
			                          ->set_text_lines(split_string(display_message, '\n'))
			                          ->set_x_alignment(TextView::XAlign::CENTER)
			                          ->set_font_size(0.45, DEFAULT_FONT_INTERVAL)};
		    } else {
			    // wrap main title
			    float title_font_size;
			    std::vector<std::string> title_lines =
			        truncate_str(tmp_video_info.title, TITLE_MAX_WIDTH, 3, MIDDLE_FONT_SIZE, MIDDLE_FONT_SIZE);
			    if (title_lines.size() == 3) {
				    title_lines = truncate_str(tmp_video_info.title, TITLE_MAX_WIDTH, 2, 0.5, 0.5);
				    title_font_size = 0.5;
			    } else {
				    title_font_size = MIDDLE_FONT_SIZE;
			    }

			    std::string author_id = tmp_video_info.author.id;
			    bool is_collab = tmp_video_info.author.is_collaboration;
			    std::string collab_video_id = tmp_video_info.id;
			    // a collaboration : tapping the author opens the screen with all the channels
			    auto author_pressed = [author_id, is_collab, collab_video_id](View &) {
				    if (is_collab) collab_request_id = collab_video_id;
				    else channel_id_pressed = author_id;
			    };
			    new_main_icon_view
			        ->set_handle(thumbnail_request(tmp_video_info.author.icon_url, SceneType::VIDEO_PLAYER, 1000,
			                                       ThumbnailType::ICON))
			        ->set_on_view_released(author_pressed);
			    main_tab_views = {
			        (new TextView(0, 0, 320, 15 * title_lines.size())) // title
			            ->set_text_lines(title_lines)
			            ->set_font_size(title_font_size, 15),
			        (new EmptyView(0, 0, 320, SMALL_MARGIN)),
			        (new HorizontalListView(0, 0, DEFAULT_FONT_INTERVAL)) // view count
			            ->set_views({
			                (new TextView(0, 0, 160, DEFAULT_FONT_INTERVAL))
			                    ->set_text(tmp_video_info.views_str)
			                    ->set_get_text_color([]() { return LIGHT0_TEXT_COLOR; }),
			            }),
			        (new HorizontalListView(0, 0, DEFAULT_FONT_INTERVAL)) // publish date
			            ->set_views({(new TextView(0, 0, 160, DEFAULT_FONT_INTERVAL))
			                             ->set_text(tmp_video_info.publish_date)
			                             ->set_x_alignment(TextView::XAlign::LEFT)
			                             ->set_get_text_color([]() { return LIGHT0_TEXT_COLOR; })}),
			        (new CustomView(0, 0, 320, DEFAULT_FONT_INTERVAL)) // like/dislike
			            ->set_draw([](const CustomView &view) {
				            int y = view.y0;
				            float x = view.x0 + SMALL_MARGIN;
				            Draw_texture(var_texture_thumb_up[var_night_mode], x, y, 16, 16);
				            x += 16 + SMALL_MARGIN;
				            Draw(cur_video_info.like_count_str, x, y, 0.5, 0.5, LIGHT0_TEXT_COLOR);
				            x += Draw_get_width(cur_video_info.like_count_str, 0.5) + SMALL_MARGIN * 2;

				            Draw_texture(var_texture_thumb_down[var_night_mode], x, y, 16, 16);
				            x += 16 + SMALL_MARGIN;
				            Draw(cur_video_info.dislike_count_str, x, y, 0.5, 0.5, LIGHT0_TEXT_COLOR);
			            }),
			        (new RuleView(0, 0, 320, SMALL_MARGIN * 2))->set_get_color([]() { return DEF_DRAW_GRAY; }),
			        (new HorizontalListView(0, 0, ICON_SIZE)) // author
			            ->set_views(
			                {(new EmptyView(0, 0, SMALL_MARGIN, ICON_SIZE)), new_main_icon_view,
			                 (new EmptyView(0, 0, SMALL_MARGIN, ICON_SIZE)),
			                 (new VerticalListView(0, 0,
			                                       tmp_video_info.author.is_collaboration
			                                           ? 320 - SMALL_MARGIN * 2 - ICON_SIZE
			                                           : 320 - SMALL_MARGIN * 3 - ICON_SIZE - SUBSCRIBE_BUTTON_WIDTH))
			                     ->set_views(
			                         {(new EmptyView(0, 0,
			                                         tmp_video_info.author.is_collaboration
			                                             ? 320 - SMALL_MARGIN * 2 - ICON_SIZE
			                                             : 320 - SMALL_MARGIN * 3 - ICON_SIZE - SUBSCRIBE_BUTTON_WIDTH,
			                                         ICON_SIZE * 0.1)),
			                          (new TextView(0, 0,
			                                        tmp_video_info.author.is_collaboration
			                                            ? 320 - SMALL_MARGIN * 2 - ICON_SIZE
			                                            : 320 - SMALL_MARGIN * 3 - ICON_SIZE - SUBSCRIBE_BUTTON_WIDTH,
			                                        DEFAULT_FONT_INTERVAL + SMALL_MARGIN))
			                              ->set_text(tmp_video_info.author.name)
			                              ->set_font_size(0.55, DEFAULT_FONT_INTERVAL + SMALL_MARGIN)
			                              ->set_on_view_released(author_pressed),
			                          (new TextView(0, 0,
			                                        tmp_video_info.author.is_collaboration
			                                            ? 320 - SMALL_MARGIN * 2 - ICON_SIZE
			                                            : 320 - SMALL_MARGIN * 3 - ICON_SIZE - SUBSCRIBE_BUTTON_WIDTH,
			                                        DEFAULT_FONT_INTERVAL))
			                              ->set_text(tmp_video_info.author.is_collaboration
			                                             ? tr("Tap to see all the channels", "Toca para ver todos los canales")
			                                         : tmp_video_info.author.subscribers.empty()
			                                             ? ""
			                                             : (tmp_video_info.metadata_from_android_vr
			                                                    ? tmp_video_info.author.subscribers
			                                                    : std::regex_replace(
			                                                          LOCALIZED(SUBSCRIBER_COUNT), std::regex("%0"),
			                                                          tmp_video_info.author.subscribers)))
			                              ->set_font_size(0.45, DEFAULT_FONT_INTERVAL + SMALL_MARGIN)
			                              ->set_get_text_color([]() { return LIGHT1_TEXT_COLOR; })}),
			                 tmp_video_info.author.is_collaboration
			                     ? (View *)(new EmptyView(0, 0, 0, ICON_SIZE))
			                     : (View *)(new CustomView(0, 0, SUBSCRIBE_BUTTON_WIDTH, ICON_SIZE))
			                           ->set_draw([author_id](const CustomView &view) {
				                           bool is_subscribed = subscription_is_subscribed(author_id);
				                           u32 button_color = is_subscribed ? LIGHT1_BACK_COLOR : 0xFF4040EE;
				                           std::string button_text =
				                               is_subscribed ? LOCALIZED(SUBSCRIBED) : LOCALIZED(SUBSCRIBE);
				                           float button_y = view.y0 + (ICON_SIZE - SUBSCRIBE_BUTTON_HEIGHT) / 2;
				                           Draw_texture(var_square_image[0], button_color, view.x0, button_y,
				                                        SUBSCRIBE_BUTTON_WIDTH, SUBSCRIBE_BUTTON_HEIGHT);
				                           Draw_x_centered(button_text, view.x0, view.x0 + SUBSCRIBE_BUTTON_WIDTH,
				                                           button_y + 4, 0.5, 0.5, 0xFF000000);
			                           })
			                           ->set_update([author_id](CustomView &view, Hid_info key) {
				                           float button_y = view.y0 + (ICON_SIZE - SUBSCRIBE_BUTTON_HEIGHT) / 2;
				                           bool in_button = key.touch_x >= view.x0 &&
				                                            key.touch_x < view.x0 + SUBSCRIBE_BUTTON_WIDTH &&
				                                            key.touch_y >= button_y &&
				                                            key.touch_y < button_y + SUBSCRIBE_BUTTON_HEIGHT;
				                           if (key.p_touch && in_button) {
					                           bool cur_subscribed = subscription_is_subscribed(author_id);
					                           if (cur_subscribed) {
						                           subscription_unsubscribe(author_id);
					                           } else {
						                           SubscriptionChannel new_channel;
						                           new_channel.id = author_id;
						                           new_channel.url = "https://m.youtube.com/channel/" + author_id;
						                           new_channel.name = cur_video_info.author.name;
						                           new_channel.icon_url = cur_video_info.author.icon_url;
						                           new_channel.subscriber_count_str = cur_video_info.author.subscribers;
						                           subscription_subscribe(new_channel);
					                           }
					                           misc_tasks_request(TASK_SAVE_SUBSCRIPTION);
					                           Home_update_local_channels();
					                           account_subscribe_async(author_id, !cur_subscribed); // also on the YouTube account when signed in
					                           var_need_refresh = true;
				                           }
			                           })}),
			        (new RuleView(0, 0, 320, SMALL_MARGIN * 2))->set_get_color([]() { return DEF_DRAW_GRAY; })};
			    if (tmp_video_info.is_upcoming) {
				    std::vector<View *> add_views = {
				        (new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL))
				            ->set_text((std::function<std::string()>)[reason = tmp_video_info.playability_reason,
				                                                      start = tmp_video_info.upcoming_start_time]() -> std::string {
					            if (start <= 0) return reason;
					            long long left = start - (long long)time(NULL);
					            if (left <= 0) return reason + " (" + tr("starting now", "empieza ya") + ")";
					            char buf[32];
					            snprintf(buf, sizeof buf, "%lld:%02lld:%02lld", left / 3600, left / 60 % 60, left % 60);
					            var_need_refresh = true; // the countdown keeps changing
					            return tr("Starts in ", "Empieza en ") + buf;
				            }),
				        (new RuleView(0, 0, 320, SMALL_MARGIN * 2))->set_get_color([]() { return DEF_DRAW_GRAY; })};
				    main_tab_views.insert(main_tab_views.end(), add_views.begin(), add_views.end());
			    }
			    {
				    TextView *play_button = (new TextView(0, 0, 100, 20));
				    play_button
				        ->set_text((std::function<std::string()>)[]() {
					        return cur_video_info.id == playing_video_info.id ? LOCALIZED(PLAYING) : LOCALIZED(PLAY);
				        })
				        ->set_x_alignment(TextView::XAlign::CENTER);
				    if (tmp_video_info.is_playable()) {
					    play_button
					        ->set_on_view_released([url](View &) {
						        if (cur_video_info.id != playing_video_info.id) {
							        send_change_video_request_wo_lock(url, true, false, false);
						        }
					        })
					        ->set_get_background_color([](const View &view) -> u32 {
						        if (cur_video_info.id == playing_video_info.id) {
							        return 0xFFFFBBBB;
						        }
						        return View::STANDARD_BACKGROUND(view);
					        });
				    } else {
					    play_button->set_get_background_color([](const View &) { return LIGHT1_BACK_COLOR; });
					    logger.info("video_player", "Video is not playable: " + tmp_video_info.playability_reason);
				    }
				    TextView *reload_button = (new TextView(0, 0, 100, 20));
				    TextView *favorite_button = (new TextView(0, 0, 100, 20));
				    favorite_button
				        ->set_text((std::function<std::string()>)[]() {
					        return favorites_contains(cur_video_info.id) ? std::string(tr("Remove fav", "Quitar fav")) : std::string(tr("+ Favorite", "+ Favorito"));
				        })
				        ->set_x_alignment(TextView::XAlign::CENTER)
				        ->set_get_background_color(View::STANDARD_BACKGROUND)
				        ->set_on_view_released([](View &) {
					        if (cur_video_info.id != "" && cur_video_info.title != "") {
						        FavoriteVideo fav;
						        fav.id = cur_video_info.id;
						        fav.title = cur_video_info.title;
						        fav.author = cur_video_info.author.name;
						        fav.length_text = Util_convert_seconds_to_time((double)cur_video_info.duration_ms / 1000);
						        favorites_toggle(fav);
						        var_need_refresh = true;
					        }
				        });
				    reload_button->set_text((std::function<std::string()>)[]() { return LOCALIZED(RELOAD); })
				        ->set_x_alignment(TextView::XAlign::CENTER)
				        ->set_get_background_color(View::STANDARD_BACKGROUND)
				        ->set_on_view_released([url](View &) {
					        if (!is_async_task_running(load_video_page)) {
						        send_change_video_request_wo_lock(cur_displaying_url, false, true, true);
						        video_retry_left = MAX_RETRY_CNT;
					        }
				        });

				    TextView *qr_button = (new TextView(0, 0, 50, 20));
				    qr_button->set_text("QR")
				        ->set_x_alignment(TextView::XAlign::CENTER)
				        ->set_get_background_color(View::STANDARD_BACKGROUND)
				        ->set_on_view_released([](View &) {
					        if (cur_video_info.id != "") {
						        std::string link = "https://www.youtube.com/watch?v=" + cur_video_info.id;
						        if (cur_video_info.id == playing_video_info.id && vid_current_pos > 10) { // also share the current moment
							        link += "&t=" + std::to_string((int)vid_current_pos) + "s";
						        }
						        qr_popup_show(link);
					        }
				        });
				    play_button->set_width(75);
				    reload_button->set_width(75);
				    favorite_button->set_width(85);

				    main_tab_views.push_back((new HorizontalListView(0, 0, 20))
				                                 ->set_views({(new EmptyView(0, 0, SMALL_MARGIN, 20)), play_button,
				                                              reload_button, favorite_button, qr_button}));
				    main_tab_views.push_back(
				        (new RuleView(0, 0, 320, SMALL_MARGIN * 2))->set_get_color([]() { return DEF_DRAW_GRAY; }));
			    }
			    {
				    // tools : jump 10 seconds back / forward (touch is only on the bottom screen, so these replace double tap), A-B loop
				    auto make_tool = [](std::function<std::string()> text, int width, std::function<void()> action) {
					    return (new TextView(0, 0, width, 20))
					        ->set_text(text)
					        ->set_x_alignment(TextView::XAlign::CENTER)
					        ->set_get_background_color(View::STANDARD_BACKGROUND)
					        ->set_on_view_released([action](View &) { action(); });
				    };
				    auto jump = [](double delta) {
					    if (network_decoder.ready && cur_video_info.id == playing_video_info.id) {
						    double pos = std::min<double>(vid_duration, std::max<double>(0, vid_current_pos + delta));
						    send_seek_request_wo_lock(pos);
					    }
				    };
				    main_tab_views.push_back(
				        (new HorizontalListView(0, 0, 20))
				            ->set_views({(new EmptyView(0, 0, SMALL_MARGIN, 20)),
				                         make_tool([]() { return std::string("-10 s"); }, 70, [jump]() { jump(-10); }),
				                         make_tool([]() { return std::string("+10 s"); }, 70, [jump]() { jump(10); }),
				                         make_tool(
				                             []() {
					                             if (ab_loop_a < 0) return std::string("A-B");
					                             if (ab_loop_b < 0) return std::string("A-B: A");
					                             return std::string("A-B: ") + Util_convert_seconds_to_time(ab_loop_a) + "-" + Util_convert_seconds_to_time(ab_loop_b);
				                             },
				                             150,
				                             []() {
					                             if (cur_video_info.id != playing_video_info.id || !network_decoder.ready) return;
					                             if (ab_loop_a < 0) {
						                             ab_loop_a = vid_current_pos;
					                             } else if (ab_loop_b < 0) {
						                             if (vid_current_pos > ab_loop_a + 1) ab_loop_b = vid_current_pos;
					                             } else {
						                             ab_loop_a = ab_loop_b = -1;
					                             }
					                             var_need_refresh = true;
				                             })}));
				    // second row : quick speed, next chapter, watch later, bookmark, mini player
				    auto is_current = []() { return network_decoder.ready && cur_video_info.id == playing_video_info.id; };
				    main_tab_views.push_back(
				        (new HorizontalListView(0, 0, 20))
				            ->set_views(
				                {(new EmptyView(0, 0, SMALL_MARGIN, 20)),
				                 make_tool([]() { return std::to_string((int)std::round(quick_speed * 100)) + "%"; }, 52,
				                           []() {
					                           static const double steps[] = {0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0};
					                           int i = 0;
					                           for (int k = 0; k < 7; k++) if (steps[k] <= quick_speed + 0.01) i = k;
					                           quick_speed = steps[(i + 1) % 7];
					                           network_decoder.set_tempo(quick_speed);
					                           channel_speed_set(playing_video_info.author.id, quick_speed); // remembered for this channel
					                           if (speed_bar_view) speed_bar_view->set_value(std::min(1.5, quick_speed));
					                           var_need_refresh = true;
				                           }),
				                 make_tool([]() { return tr("Chap >>", "Cap >>"); }, 60,
				                           [is_current]() {
					                           if (!is_current()) return;
					                           for (auto &chapter : current_chapters) {
						                           if (chapter.first > vid_current_pos + 1) {
							                           send_seek_request_wo_lock(chapter.first);
							                           break;
						                           }
					                           }
				                           }),
				                 make_tool([]() { return watch_later_contains(cur_video_info.id) ? tr("Later: yes", "Luego: sí") : tr("Later", "Luego"); }, 68,
				                           []() {
					                           if (cur_video_info.id != "" && cur_video_info.title != "") {
						                           FavoriteVideo v;
						                           v.id = cur_video_info.id;
						                           v.title = cur_video_info.title;
						                           v.author = cur_video_info.author.name;
						                           v.length_text = Util_convert_seconds_to_time((double)cur_video_info.duration_ms / 1000);
						                           watch_later_toggle(v);
						                           var_need_refresh = true;
					                           }
				                           }),
				                 make_tool([]() { return tr("+Mark", "+Marca"); }, 56,
				                           [is_current]() {
					                           if (is_current()) bookmark_add_request = true;
				                           }),
				                 make_tool([]() { return std::string("Mini"); }, 50, []() { mini_visible = true; var_need_refresh = true; })}));
				    // third row : skip 30 s (intros), stop after this video
				    main_tab_views.push_back(
				        (new HorizontalListView(0, 0, 20))
				            ->set_views(
				                {(new EmptyView(0, 0, SMALL_MARGIN, 20)),
				                 make_tool([]() { return std::string("+30 s"); }, 70, [jump]() { jump(30); }),
				                 make_tool(
				                     []() {
					                     return stop_after_video ? tr("Stop after: yes", "Parar al acabar: sí") : tr("Stop after video", "Parar al acabar");
				                     },
				                     110,
				                     []() {
					                     stop_after_video = !stop_after_video;
					                     var_need_refresh = true;
				                     }),
				                 make_tool([]() { return tr("Save audio", "Guardar audio"); }, 90,
				                           []() {
					                           if (cur_video_info.id != "")
						                           offline_download_async(cur_video_info.id, cur_video_info.title, cur_video_info.author.name,
						                                                  cur_video_info.audio_stream_url);
				                           }),
				                 make_tool([]() { return offline_has_id(cur_video_info.id) ? tr("Saved offline", "Guardado") : tr("Save video", "Guardar vídeo"); }, 90,
				                           []() {
					                           if (cur_video_info.id == "") return;
					                           // the saved quality : 360p at most (240p on the old 3DS), the closest one that exists
					                           int wanted = var_is_new3ds ? 360 : 240, quality = 0;
					                           std::string video_url;
					                           for (auto &i : cur_video_info.video_stream_urls) {
						                           if (i.first <= wanted && i.first > quality) {
							                           quality = i.first;
							                           video_url = i.second;
						                           }
					                           }
					                           offline_download_async(cur_video_info.id, cur_video_info.title, cur_video_info.author.name,
					                                                  cur_video_info.audio_stream_url, true, video_url,
					                                                  var_is_new3ds ? cur_video_info.both_stream_url : "", quality);
				                           })}));
				    // live chat (live streams) and the screens of the video's extra information
				    main_tab_views.push_back(
				        (new HorizontalListView(0, 0, 20))
				            ->set_views({(new EmptyView(0, 0, SMALL_MARGIN, 20)),
				                         make_tool([]() { return tr("Live chat", "Chat en directo"); }, 100,
				                                   []() {
					                                   if (!cur_video_info.is_livestream) {
						                                   notice_show(tr("This video is not a live stream", "Este vídeo no es un directo"));
					                                   } else if (cur_video_info.id != "") {
						                                   chat_request = true;
						                                   var_need_refresh = true;
					                                   }
				                                   })}));
				    // fourth row : actions on your YouTube account (OAuth login needed)
				    main_tab_views.push_back(
				        (new HorizontalListView(0, 0, 20))
				            ->set_views({(new EmptyView(0, 0, SMALL_MARGIN, 20)),
				                         make_tool([]() { return tr("Like", "Me gusta"); }, 60,
				                                   []() { if (cur_video_info.id != "") account_like_async(cur_video_info.id, 1); }),
				                         make_tool([]() { return tr("Dislike", "No me gusta"); }, 80,
				                                   []() { if (cur_video_info.id != "") account_like_async(cur_video_info.id, -1); }),
				                         make_tool([]() { return tr("My list", "Mi lista"); }, 70,
				                                   []() { if (cur_video_info.id != "") account_playlists_async(cur_video_info.id); }),
				                         make_tool([]() { return tr("Bell", "Campana"); }, 60,
				                                   []() {
					                                   static int bell_level = 0; // all -> personalized -> none -> all ...
					                                   if (cur_video_info.author.id != "") {
						                                   account_bell_async(cur_video_info.author.id, bell_level);
						                                   bell_level = (bell_level + 1) % 3;
					                                   }
				                                   }),
				                         make_tool([]() { return tr("Not interested", "No me interesa"); }, 100,
				                                   []() { if (cur_video_info.id != "") account_not_interested_async(cur_video_info.id, false); }),
				                         make_tool([]() { return tr("Hide channel", "Ocultar canal"); }, 100,
				                                   []() { if (cur_video_info.id != "") account_not_interested_async(cur_video_info.id, true); }),
				                         make_tool([]() { return tr("Translate", "Traducir"); }, 80,
				                                   []() {
					                                   if (cur_video_info.title != "")
						                                   translate_async(cur_video_info.title, cur_video_info.title + "\n\n" + cur_video_info.description);
				                                   })}));
				    main_tab_views.push_back(
				        (new RuleView(0, 0, 320, SMALL_MARGIN * 2))->set_get_color([]() { return DEF_DRAW_GRAY; }));

				    // chapters : lines of the description that start with a timestamp ("0:00 Intro")
				    std::vector<std::pair<double, std::string>> chapters;
				    {
					    std::istringstream desc(tmp_video_info.description);
					    std::string line;
					    while (std::getline(desc, line) && chapters.size() < 40) {
						    int ts_start = -1, ts_end = -1;
						    double seconds = 0;
						    if (Util_find_timestamp_in_text(line, 0, &ts_start, &ts_end, &seconds) != -1 && ts_start >= 0 &&
						        ts_start <= 2 && ts_end > ts_start && ts_end <= (int)line.size() && seconds >= 0) {
							    std::string title = line.substr(ts_end);
							    size_t first = title.find_first_not_of(" -\t:|)(");
							    title = first == std::string::npos ? "" : title.substr(first);
							    if (!title.empty()) chapters.push_back({seconds, title});
						    }
					    }
				    }
				    pending_chapters = chapters.size() >= 2 ? chapters : decltype(chapters)(); // installed with the page (the UI thread reads current_chapters)
				    // bookmarks of this video (the list is refilled whenever one is added or removed)
				    pending_bookmarks_video_id = tmp_video_info.id;
				    pending_bookmarks_list_view = new VerticalListView(0, 0, 320);
				    main_tab_views.push_back(pending_bookmarks_list_view);
				    if (chapters.size() >= 2) {
					    main_tab_views.push_back((new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL))
					                                 ->set_text(tr("Chapters", "Capítulos"))
					                                 ->set_font_size(0.5, DEFAULT_FONT_INTERVAL)
					                                 ->set_get_text_color([]() { return LIGHT1_TEXT_COLOR; }));
					    for (auto &chapter : chapters) {
						    double seconds = chapter.first;
						    main_tab_views.push_back(
						        (new TextView(SMALL_MARGIN, 0, 320 - SMALL_MARGIN * 2, 22))
						            ->set_text(Util_convert_seconds_to_time(seconds) + "  " + truncate_first_line(chapter.second, 250, 0.5))
						            ->set_font_size(0.5, DEFAULT_FONT_INTERVAL)
						            ->set_y_alignment(TextView::YAlign::CENTER)
						            ->set_get_background_color(View::STANDARD_BACKGROUND)
						            ->set_on_view_released([seconds](View &) {
							            if (network_decoder.ready && cur_video_info.id == playing_video_info.id)
								            send_seek_request_wo_lock(seconds);
						            }));
					    }
					    main_tab_views.push_back(
					        (new RuleView(0, 0, 320, SMALL_MARGIN * 2))->set_get_color([]() { return DEF_DRAW_GRAY; }));
				    }
			    }
			    {
				    std::vector<std::string> description_lines;
				    auto &description = tmp_video_info.description;
				    auto itr = description.begin();
				    while (itr != description.end()) {
					    auto next_itr = std::find(itr, description.end(), '\n');
					    auto cur_lines = truncate_str(std::string(itr, next_itr), DESC_MAX_WIDTH, 100, 0.5, 0.5);
					    description_lines.insert(description_lines.end(), cur_lines.begin(), cur_lines.end());
					    if (next_itr != description.end()) {
						    itr = std::next(next_itr);
					    } else {
						    break;
					    }
				    }

				    // Create PostView for description to enable timestamp clicking
				    PostView *description_view = (new PostView(0, 0, 320))
				                                     ->set_is_description_mode(true)
				                                     ->set_author_name("")
				                                     ->set_author_icon_url("")
				                                     ->set_time_str("")
				                                     ->set_upvote_str("")
				                                     ->set_content_lines(description_lines)
				                                     ->set_has_more_replies([]() { return false; })
				                                     ->set_on_timestamp_pressed([](double seconds) {
					                                     send_seek_request_wo_lock(seconds);
					                                     var_need_refresh = true;
				                                     });
				    description_view->lines_shown = description_lines.size();

				    std::vector<View *> add_views = {
				        description_view,
				        (new RuleView(0, 0, 320, SMALL_MARGIN * 2))->set_get_color([]() { return DEF_DRAW_GRAY; })};
				    main_tab_views.insert(main_tab_views.end(), add_views.begin(), add_views.end());
			    }
		    }
		    // prepare suggestion view
		    std::vector<View *> new_suggestion_views;
		    for (size_t i = 0; i < tmp_video_info.suggestions.size(); i++) {
			    new_suggestion_views.push_back(suggestion_to_view(tmp_video_info.suggestions[i]));
		    }

		    // prepare comment views (comments exist from the first if it's loaded from cache)
		    std::vector<View *> new_comment_views;
		    for (size_t i = 0; i < tmp_video_info.comments.size(); i++) {
			    new_comment_views.push_back(comment_to_view(tmp_video_info.comments[i], i));
		    }

		    // prepare captions view
		    static std::string selected_base_lang = "";
		    static std::string selected_translation_lang = "";
		    static std::pair<std::string *, std::string *> load_caption_arg = {&selected_base_lang,
		                                                                       &selected_translation_lang};

		    selected_base_lang =
		        tmp_video_info.caption_base_languages.size() ? tmp_video_info.caption_base_languages[0].id : "";
		    { // the language chosen the last time is preselected when this video has it
			    std::string remembered = caption_lang_remembered();
			    for (auto &lang : tmp_video_info.caption_base_languages) {
				    if (remembered != "" && lang.id == remembered) selected_base_lang = lang.id;
			    }
		    }
		    selected_translation_lang = "";

		    int exit_button_height = DEFAULT_FONT_INTERVAL * 1.5;
		    View *exit_button_view =
		        (new HorizontalListView(0, 0, exit_button_height))
		            ->set_views({(new EmptyView(0, 0, 320 - 100, exit_button_height))
		                             ->set_get_background_color([](const View &) { return DEFAULT_BACK_COLOR; }),
		                         (new TextView(0, 0, 100, exit_button_height))
		                             ->set_text((std::function<std::string()>)[]() { return LOCALIZED(OK); })
		                             ->set_x_alignment(TextView::XAlign::CENTER)
		                             ->set_text_offset(0, -2.0)
		                             ->set_background_color(COLOR_GRAY(0x80))
		                             ->set_on_view_released(
		                                 [](View &view) {
			                                 caption_lang_remember(*load_caption_arg.first);
			                                 queue_async_task(load_caption, &load_caption_arg);
		                                 })})
		            ->set_draw_order({1, 0});

		    int language_selector_height = CONTENT_Y_HIGH - exit_button_view->get_height();
		    // base languages
		    VerticalListView *caption_left_view = new VerticalListView(0, 0, 160);
		    caption_left_view->views.push_back(
		        (new TextView(0, 0, 160, DEFAULT_FONT_INTERVAL * 1.2))
		            ->set_text((std::function<std::string()>)[]() { return LOCALIZED(CAPTION_BASE_LANGUAGES); })
		            ->set_get_background_color([](const View &) { return DEFAULT_BACK_COLOR; }));
		    caption_left_view->views.push_back((new RuleView(0, 0, 160, 3))->set_get_background_color([](const View &) {
			    return DEFAULT_BACK_COLOR;
		    }));
		    ScrollView *base_languages_selector_view =
		        new ScrollView(0, 0, 160, language_selector_height - caption_left_view->get_height());
		    for (size_t i = 0; i < tmp_video_info.caption_base_languages.size(); i++) {
			    auto &cur_lang = tmp_video_info.caption_base_languages[i];
			    base_languages_selector_view->views.push_back(
			        (new TextView(0, 0, 160, DEFAULT_FONT_INTERVAL))
			            ->set_text(cur_lang.name)
			            ->set_text_offset(0.0, -1.0)
			            ->set_get_background_color([cur_lang](const View &view) {
				            if (cur_lang.id == selected_base_lang) {
					            return COLOR_GRAY(0x80);
				            } else {
					            int darkness = std::min<int>(
					                0xFF, 0xD0 + 0x30 * (1 - std::min(1.0, 0.15 * view.view_holding_time)));
					            if (var_night_mode) {
						            darkness = 0xFF - darkness;
					            }
					            return COLOR_GRAY(darkness);
				            }
			            })
			            ->set_on_view_released([cur_lang](View &view) { selected_base_lang = cur_lang.id; }));
		    }
		    caption_left_view->views.push_back(base_languages_selector_view);
		    caption_left_view->set_draw_order({2, 0, 1});

		    // translation languages
		    VerticalListView *caption_right_view = new VerticalListView(0, 0, 160);
		    ScrollView *translation_languages_selector_view = new ScrollView(0, 0, 160, 0); // dummy height
		caption_right_view->views.push_back(
		    (new SelectorView(0, 0, 160, DEFAULT_FONT_INTERVAL * 2.5, false))
		        ->set_texts({(std::function<std::string()>)[]() { return LOCALIZED(OFF);
	    }
	    , (std::function<std::string()>)[]() { return LOCALIZED(ON); }
    },
		                    0)
		        ->set_title([](const SelectorView &) {
	return LOCALIZED(CAPTION_TRANSLATION); })
		        ->set_on_change([translation_languages_selector_view](const SelectorView &view) {
	translation_languages_selector_view->set_is_visible(view.selected_button)->set_is_touchable(view.selected_button);
		        })
		        ->set_get_background_color([](const View &) {
	return DEFAULT_BACK_COLOR; }));
    translation_languages_selector_view->set_height(language_selector_height - caption_right_view->get_height());
    for (size_t i = 0; i < tmp_video_info.caption_translation_languages.size(); i++) {
	    auto &cur_lang = tmp_video_info.caption_translation_languages[i];
	    TextView *cur_option_view =
	        (new TextView(0, 0, 160, DEFAULT_FONT_INTERVAL))->set_text(cur_lang.name)->set_text_offset(0.0, -1.0);
	    cur_option_view->set_get_background_color([cur_lang](const View &view) {
		    if (cur_lang.id == selected_translation_lang) {
			    return COLOR_GRAY(0x80);
		    } else {
			    int darkness = std::min<int>(0xFF, 0xD0 + 0x30 * (1 - std::min(1.0, 0.15 * view.view_holding_time)));
			    if (var_night_mode) {
				    darkness = 0xFF - darkness;
			    }
			    return COLOR_GRAY(darkness);
		    }
	    });
	    cur_option_view->set_on_view_released([cur_lang](View &view) { selected_translation_lang = cur_lang.id; });
	    translation_languages_selector_view->views.push_back(cur_option_view);
    }
    translation_languages_selector_view->set_is_visible(false)->set_is_touchable(false);
    caption_right_view->views.push_back(translation_languages_selector_view);
    caption_right_view->set_draw_order({1, 0});

    // prepare playlist view
    double playlist_view_scroll = playlist_list_view->get_offset(); // this call should not need mutex locking
    std::vector<View *> new_playlist_views;
    for (auto video : tmp_video_info.playlist.videos) {
	    new_playlist_views.push_back(suggestion_to_view(YouTubeSuccinctItem(video)));
    }
    if (tmp_video_info.playlist.id != "" && tmp_video_info.playlist.id == playing_video_info.playlist.id) {
	    for (size_t i = 0; i < tmp_video_info.playlist.videos.size(); i++) {
		    if (youtube_get_video_id_by_url(tmp_video_info.playlist.videos[i].url) == playing_video_info.id) {
			    new_playlist_views[i]->set_get_background_color([](const View &) { return COLOR_LIGHT_BLUE; });
		    }
	    }
    }
    if (tmp_video_info.playlist.selected_index >= 0 &&
        tmp_video_info.playlist.selected_index < (int)new_playlist_views.size()) {
	    new_playlist_views[tmp_video_info.playlist.selected_index]->set_get_background_color(
	        [](const View &) { return COLOR_LIGHT_GREEN; });
	    double selected_y = tmp_video_info.playlist.selected_index * SUGGESTIONS_VERTICAL_INTERVAL;
	    if (playlist_view_scroll < selected_y - (CONTENT_Y_HIGH - PLAYLIST_TOP_HEIGHT)) {
		    playlist_view_scroll = selected_y - (CONTENT_Y_HIGH - PLAYLIST_TOP_HEIGHT) + SUGGESTIONS_VERTICAL_INTERVAL;
	    }
	    if (playlist_view_scroll > selected_y + SUGGESTIONS_VERTICAL_INTERVAL) {
		    playlist_view_scroll = selected_y;
	    }
    } else {
	    playlist_view_scroll = 0;
    }
    playlist_view_scroll =
        std::min<double>(playlist_view_scroll, tmp_video_info.playlist.videos.size() * SUGGESTIONS_VERTICAL_INTERVAL -
                                                   (CONTENT_Y_HIGH - PLAYLIST_TOP_HEIGHT));
    playlist_view_scroll = std::max<double>(playlist_view_scroll, 0);

    logger.info("player/load-v", "truncate/view creation end");

    // acquire lock and perform actual replacements
    small_resource_lock.lock();
    if (!vid_already_init) { // app shut down while loading
	    small_resource_lock.unlock();
	    return;
    }
    cur_video_info = tmp_video_info;
    if (tmp_video_info.url.compare(0, 8, "offline:") != 0 || url.compare(0, 8, "offline:") == 0) {
	    video_info_cache[url] = tmp_video_info; // (not the saved copy that replaced an online video)
    }
    while (video_info_cache.size() > 5) { // every cached page holds suggestions and comments : keep only a few
	    auto victim = video_info_cache.begin();
	    while (victim != video_info_cache.end() && (victim->first == url || victim->first == cur_displaying_url || victim->first == cur_playing_url)) ++victim;
	    if (victim == video_info_cache.end()) break;
	    video_info_cache.erase(victim);
    }
    if (cur_video_info.playlist.videos.size()) {
	    main_view
	        ->set_views({main_tab_view, suggestion_tab_view, comment_tab_view, captions_tab_view, playback_tab_view,
	                     playlist_tab_view},
	                    TAB_PLAYLIST)
	        ->set_tab_texts<std::function<std::string()>>(
	            {[]() { return LOCALIZED(GENERAL); }, []() { return LOCALIZED(SUGGESTIONS); },
	             []() { return LOCALIZED(COMMENTS); }, []() { return LOCALIZED(CAPTIONS); },
	             []() { return LOCALIZED(PLAYBACK); }, []() { return LOCALIZED(PLAYLIST); }});
    } else {
	    main_view
	        ->set_views({main_tab_view, suggestion_tab_view, comment_tab_view, captions_tab_view, playback_tab_view},
	                    TAB_GENERAL)
	        ->set_tab_texts<std::function<std::string()>>(
	            {[]() { return LOCALIZED(GENERAL); }, []() { return LOCALIZED(SUGGESTIONS); },
	             []() { return LOCALIZED(COMMENTS); }, []() { return LOCALIZED(CAPTIONS); },
	             []() { return LOCALIZED(PLAYBACK); }});
    }

    thumbnail_cancel_request(main_icon_view->handle);
    bookmarks_list_view = NULL; // it is deleted together with the old page
    main_tab_view->recursive_delete_subviews();
    main_icon_view = new_main_icon_view;
    main_tab_view->set_views({main_tab_views});
    bookmarks_list_view = pending_bookmarks_list_view;
    pending_bookmarks_list_view = NULL;
    current_chapters = pending_chapters;
    bookmarks_video_id = pending_bookmarks_video_id;
    bookmarks_rebuild_request = true;

    suggestion_main_view->recursive_delete_subviews();
    suggestion_main_view->views = new_suggestion_views;
    suggestion_tab_view->reset();
    update_suggestion_bottom_view();

    // cancel thumbnail requests
    for (auto view : comment_thumbnail_loaded_list) {
	    thumbnail_cancel_request(view->author_icon_handle);
	    view->author_icon_handle = -1;
    }
    comment_thumbnail_loaded_list.clear();
    comments_main_view->recursive_delete_subviews();
    comments_main_view->views = new_comment_views;
    comment_tab_view->reset();
    update_comment_bottom_view();

    caption_main_view->recursive_delete_subviews();
    caption_overlay_view->set_caption_data({});
    caption_language_select_view->recursive_delete_subviews();
    caption_language_select_view->set_views(
        {exit_button_view, (new HorizontalListView(0, 0, CONTENT_Y_HIGH - exit_button_view->get_height()))
                               ->set_views({caption_left_view, caption_right_view})});
    caption_language_select_view->set_draw_order({1, 0});
    captions_tab_view->set_subview(caption_language_select_view);

    playlist_title_view->set_text(tmp_video_info.playlist.title);
    playlist_author_view->set_text(tmp_video_info.playlist.author_name);
    playlist_list_videos_view->recursive_delete_subviews();
    playlist_list_videos_view->views = new_playlist_views;
    playlist_list_view->set_offset(playlist_view_scroll);

    var_need_refresh = true;
    small_resource_lock.unlock();
    }

    if (is_to_play && tmp_video_info.is_playable()) {
	    small_resource_lock.lock();
	    if (!vid_already_init) { // app shut down while loading
		    small_resource_lock.unlock();
		    return;
	    }
	    if (tmp_video_info.duration_ms > 60 * 60 * 1000 && tmp_video_info.audio_stream_url != "" &&
	        tmp_video_info.video_stream_urls.size()) {
		    tmp_video_info.both_stream_url =
		        ""; // for some reason, itag 18 causes problems when the video is long enough (it is only dropped when
		            // there is another way to play : without a separate audio stream itag 18 is the only option)
	    }
	    if (pending_offline_delete != "" && pending_offline_delete != tmp_video_info.id) { // the finished download is not needed any more
		    offline_remove(pending_offline_delete);
		    video_info_cache.erase("offline:" + pending_offline_delete);
		    pending_offline_delete = "";
	    }
	    playing_video_info = tmp_video_info;
	    if (tmp_video_info.url.compare(0, 8, "offline:") != 0 || url.compare(0, 8, "offline:") == 0) { // the saved copy that replaced an online video is not remembered under its address
		    video_info_cache[url] = tmp_video_info;
	    }
	    vid_resume_id[0] = 0; // the old video may still be decoding : its position must not be saved for the new id.
	                          // The decode thread sets the id when the new video really starts.

	    vid_change_video_request = true;
	    if (network_decoder.ready) {
		    network_decoder.interrupt = true;
	    }

	    thumbnail_cancel_request(cur_playing_video_view->thumbnail_handle);
	    cur_playing_video_view->thumbnail_handle = -1;
	    cur_playing_video_view
	        ->set_title_lines(
	            truncate_str(tmp_video_info.title, 320 - VIDEO_LIST_THUMBNAIL_WIDTH - SMALL_MARGIN * 3, 2, 0.5, 0.5))
	        ->set_thumbnail_url(tmp_video_info.succinct_thumbnail_url)
	        ->set_auxiliary_lines({tmp_video_info.author.name});
	    cur_playing_video_view->thumbnail_handle = thumbnail_request(
	        tmp_video_info.succinct_thumbnail_url, SceneType::VIDEO_PLAYER, 999, ThumbnailType::VIDEO_THUMBNAIL);

	    // prepare the quality selector
	    std::vector<int> available_qualities;
	    for (auto &i : tmp_video_info.video_stream_urls) {
		    available_qualities.push_back(i.first);
	    }

	    video_quality_selector_view->button_texts = {(std::function<std::string()>)[](){return LOCALIZED(OFF);
    }
    }
    ;
    for (auto i : available_qualities) {
	    if (var_is_new3ds || i <= 240) {
		    video_quality_selector_view->button_texts.push_back(std::to_string(i) + "p");
	    }
    }
    video_quality_selector_view->button_num = video_quality_selector_view->button_texts.size();

    auto is_available = [&](int p_value) {
	    return tmp_video_info.video_stream_urls.count(p_value) ||
	           ((p_value == 360 || p_value == 480) && tmp_video_info.both_stream_url != "");
    };
    int wanted_quality = var_video_quality;
    if (var_auto_quality && var_auto_quality_cap > 0 && wanted_quality > var_auto_quality_cap) {
	    wanted_quality = var_auto_quality_cap;
    }
    if (var_data_saver && wanted_quality > 240) {
	    wanted_quality = 240;
    }
    const bool offline_item = tmp_video_info.url.compare(0, 8, "offline:") == 0;
    const bool offline_video = offline_item && !tmp_video_info.video_stream_urls.empty(); // saved with its video
    const bool offline_audio_only = offline_item && !offline_video;
    if (offline_audio_only) {
	    audio_only_mode = true; // saved audio has no video
    } else if (var_video_quality != 0 || offline_video) {
	    audio_only_mode = false; // a previous video (offline or without video streams) must not keep this one audio-only
    }
    if (offline_video) {
	    wanted_quality = tmp_video_info.video_stream_urls.begin()->first; // the saved one
    }
    if ((var_video_quality == 0 && !offline_video) || offline_audio_only) {
	    audio_only_mode = true;
    } else {
	    if (!audio_only_mode && !is_available(wanted_quality)) {
		    video_p_value = var_is_new3ds ? 360 : 144;
		    if (var_is_new3ds && !is_available(video_p_value)) {
			    auto it =
			        std::find_if(available_qualities.rbegin(), available_qualities.rend(),
			                     [&is_available](int quality) { return quality != 480 && is_available(quality); });
			    if (it != available_qualities.rend()) {
				    video_p_value = *it;
			    } else {
				    audio_only_mode = true;
			    }
		    } else {
			    video_p_value = 144;
			    if (!is_available(video_p_value)) {
				    audio_only_mode = true;
			    }
		    }
	    } else {
		    video_p_value = wanted_quality;
	    }
    }

    if (audio_only_mode) {
	    video_quality_selector_view->selected_button = 0;
    } else {
	    auto it = std::find(available_qualities.begin(), available_qualities.end(), (int)video_p_value);
	    if (it != available_qualities.end()) {
		    video_quality_selector_view->selected_button = 1 + (it - available_qualities.begin());
	    } else {
		    auto closest_it = std::min_element(available_qualities.begin(), available_qualities.end(),
		                                       [video_p_value = video_p_value](int a, int b) {
			                                       return std::abs(a - video_p_value) < std::abs(b - video_p_value);
		                                       });
		    if (closest_it != available_qualities.end()) {
			    video_p_value = *closest_it;
			    video_quality_selector_view->selected_button = 1 + (closest_it - available_qualities.begin());
		    } else {
			    audio_only_mode = true;
			    video_quality_selector_view->selected_button = 0;
		    }
	    }
    }
    video_quality_selector_view->set_on_change([available_qualities](const SelectorView &view) {
	    bool changed = false;
	    if (view.selected_button == 0) {
		    if (!audio_only_mode) {
			    changed = true;
		    }
		    audio_only_mode = true;
	    } else {
		    int new_p_value = available_qualities[view.selected_button - 1];
		    if (audio_only_mode || video_p_value != new_p_value) {
			    changed = true;
		    }
		    audio_only_mode = false;
		    video_p_value = new_p_value;
	    }
	    if (changed) {
		    seek_at_init_request = vid_current_pos;
		    vid_change_video_request = true;
		    if (network_decoder.ready) {
			    network_decoder.interrupt = true;
		    }
	    }
	    var_video_quality = audio_only_mode ? 0 : video_p_value;
	    misc_tasks_request(TASK_SAVE_SETTINGS);
    });
    // update playlist tab
    if (cur_video_info.playlist.id != "" && cur_video_info.playlist.id == tmp_video_info.playlist.id) {
	    size_t playlist_len = std::min(std::min(cur_video_info.playlist.videos.size(), tmp_video_info.playlist.videos.size()),
	                                   playlist_list_videos_view->views.size());
	    for (size_t i = 0; i < playlist_len; i++) {
		    if ((int)i == cur_video_info.playlist.selected_index) {
			    playlist_list_videos_view->views[i]->set_get_background_color(
			        [](const View &) { return COLOR_LIGHT_BLUE; });
		    } else if (youtube_get_video_id_by_url(tmp_video_info.playlist.videos[i].url) == tmp_video_info.id) {
			    playlist_list_videos_view->views[i]->set_get_background_color(
			        [](const View &) { return COLOR_LIGHT_GREEN; });
		    } else {
			    playlist_list_videos_view->views[i]->set_get_background_color(View::STANDARD_BACKGROUND);
		    }
	    }
    }
    small_resource_lock.unlock();
    }
    }
    static void load_more_suggestions(void *arg_) {
	    YouTubeVideoDetail *arg = (YouTubeVideoDetail *)arg_;

	    add_cpu_limit(ADDITIONAL_CPU_LIMIT);
	    auto new_result = *arg;
	    new_result.load_more_suggestions();
	    remove_cpu_limit(ADDITIONAL_CPU_LIMIT);

	    // wrap suggestion titles
	    logger.info("player/load-s", "truncate/view creation start");
	    std::vector<View *> new_suggestion_views;
	    for (size_t i = arg->suggestions.size(); i < new_result.suggestions.size(); i++) {
		    new_suggestion_views.push_back(suggestion_to_view(new_result.suggestions[i]));
	    }
	    logger.info("player/load-s", "truncate/view creation end");

	    small_resource_lock.lock();
	    if (!vid_already_init) { // app shut down while loading
		    small_resource_lock.unlock();
		    return;
	    }
	    if (new_result.error != "") {
		    cur_video_info.error = new_result.error;
	    } else {
		    cur_video_info = new_result;
		    suggestion_main_view->views.insert(suggestion_main_view->views.end(), new_suggestion_views.begin(),
		                                       new_suggestion_views.end());
		    update_suggestion_bottom_view();
		    video_info_cache[cur_video_info.url] = new_result;
	    }
	    var_need_refresh = true;
	    small_resource_lock.unlock();
    }

    static void load_more_comments(void *arg_) {
	    YouTubeVideoDetail *arg = (YouTubeVideoDetail *)arg_;

	    add_cpu_limit(ADDITIONAL_CPU_LIMIT);
	    auto new_result = *arg;
	    new_result.load_more_comments();
	    remove_cpu_limit(ADDITIONAL_CPU_LIMIT);

	    std::vector<View *> new_comment_views;
	    // wrap comments
	    logger.info("player/load-c", "truncate/views creation start");
	    for (size_t i = arg->comments.size(); i < new_result.comments.size(); i++) {
		    new_comment_views.push_back(comment_to_view(new_result.comments[i], i));
	    }
	    logger.info("player/load-c", "truncate/views creation end");

	    small_resource_lock.lock();
	    if (!vid_already_init) { // app shut down while loading
		    small_resource_lock.unlock();
		    return;
	    }
	    cur_video_info = new_result;
	    video_info_cache[cur_video_info.url] = new_result;
	    comments_main_view->views.insert(comments_main_view->views.end(), new_comment_views.begin(),
	                                     new_comment_views.end());
	    update_comment_bottom_view();
	    var_need_refresh = true;
	    small_resource_lock.unlock();
    }

    // switches the comments between "top" and "newest first" (the sort tokens come from the first comments answer)
    static void change_comment_sort(void *) {
	    small_resource_lock.lock();
	    int target = cur_video_info.comment_sort_type == 0 ? 1 : 0;
	    auto new_result = cur_video_info;
	    small_resource_lock.unlock();
	    if (new_result.comment_sort_tokens[target] == "") {
		    return;
	    }
	    add_cpu_limit(ADDITIONAL_CPU_LIMIT);
	    new_result.comments.clear();
	    new_result.comment_continue_token = new_result.comment_sort_tokens[target];
	    new_result.comment_continue_type = 1;
	    new_result.comment_sort_type = target;
	    new_result.load_more_comments();
	    remove_cpu_limit(ADDITIONAL_CPU_LIMIT);

	    std::vector<View *> new_comment_views;
	    for (size_t i = 0; i < new_result.comments.size(); i++) {
		    new_comment_views.push_back(comment_to_view(new_result.comments[i], i));
	    }

	    small_resource_lock.lock();
	    if (!vid_already_init) {
		    small_resource_lock.unlock();
		    return;
	    }
	    cur_video_info = new_result;
	    video_info_cache[cur_video_info.url] = new_result;
	    for (auto view : comment_thumbnail_loaded_list) { // the avatars of the old comments are going away
		    thumbnail_cancel_request(view->author_icon_handle);
		    view->author_icon_handle = -1;
	    }
	    comment_thumbnail_loaded_list.clear();
	    comments_main_view->recursive_delete_subviews();
	    comments_main_view->views = new_comment_views;
	    update_comment_bottom_view();
	    var_need_refresh = true;
	    small_resource_lock.unlock();
    }

    static void load_more_replies(void *arg_) {
	    int comment_index = (int)arg_;
	    PostView *comment_view = dynamic_cast<PostView *>(comments_main_view->views[comment_index]);
	    YouTubeVideoDetail::Comment &comment = cur_video_info.comments[comment_index];

	    add_cpu_limit(ADDITIONAL_CPU_LIMIT);
	    auto new_comment = comment;
	    new_comment.load_more_replies();
	    remove_cpu_limit(ADDITIONAL_CPU_LIMIT);

	    std::vector<PostView *> new_reply_views;
	    // wrap comments
	    logger.info("player/load-r", "truncate start");
	    for (size_t i = comment.replies.size(); i < new_comment.replies.size(); i++) {
		    auto &cur_reply = new_comment.replies[i];
		    auto &cur_content = cur_reply.content;
		    std::vector<std::string> cur_lines;
		    auto itr = cur_content.begin();
		    while (itr != cur_content.end()) {
			    if (cur_lines.size() >= COMMENT_MAX_LINE_NUM) {
				    break;
			    }
			    auto next_itr = std::find(itr, cur_content.end(), '\n');
			    auto tmp = truncate_str(std::string(itr, next_itr), REPLY_MAX_WIDTH,
			                            COMMENT_MAX_LINE_NUM - cur_lines.size(), 0.5, 0.5);
			    cur_lines.insert(cur_lines.end(), tmp.begin(), tmp.end());

			    if (next_itr != cur_content.end()) {
				    itr = std::next(next_itr);
			    } else {
				    break;
			    }
		    }
		    std::string author_id = cur_reply.author.id;
		    new_reply_views.push_back(
		        (new PostView(REPLY_INDENT, 0, 320 - REPLY_INDENT))
		            ->set_author_name(cur_reply.author.name)
		            ->set_author_icon_url(cur_reply.author.icon_url)
		            ->set_time_str(cur_reply.publish_date)
		            ->set_upvote_str(cur_reply.upvotes_str)
		            ->set_content_lines(cur_lines)
		            ->set_has_more_replies([]() { return false; })
		            ->set_on_author_icon_pressed([author_id](const PostView &view) { channel_id_pressed = author_id; })
		            ->set_is_reply(true)
		            ->set_on_timestamp_pressed([](double seconds) {
			            send_seek_request_wo_lock(seconds);
			            var_need_refresh = true;
		            }));
	    }
	    logger.info("player/load-r", "truncate end");

	    small_resource_lock.lock();
	    if (!vid_already_init) { // app shut down while loading
		    small_resource_lock.unlock();
		    return;
	    }
	    comment = new_comment; // do not apply to the cache because it's a mess to also save the folding status
	    comment_view->replies.insert(comment_view->replies.end(), new_reply_views.begin(), new_reply_views.end());
	    comment_view->replies_shown = comment_view->replies.size();
	    comment_view->is_loading_replies = false;
	    small_resource_lock.unlock();
	    var_need_refresh = true;
    }
    static void load_caption(void *arg_) {
	    auto *arg = (std::pair<std::string *, std::string *> *)arg_;
	    auto &base_lang_id = *arg->first;
	    auto &translation_lang_id = *arg->second;

	    add_cpu_limit(ADDITIONAL_CPU_LIMIT);
	    auto new_result = cur_video_info;
	    new_result.load_caption(base_lang_id, translation_lang_id);
	    remove_cpu_limit(ADDITIONAL_CPU_LIMIT);

	    std::vector<View *> caption_main_views;

	    int top_button_height = DEFAULT_FONT_INTERVAL * 1.5;
	    int top_button_width = 140;
	caption_main_views.push_back(
	    (new HorizontalListView(0, 0, top_button_height))
	        ->set_views({(new SelectorView(0, 0, 320 - top_button_width, top_button_height, false))
	                         ->set_texts({(std::function<std::string()>)[]() { return LOCALIZED(OFF); },
	                                      (std::function<std::string()>)[]() {
		return LOCALIZED(ON); }
    },
	                                     1)
	                         ->set_on_change([](const SelectorView &view) {
	caption_overlay_view->set_is_visible(view.selected_button);
	                         }),
	                     (new TextView(0, 0, top_button_width, top_button_height))
	                         ->set_text((std::function<std::string()>)[]() {
	return LOCALIZED(SELECT_LANGUAGE); })
	                         ->set_text_offset(0.0, -2.0)
	                         ->set_x_alignment(TextView::XAlign::CENTER)
	                         ->set_on_view_released(
	                             [](View &view) {
	captions_tab_view->set_subview(caption_language_select_view); })
	                         ->set_background_color(COLOR_GRAY(0x80))
    }));
    if (!new_result.caption_data[{base_lang_id, translation_lang_id}].size()) {
	    caption_main_views.push_back(
	        (new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL))
	            ->set_text((std::function<std::string()>)[]() { return LOCALIZED(NO_CAPTION); })
	            ->set_x_alignment(TextView::XAlign::CENTER));
    }
    for (const auto &caption_piece : new_result.caption_data[{base_lang_id, translation_lang_id}]) {
	    const auto &cur_content = caption_piece.content;

	    if (!cur_content.size() || cur_content == "\n") {
		    continue;
	    }

	    std::vector<std::string> cur_lines;
	    auto itr = cur_content.begin();
	    while (itr != cur_content.end()) {
		    auto next_itr = std::find(itr, cur_content.end(), '\n');
		    auto tmp = truncate_str(std::string(itr, next_itr), CAPTION_CONTENT_MAX_WIDTH, 20, 0.5, 0.5);
		    cur_lines.insert(cur_lines.end(), tmp.begin(), tmp.end());

		    if (next_itr != cur_content.end()) {
			    itr = std::next(next_itr);
		    } else {
			    break;
		    }
	    }

	    float start_time = caption_piece.start_time;
	    float end_time = caption_piece.end_time;
	    caption_main_views.push_back(
	        (new HorizontalListView(0, 0, DEFAULT_FONT_INTERVAL * cur_lines.size() + SMALL_MARGIN))
	            ->set_views({(new TextView(0, 0, CAPTION_TIMESTAMP_WIDTH, DEFAULT_FONT_INTERVAL))
	                             ->set_text(Util_convert_seconds_to_time(start_time))
	                             ->set_text_offset(0.0, -1.0)
	                             ->set_get_text_color([]() { return COLOR_LINK; })
	                             ->set_on_view_released([start_time](View &view) {
		                             if (network_decoder.ready) {
			                             send_seek_request_wo_lock(start_time);
		                             }
	                             }),
	                         (new TextView(0, 0, CAPTION_CONTENT_MAX_WIDTH, DEFAULT_FONT_INTERVAL * cur_lines.size()))
	                             ->set_text_lines(cur_lines)
	                             ->set_text_offset(0.0, -1.0)
	                             ->set_get_background_color([start_time, end_time](const View &) {
		                             return vid_current_pos >= start_time && vid_current_pos < end_time
		                                        ? LIGHT1_BACK_COLOR
		                                        : DEFAULT_BACK_COLOR;
	                             })}));
    }
    caption_main_views.push_back(new EmptyView(0, 0, 320, SMALL_MARGIN));

    // caption overlay
    CaptionOverlayView *new_caption_overlay_view =
        (new CaptionOverlayView(0, 0, 400, 240))
            ->set_caption_data(new_result.caption_data[{base_lang_id, translation_lang_id}]);

    small_resource_lock.lock();
    if (!vid_already_init) { // app shut down while loading
	    small_resource_lock.unlock();
	    return;
    }
    caption_main_view->recursive_delete_subviews();
    caption_main_view->reset();
    caption_main_view->set_views(caption_main_views);
    captions_tab_view->set_subview(caption_main_view);

    delete caption_overlay_view;
    caption_overlay_view = new_caption_overlay_view;
    caption_overlay_view->set_is_visible(true);

    small_resource_lock.unlock();
    }

    /* -------------------------------------------------------------------------------------------------------------- */
    /* -------------------------------------------------------------------------------------------------------------- */
    /* -------------------------------------------------------------------------------------------------------------- */
    // END : functions called from async_task.cpp

    static bool send_load_more_suggestions_request() {
	    if (is_async_task_running(load_video_page) || is_async_task_running(load_more_suggestions)) {
		    return false;
	    }
	    queue_async_task(load_more_suggestions, &cur_video_info);
	    return true;
    }

    // should be called while `small_resource_lock` is locked
    static void send_change_video_request_wo_lock(std::string url, bool update_player, bool update_view,
                                                  bool force_load) {
	    // a page load that is already running would delay the new request until it finishes : tell it to give up
	    if (is_async_task_running(load_video_page) == 2) {
		    youtube_load_cancel = true;
	    }
	    remove_all_async_tasks_with_type(load_video_page);
	    remove_all_async_tasks_with_type(load_more_suggestions);
	    remove_all_async_tasks_with_type(load_more_comments);

	    if (force_load) {
		    video_info_cache.erase(url);
	    }

	    if (update_player) {
		    if (cur_playing_url != url) {
			    vid_play_request = false;
			    cur_playing_url = url;
		    }
	    }
	    if (update_view) {
		    if (cur_displaying_url != url) {
			    cur_displaying_url = url;
			    if (main_view->selected_tab != TAB_PLAYLIST) {
				    main_view->selected_tab = TAB_GENERAL;
			    }
		    }
	    }
	    if (update_player && update_view) {
		    queue_async_task(load_video_page, NULL);
	    } else if (update_view) {
		    queue_async_task(load_video_page, &cur_displaying_url);
	    } else if (update_player) {
		    queue_async_task(load_video_page, &cur_playing_url);
	    }
	    var_need_refresh = true;
    }
    static void send_change_video_request(std::string url, bool update_player, bool update_view, bool force_load) {
	    small_resource_lock.lock();
	    send_change_video_request_wo_lock(url, update_player, update_view, force_load);
	    small_resource_lock.unlock();
    }

    static void send_seek_request_wo_lock(double pos) {
	    vid_seek_pos = pos;
	    vid_current_pos = pos;
	    vid_seek_request = true;
	    if (network_decoder.ready) { // avoid locking while initing
		    network_decoder.interrupt = true;
	    }
    }
    static void send_seek_request(double pos) {
	    small_resource_lock.lock();
	    send_seek_request_wo_lock(pos);
	    small_resource_lock.unlock();
    }

    bool video_is_audio_only_playing() { return audio_only_mode && vid_play_request && !vid_pausing; }
    float lcd_off_timeout_seconds() {
	    float timeout = var_time_to_turn_off_lcd;
	    if (var_audio_screens_off && video_is_audio_only_playing()) {
		    timeout = timeout != 0 ? std::min<float>(timeout, 15.0f) : 15.0f;
	    }
	    return timeout;
    }

// the countdown before an automatic next video (called by the player and, when it is not on screen, by video_is_playing())
static void autoplay_countdown_tick() {
	small_resource_lock.lock();
	if (autoplay_pending_url != "") {
		if (!eof_reached || playing_video_info.id != autoplay_pending_for) { // the user replayed / sought / chose another video
			autoplay_pending_url = "";
		} else if (osGetTime() >= autoplay_due_ms) {
			std::string next = autoplay_pending_url;
			autoplay_pending_url = "";
			send_change_video_request_wo_lock(next, true, false, false);
		} else {
			static int last_shown = -1;
			int left = (int)((autoplay_due_ms - osGetTime()) / 1000) + 1;
			if (left != last_shown) {
				last_shown = left;
				notice_show(tr("Next video in ", "Siguiente vídeo en ") + std::to_string(left) + " s (A: " + tr("cancel", "cancelar") + ")");
			}
			var_need_refresh = true;
		}
	}
	small_resource_lock.unlock();
}

    bool video_is_playing() {
	    autoplay_countdown_tick();
	    if (var_sleep_timer_end && osGetTime() >= var_sleep_timer_end) { // sleep timer : pause the playback
		    var_sleep_timer_end = 0;
		    small_resource_lock.lock();
		    if (vid_play_request && !vid_pausing) {
			    if (!vid_pausing_seek) {
				    Util_speaker_pause(0);
			    }
			    vid_pausing = true;
		    }
		    small_resource_lock.unlock();
		    var_need_refresh = true;
	    }
	    if (!vid_play_request && vid_pausing_seek) {
		    small_resource_lock.lock();
		    vid_pausing_seek = false;
		    if (!vid_pausing) {
			    Util_speaker_resume(0);
		    }
		    small_resource_lock.unlock();
	    }
	    return vid_play_request;
    }
#define SMALL_FONT_SIZE 0.4

    namespace Bar {
    bool bar_grabbed = false;
    double last_grab_timestamp = 0;
    int last_touch_x = -1;
    int last_touch_y = -1;
    float bar_x_l, bar_x_r;
    constexpr int MAXIMIZE_ICON_WIDTH = 28;
    constexpr int TIME_STR_RIGHT_MARGIN = 2;
    constexpr int TIME_STR_LEFT_MARGIN = 3;
    // seek preview : YouTube storyboards are sheets of small frames, one frame every few seconds
    struct StoryboardLevel {
        int w, h, count, cols, rows, interval_ms;
        std::string name, sigh;
    };
    static std::string sb_spec_parsed;
    static std::string sb_base_url;
    static StoryboardLevel sb_level;
    static int sb_level_index = -1;
    static int sb_handle = -1;
    static int sb_sheet = -1;
    static std::string sb_vid_id;
    static bool storyboard_prepare(const std::string &spec) {
	    if (spec != sb_spec_parsed) {
		    sb_spec_parsed = spec;
		    sb_level_index = -1;
		    if (sb_handle != -1) thumbnail_cancel_request(sb_handle);
		    sb_handle = -1;
		    sb_sheet = -1;
		    std::vector<std::string> parts;
		    size_t start = 0;
		    while (true) {
			    size_t bar = spec.find('|', start);
			    parts.push_back(spec.substr(start, bar == std::string::npos ? std::string::npos : bar - start));
			    if (bar == std::string::npos) break;
			    start = bar + 1;
		    }
		    if (parts.size() < 2) return false;
		    sb_base_url = parts[0];
		    int best_diff = 1 << 30;
		    for (size_t i = 1; i < parts.size(); i++) {
			    std::vector<std::string> f;
			    size_t s = 0;
			    while (true) {
				    size_t h = parts[i].find('#', s);
				    f.push_back(parts[i].substr(s, h == std::string::npos ? std::string::npos : h - s));
				    if (h == std::string::npos) break;
				    s = h + 1;
			    }
			    if (f.size() < 8) continue;
			    StoryboardLevel lv = {atoi(f[0].c_str()), atoi(f[1].c_str()), atoi(f[2].c_str()), atoi(f[3].c_str()),
			                          atoi(f[4].c_str()), atoi(f[5].c_str()), f[6], f[7]};
			    if (lv.w <= 0 || lv.h <= 0 || lv.cols <= 0 || lv.rows <= 0 || lv.interval_ms <= 0) continue;
			    int diff = std::abs(lv.w - 160); // about 160 px wide frames are enough for a small preview
			    if (diff < best_diff) {
				    best_diff = diff;
				    sb_level = lv;
				    sb_level_index = (int)i - 1;
			    }
		    }
	    }
	    return sb_level_index >= 0;
    }
    static void draw_seek_preview(double timestamp, float bar_x, float y_top) {
	    if (!storyboard_prepare(playing_video_info.storyboard_spec)) return;
	    int frame = std::min<int>(sb_level.count - 1, std::max<int>(0, (int)(timestamp * 1000 / sb_level.interval_ms)));
	    int per_sheet = sb_level.cols * sb_level.rows;
	    int sheet = frame / per_sheet;
	    if (sheet != sb_sheet || sb_handle == -1) {
		    std::string url = sb_base_url;
		    auto replace_all = [&](const std::string &from, const std::string &to) {
			    size_t pos = 0;
			    while ((pos = url.find(from, pos)) != std::string::npos) {
				    url.replace(pos, from.size(), to);
				    pos += to.size();
			    }
		    };
		    std::string name = sb_level.name;
		    size_t m = name.find("$M");
		    if (m != std::string::npos) name.replace(m, 2, std::to_string(sheet));
		    replace_all("$L", std::to_string(sb_level_index));
		    replace_all("$N", name);
		    url += "&sigh=" + sb_level.sigh;
		    if (sb_handle != -1) thumbnail_cancel_request(sb_handle);
		    sb_handle = thumbnail_request(url, global_current_scene, PRIORITY_FOREGROUND);
		    sb_sheet = sheet;
	    }
	    int within = frame % per_sheet;
	    const float draw_w = 96, draw_h = draw_w * sb_level.h / sb_level.w;
	    float x = std::min(320 - draw_w - 2, std::max(2.0f, bar_x - draw_w / 2));
	    float y = y_top - draw_h - 4;
	    Draw_texture(var_square_image[0], DEF_DRAW_BLACK, x - 2, y - 2, draw_w + 4, draw_h + 4);
	    thumbnail_draw_part(sb_handle, (within % sb_level.cols) * sb_level.w, (within / sb_level.cols) * sb_level.h,
	                        sb_level.w, sb_level.h, x, y, draw_w, draw_h);
    }

    void video_draw_playing_bar() {
	    // draw
	    float y_l = 240 - VIDEO_PLAYING_BAR_HEIGHT;
	    float y_center = 240 - (float)VIDEO_PLAYING_BAR_HEIGHT / 2;

	    Draw_texture(var_square_image[0], DEF_DRAW_DARK_GRAY, 0, y_l, 320, VIDEO_PLAYING_BAR_HEIGHT);

	    if (get_network_waiting_status()) { // loading
		    C2D_DrawCircleSolid(6, y_center, 0, 2, DEF_DRAW_WHITE);
		    C2D_DrawCircleSolid(12, y_center, 0, 2, DEF_DRAW_WHITE);
		    C2D_DrawCircleSolid(18, y_center, 0, 2, DEF_DRAW_WHITE);
	    } else if (vid_pausing || vid_pausing_seek || !vid_play_request) { // pausing
		    C2D_DrawTriangle(8, y_l + 3, DEF_DRAW_WHITE, 8, 240 - 4, DEF_DRAW_WHITE,
		                     8 + (VIDEO_PLAYING_BAR_HEIGHT - 6) * std::sqrt(3) / 2, y_center, DEF_DRAW_WHITE, 0);
	    } else { // playing
		    C2D_DrawRectSolid(8, y_l + 3, 0, 4, VIDEO_PLAYING_BAR_HEIGHT - 6, DEF_DRAW_WHITE);
		    C2D_DrawRectSolid(16, y_l + 3, 0, 4, VIDEO_PLAYING_BAR_HEIGHT - 6, DEF_DRAW_WHITE);
	    }
	    // maximize icon
	    Draw_texture(var_square_image[0], DEF_DRAW_WHITE, 320 - MAXIMIZE_ICON_WIDTH + 4, y_l + 3, 8, 3);
	    Draw_texture(var_square_image[0], DEF_DRAW_WHITE, 320 - MAXIMIZE_ICON_WIDTH + 4, y_l + 3, 3, 5);
	    Draw_texture(var_square_image[0], DEF_DRAW_WHITE, 320 - MAXIMIZE_ICON_WIDTH + 4, 240 - 6, 8, 3);
	    Draw_texture(var_square_image[0], DEF_DRAW_WHITE, 320 - MAXIMIZE_ICON_WIDTH + 4, 240 - 8, 3, 5);
	    Draw_texture(var_square_image[0], DEF_DRAW_WHITE, 320 - 12, y_l + 3, 8, 3);
	    Draw_texture(var_square_image[0], DEF_DRAW_WHITE, 320 - 7, y_l + 3, 3, 5);
	    Draw_texture(var_square_image[0], DEF_DRAW_WHITE, 320 - 12, 240 - 6, 8, 3);
	    Draw_texture(var_square_image[0], DEF_DRAW_WHITE, 320 - 7, 240 - 8, 3, 5);

	    double bar_timestamp = vid_current_pos;
	    if (bar_grabbed) {
		    bar_timestamp = last_grab_timestamp;
	    } else if (vid_seek_request) {
		    bar_timestamp = vid_current_pos;
	    }
	    double vid_progress = network_decoder.get_bar_pos_from_timestamp(bar_timestamp);

	    std::string time_str0 = Util_convert_seconds_to_time(bar_timestamp);
	    std::string time_str1 = "/ " + Util_convert_seconds_to_time(vid_duration);
	    float time_str0_w = Draw_get_width(time_str0, SMALL_FONT_SIZE);
	    float time_str1_w = Draw_get_width(time_str1, SMALL_FONT_SIZE);
	    bar_x_l = 30;
	    bar_x_r = 320 - std::max(time_str0_w, time_str1_w) - TIME_STR_RIGHT_MARGIN - TIME_STR_LEFT_MARGIN -
	              MAXIMIZE_ICON_WIDTH;

	    float time_str_w = std::max(time_str0_w, time_str1_w);
	    Draw(time_str0, bar_x_r + TIME_STR_LEFT_MARGIN + (time_str_w - time_str0_w) / 2, y_l - 1, SMALL_FONT_SIZE,
	         SMALL_FONT_SIZE, DEF_DRAW_WHITE);
	    Draw(time_str1, bar_x_r + TIME_STR_LEFT_MARGIN + (time_str_w - time_str1_w) / 2, y_center - 2, SMALL_FONT_SIZE,
	         SMALL_FONT_SIZE, DEF_DRAW_WHITE);
	    Draw_texture(var_square_image[0], DEF_DRAW_LIGHT_GRAY, bar_x_l, y_center - 2, bar_x_r - bar_x_l, 4);
	    if (bar_grabbed && vid_duration != 0) {
		    draw_seek_preview(bar_timestamp, bar_x_l + (bar_x_r - bar_x_l) * vid_progress, y_l);
	    }
	    if (vid_duration != 0) {
		    Draw_texture(var_square_image[0], 0xFF3333D0, bar_x_l, y_center - 2, (bar_x_r - bar_x_l) * vid_progress, 4);
		    C2D_DrawCircleSolid(bar_x_l + (bar_x_r - bar_x_l) * vid_progress, y_center, 0, bar_grabbed ? 6 : 4,
		                        0xFF3333D0);
	    }
    }
    void video_update_playing_bar(Hid_info key) {
	    float y_l = 240 - VIDEO_PLAYING_BAR_HEIGHT;
	    float y_center = 240 - (float)VIDEO_PLAYING_BAR_HEIGHT / 2;

	    constexpr int GRAB_TOLERANCE = 5;
	    if (!bar_grabbed && network_decoder.ready && key.p_touch && bar_x_l - GRAB_TOLERANCE <= key.touch_x &&
	        key.touch_x <= bar_x_r + GRAB_TOLERANCE && y_center - 2 - GRAB_TOLERANCE <= key.touch_y &&
	        key.touch_y <= y_center + 2 + GRAB_TOLERANCE) {
		    bar_grabbed = true;
		    var_need_refresh = true;
	    }
	    if (bar_grabbed) {
		    last_grab_timestamp = network_decoder.get_timestamp_from_bar_pos(
		        ((key.touch_x != -1 ? key.touch_x : last_touch_x) - bar_x_l) / (bar_x_r - bar_x_l));
	    }
	    if (bar_grabbed && key.touch_x == -1) {
		    if (network_decoder.ready) {
			    send_seek_request(last_grab_timestamp);
		    }
		    bar_grabbed = false;
		    var_need_refresh = true;
	    }
	    if (key.p_touch && key.touch_x >= 320 - MAXIMIZE_ICON_WIDTH && key.touch_y >= 240 - VIDEO_PLAYING_BAR_HEIGHT) {
		    global_intent.next_scene = SceneType::VIDEO_PLAYER;
		    global_intent.arg = cur_playing_url;
	    }
	    small_resource_lock.lock();
	    if (!vid_pausing_seek && bar_grabbed && !vid_pausing) {
		    Util_speaker_pause(0);
	    }
	    if (vid_pausing_seek && !bar_grabbed && !vid_pausing) {
		    Util_speaker_resume(0);
	    }
	    vid_pausing_seek = bar_grabbed;

	    // start/stop
	    if (!get_network_waiting_status() && !vid_pausing_seek && key.p_touch && key.touch_x < 22 &&
	        y_l <= key.touch_y) {
		    if (vid_play_request) {
			    if (vid_pausing) {
				    vid_pausing = false;
				    if (eof_reached) {
					    send_seek_request_wo_lock(0);
				    }
				    Util_speaker_resume(0);
			    } else {
				    vid_pausing = true;
				    Util_speaker_pause(0);
			    }
		    } else if (cur_video_info.is_playable()) {
			    network_waiting_status = NULL;
			    send_change_video_request_wo_lock(cur_displaying_url, true, false, false);
		    }
	    }
	    small_resource_lock.unlock();

	    last_touch_x = key.touch_x;
	    last_touch_y = key.touch_y;
    }
    } // namespace Bar

    // ***** mini player : big controls on the bottom screen to listen with the console in the pocket *****
    static void toggle_pause() {
	    if (!vid_play_request) return;
	    if (vid_pausing) {
		    if (!vid_pausing_seek) Util_speaker_resume(0);
		    if (eof_reached) send_seek_request_wo_lock(0);
		    vid_pausing = false;
	    } else {
		    if (!vid_pausing_seek) Util_speaker_pause(0);
		    vid_pausing = true;
	    }
    }
    static void mini_next() {
	    std::string queued_url;
	    if (play_queue_pop(queued_url)) send_change_video_request_wo_lock(queued_url, true, false, false);
    }
    static void mini_restart() {
	    if (network_decoder.ready) send_seek_request_wo_lock(0);
    }
    // actions that can be bound to X / Y (called outside of the view update, so the locking variants are fine)
    static void mini_set_volume(double v);
    static void run_button_action(int action) {
	    switch (action) {
	    case 1:
		    if (network_decoder.ready) send_seek_request(std::min<double>(vid_duration, vid_current_pos + 30));
		    break;
	    case 2: {
		    std::string queued_url;
		    if (play_queue_pop(queued_url)) send_change_video_request(queued_url, true, false, false);
		    break;
	    }
	    case 3:
		    if (network_decoder.ready) send_seek_request(0);
		    break;
	    case 4:
		    mini_visible = true;
		    break;
	    case 5:
		    if (network_decoder.ready) bookmark_add_request = true;
		    break;
	    case 6:
		    stop_after_video = !stop_after_video;
		    break;
	    case 7:
	    case 8: { // volume (the same preamp as the mini player, remembered for the channel)
		    mini_set_volume(mini_volume + (action == 7 ? 0.15 : -0.15));
		    notice_show(tr("Volume ", "Volumen ") + std::to_string((int)(mini_volume * 100 + 0.5)) + "%");
		    break;
	    }
	    case 9:
	    case 10: { // brightness of the screens
		    var_lcd_brightness = std::max(15, std::min(163, var_lcd_brightness + (action == 9 ? 14 : -14)));
		    misc_tasks_request(TASK_CHANGE_BRIGHTNESS);
		    misc_tasks_request(TASK_SAVE_SETTINGS);
		    notice_show(tr("Brightness ", "Brillo ") + std::to_string((var_lcd_brightness - 15) * 100 / 148) + "%");
		    break;
	    }
	    }
	    var_need_refresh = true;
    }
    static void mini_set_volume(double v) {
	    mini_volume = std::min(3.0, std::max(0.1, v));
	    network_decoder.set_preamp(mini_volume);
	    channel_volume_set(playing_video_info.author.id, mini_volume); // remembered for this channel
	    var_need_refresh = true;
    }
    static void mini_player_draw() {
	    if (!mini_visible) return;
	    Draw_texture(var_square_image[0], DEFAULT_BACK_COLOR, 0, 0, 320, 240);
	    auto lines = truncate_str(playing_video_info.title, 290, 2, 0.55, 0.55);
	    float y = 8;
	    for (auto &line : lines) {
		    Draw_xy_centered(line, 0, 320, y, y + 20, 0.55, 0.55, DEFAULT_TEXT_COLOR);
		    y += 20;
	    }
	    Draw_xy_centered(playing_video_info.author.name, 0, 320, y, y + 18, 0.45, 0.45, LIGHT1_TEXT_COLOR);
	    // progress
	    double dur = vid_duration > 0 ? vid_duration : 1;
	    double pos = std::min(dur, std::max(0.0, vid_current_pos));
	    Draw_texture(var_square_image[0], LIGHT1_BACK_COLOR, 20, 80, 280, 6);
	    Draw_texture(var_square_image[0], COLOR_ACCENT, 20, 80, 280 * pos / dur, 6);
	    Draw_xy_centered(Util_convert_seconds_to_time(pos) + " / " + Util_convert_seconds_to_time(vid_duration), 0, 320,
	                     88, 106, 0.45, 0.45, LIGHT0_TEXT_COLOR);
	    // buttons : previous (restart) / play-pause / next in queue
	    const u32 btn = LIGHT1_BACK_COLOR;
	    Draw_texture(var_square_image[0], btn, 20, 115, 80, 60);
	    Draw_texture(var_square_image[0], COLOR_ACCENT, 120, 115, 80, 60);
	    Draw_texture(var_square_image[0], btn, 220, 115, 80, 60);
	    u32 ic = DEF_DRAW_WHITE;
	    // icons : "previous" |< and "next" >| ; the left-handed mode swaps their sides
	    auto draw_prev = [&](float cx) {
		    C2D_DrawTriangle(cx + 12, 130, ic, cx + 12, 160, ic, cx - 10, 145, ic, 0);
		    C2D_DrawRectSolid(cx - 16, 130, 0, 4, 30, ic);
	    };
	    auto draw_next = [&](float cx) {
		    C2D_DrawTriangle(cx - 12, 130, ic, cx - 12, 160, ic, cx + 10, 145, ic, 0);
		    C2D_DrawRectSolid(cx + 12, 130, 0, 4, 30, ic);
	    };
	    if (var_left_handed) {
		    draw_next(60), draw_prev(260);
	    } else {
		    draw_prev(60), draw_next(260);
	    }
	    if (vid_pausing || !vid_play_request) {
		    C2D_DrawTriangle(150, 128, ic, 150, 162, ic, 176, 145, ic, 0);
	    } else {
		    C2D_DrawRectSolid(148, 128, 0, 7, 34, ic);
		    C2D_DrawRectSolid(165, 128, 0, 7, 34, ic);
	    }
	    // volume
	    Draw_texture(var_square_image[0], btn, 20, 190, 50, 36);
	    Draw_texture(var_square_image[0], btn, 250, 190, 50, 36);
	    Draw_xy_centered(var_left_handed ? "+" : "-", 20, 70, 190, 226, 0.9, 0.9, DEFAULT_TEXT_COLOR);
	    Draw_xy_centered(var_left_handed ? "-" : "+", 250, 300, 190, 226, 0.9, 0.9, DEFAULT_TEXT_COLOR);
	    Draw_xy_centered(tr("Volume ", "Volumen ") + std::to_string((int)std::round(mini_volume * 100)) + "%", 70, 250, 190,
	                     226, 0.5, 0.5, DEFAULT_TEXT_COLOR);
	    Draw_xy_centered("X", 290, 320, 0, 24, 0.6, 0.6, LIGHT1_TEXT_COLOR);
	    if (!vid_pausing && vid_play_request) var_need_refresh = true; // keep the time moving
    }
    static bool mini_player_update(Hid_info key) {
	    if (!mini_visible) return false;
	    static int start_x = -1, start_y = -1, last_x = -1, last_y = -1;
	    if (key.p_b) {
		    mini_visible = false;
		    var_need_refresh = true;
		    return true;
	    }
	    if (key.p_a) toggle_pause();
	    if (key.p_r) mini_next();
	    if (key.p_l) mini_restart();
	    if (key.p_touch) {
		    start_x = last_x = key.touch_x;
		    start_y = last_y = key.touch_y;
	    } else if (key.touch_x != -1) {
		    last_x = key.touch_x;
		    last_y = key.touch_y;
	    } else if (start_x != -1) { // released
		    int dx = last_x - start_x, dy = last_y - start_y;
		    if (std::abs(dx) > 70 && std::abs(dy) < 40) { // swipe : left = next in the queue, right = restart
			    dx < 0 ? mini_next() : mini_restart();
		    } else if (std::abs(dx) < 12 && std::abs(dy) < 12) {
			    int x = start_x, y = start_y;
			    if (x >= 290 && y < 30) {
				    mini_visible = false;
			    } else if (y >= 115 && y < 175) {
				    bool left_is_prev = !var_left_handed;
				    if (x >= 20 && x < 100) left_is_prev ? mini_restart() : mini_next();
				    else if (x >= 120 && x < 200) toggle_pause();
				    else if (x >= 220 && x < 300) left_is_prev ? mini_next() : mini_restart();
			    } else if (y >= 190 && y < 226) {
				    bool left_is_down = !var_left_handed;
				    if (x >= 20 && x < 70) mini_set_volume(left_is_down ? mini_volume / 1.25 : mini_volume * 1.25);
				    else if (x >= 250 && x < 300) mini_set_volume(left_is_down ? mini_volume * 1.25 : mini_volume / 1.25);
			    } else if (y >= 70 && y < 100 && x >= 20 && x < 300 && network_decoder.ready) {
				    send_seek_request_wo_lock(vid_duration * (x - 20) / 280.0);
			    }
		    }
		    var_need_refresh = true;
		    start_x = -1;
	    }
	    return true;
    }
    void video_update_playing_bar(Hid_info key) { Bar::video_update_playing_bar(key); }
    void video_draw_playing_bar() { Bar::video_draw_playing_bar(); }

    void video_set_linear_filter_enabled(bool enabled) {
	    if (vid_already_init) {
		    for (int i = 0; i < 2; i++) {
			    Draw_c2d_image_set_filter(&vid_image[i], enabled);
		    }
	    }
    }
    void video_set_skip_drawing(bool skip) { video_skip_drawing = skip; }

    bool video_should_draw_top_bar() { return !var_full_screen_mode || !network_decoder.ready; }
    u32 video_get_top_screen_background_color() {
	    return vid_play_request && network_decoder.ready && !audio_only_mode ? DEF_DRAW_BLACK : DEFAULT_BACK_COLOR;
    }
    // screensaver : after a minute of pause the logo bounces on a black screen (saves the screen from burn-in)
    static bool screensaver_active() {
	    static u64 paused_since = 0;
	    if (vid_play_request && vid_pausing && !get_network_waiting_status()) {
		    if (!paused_since) paused_since = osGetTime();
	    } else {
		    paused_since = 0;
	    }
	    return paused_since && osGetTime() - paused_since > 60000;
    }
    void video_draw_video_frame() {
	    if (screensaver_active()) {
		    Draw_texture(var_square_image[0], DEF_DRAW_BLACK, 0, 0, 400, 240);
		    auto wave = [](double t, double period, double lo, double hi) {
			    double ph = fmod(t, period) / period; // 0..1
			    double tri = ph < 0.5 ? ph * 2 : 2 - ph * 2;
			    return lo + (hi - lo) * tri;
		    };
		    double t = osGetTime() / 1000.0;
		    logo_draw(wave(t, 23, 90, 310), wave(t, 17, 80, 170));
		    var_need_refresh = true;
		    return;
	    }
	    int texture_index = !texture_index_head;
	    if (vid_play_request && network_decoder.ready && !audio_only_mode) {
		    Draw_texture(vid_image[texture_index].c2d, vid_x, vid_y, vid_tex_width[texture_index] * vid_zoom,
		                 vid_tex_height[texture_index] * vid_zoom);
		    small_resource_lock.lock(); // the overlay is replaced by the loader threads
		    if (caption_overlay_view) {
			    caption_overlay_view->cur_timestamp = vid_current_pos;
			    caption_overlay_view->draw();
		    }
		    small_resource_lock.unlock();
	    } else if (vid_play_request && network_decoder.ready && audio_only_mode) {
		    // minimalist audio-only screen : title, author, time and a progress bar
		    small_resource_lock.lock(); // playing_video_info is replaced when the video changes
		    std::string audio_title = playing_video_info.title, audio_author = playing_video_info.author.name;
		    small_resource_lock.unlock();
		    auto title_lines = truncate_str(audio_title, 360, 3, 0.65, 0.65);
		    float y = 60;
		    for (auto &line : title_lines) {
			    Draw_xy_centered(line, 0, 400, y, y + 22, 0.65, 0.65, DEFAULT_TEXT_COLOR);
			    y += 22;
		    }
		    Draw_xy_centered(audio_author, 0, 400, y + 2, y + 20, 0.5, 0.5, LIGHT1_TEXT_COLOR);
		    double dur = vid_duration > 0 ? vid_duration : 1;
		    double pos = std::min(dur, std::max(0.0, vid_current_pos));
		    Draw_xy_centered(Util_convert_seconds_to_time(pos) + " / " + Util_convert_seconds_to_time(vid_duration), 0,
		                     400, 160, 180, 0.55, 0.55, DEFAULT_TEXT_COLOR);
		    Draw_texture(var_square_image[0], LIGHT1_BACK_COLOR, 40, 188, 320, 6);
		    Draw_texture(var_square_image[0], COLOR_ACCENT, 40, 188, 320 * pos / dur, 6);
	    } else {
		    logo_draw(200, 120);
	    }
    }
    void video_draw_top_screen() {
	    // fit to screen size
	    vid_zoom = std::min(401.0 / vid_width_org, (var_full_screen_mode ? 241.0 : 226.0) / vid_height_org);
	    vid_zoom = std::min(10.0, std::max(0.05, vid_zoom));
	    vid_x = (400 - (vid_width_org * vid_zoom)) / 2;
	    vid_y = ((var_full_screen_mode ? 240 : 225) - (vid_height_org * vid_zoom)) / 2;
	    if (!var_full_screen_mode) {
		    vid_y += 15;
	    }

		Draw_screen_ready(0, video_get_top_screen_background_color());
	    video_draw_video_frame();
		if (var_3d_mode) {
			vid_x = (800 - (vid_width_org * vid_zoom)) / 2;
			Draw_screen_ready(1, video_get_top_screen_background_color());
	    	video_draw_video_frame();
		}
	    logger.draw();
	    if (video_should_draw_top_bar()) {
		    Draw_top_ui();
	    }
	    if (var_debug_mode) {
		    Draw_debug_info();
	    }
    }

    static void decode_thread(void *arg) {
	    logger.info(DEF_SAPP0_DECODE_THREAD_STR, "Thread started.");

	    Result_with_string result;
	    int ch = 0;
	    int audio_size = 0;
	    int w = 0;
	    int h = 0;
	    bool key = false;
	    std::string format = "";
	    std::string type = (char *)arg;
	    TickCounter counter0, counter1;
	    osTickCounterStart(&counter0);
	    osTickCounterStart(&counter1);

	    while (vid_thread_run) {
		    if (vid_play_request || vid_change_video_request) {
			    vid_x = 0;
			    vid_y = 15;
			    vid_frametime = 0;
			    vid_framerate = 0;
			    vid_current_pos = 0;
			    vid_duration = 0;
			    vid_zoom = 1;
			    vid_width = 0;
			    vid_height = 0;
			    texture_index_head = 0;
			    vid_video_format = "n/a";
			    vid_audio_format = "n/a";
			    vid_change_video_request = false;
			    vid_seek_request = false;
			    eof_reached = false;
			    vid_play_request = true;
			    vid_total_time = 0;
			    vid_total_frames = 0;
			    vid_min_time = 99999999;
			    vid_max_time = 0;
			    vid_recent_total_time = 0;
			    for (int i = 0; i < 90; i++) {
				    vid_recent_time[i] = 0;
			    }

			    for (int i = 0; i < 2; i++) {
				    vid_tex_width[i] = 0;
				    vid_tex_height[i] = 0;
			    }

			    for (int i = 0; i < 320; i++) {
				    vid_time[0][i] = 0;
				    vid_time[1][i] = 0;
			    }

			    vid_audio_time = 0;
			    vid_video_time = 0;
			    vid_copy_time[0] = 0;
			    vid_copy_time[1] = 0;
			    vid_convert_time = 0;

			    // video page parsing sometimes randomly fails, so try several times
			    network_waiting_status = "Reading Stream";
			    // snapshot of the urls : the async thread may replace playing_video_info while the decoder starts
			    std::string snap_video_url, snap_audio_url, snap_both_url, snap_id;
			    int snap_fragment_len = -1;
			    bool snap_ts_adjust = false;
			    {
				    small_resource_lock.lock();
				    auto url_it = playing_video_info.video_stream_urls.find((int)video_p_value);
				    if (url_it != playing_video_info.video_stream_urls.end()) snap_video_url = url_it->second;
				    snap_audio_url = playing_video_info.audio_stream_url;
				    snap_both_url = playing_video_info.both_stream_url;
				    snap_id = playing_video_info.id;
				    snap_fragment_len = playing_video_info.is_livestream ? playing_video_info.stream_fragment_len : -1;
				    snap_ts_adjust = playing_video_info.needs_timestamp_adjusting();
				    small_resource_lock.unlock();
			    }
			    if (audio_only_mode) {
				    result = network_decoder.init(
				        snap_audio_url != "" ? snap_audio_url : snap_both_url, stream_downloader,
				        snap_fragment_len,
				        snap_ts_adjust, var_is_new3ds);
			    } else if (snap_video_url != "" &&
			               snap_audio_url != "") {
				    result = network_decoder.init(
				        snap_video_url, snap_audio_url,
				        stream_downloader,
				        snap_fragment_len,
				        snap_ts_adjust,
				        var_is_new3ds && (video_p_value == 360 || video_p_value == 480));
			    } else if ((video_p_value == 360 || video_p_value == 480) && snap_both_url != "") {
				    // itag 18 (both_stream) of a long video takes too much time and sometimes leads to a crash
				    result = network_decoder.init(
				        snap_both_url, stream_downloader,
				        snap_fragment_len,
				        snap_ts_adjust, var_is_new3ds);
			    } else {
				    result.code = -1;
				    result.string = "YouTube parser error";
				    result.error_description = "No valid stream url extracted";
				    { // details for the log and for the error message : what was available when this happened
					    std::string details = "quality=" + std::to_string((int)video_p_value) + " audio_only=" + std::to_string(audio_only_mode) +
					                          " video_url=" + std::to_string(snap_video_url != "") + " audio_url=" + std::to_string(snap_audio_url != "") +
					                          " both_url=" + std::to_string(snap_both_url != "");
					    small_resource_lock.lock();
					    details += " status=" + playing_video_info.playability_status + " streams=" + std::to_string(playing_video_info.video_stream_urls.size());
					    small_resource_lock.unlock();
					    logger.warning("player", "no valid stream : " + details);
					    result.error_description += "\n" + details;
				    }
			    }

			    logger.info(DEF_SAPP0_DECODE_THREAD_STR,
			                "network_decoder.init()..." + result.string + result.error_description, result.code);
			    if (result.code != 0) {
				    if (video_retry_left > 0) {
					    video_retry_left--;
					    logger.caution("dec", "failed, retrying. retry cnt left:" + std::to_string(video_retry_left));
					    send_change_video_request(cur_playing_url, true, false, true);
				    } else {
					    Util_err_set_error_message(result.string, result.error_description, DEF_SAPP0_DECODE_THREAD_STR,
					                               result.code);
					    Util_err_set_error_show_flag(true);
					    var_need_refresh = true;
				    }
				    vid_play_request = false;
			    }
			    network_waiting_status = NULL;

			    if (vid_play_request) {
				    {
					    auto tmp = network_decoder.get_audio_info();
					    // bitrate = tmp.bitrate;
					    vid_sample_rate = tmp.sample_rate;
					    ch = tmp.ch;
					    vid_audio_format = tmp.format_name;
					    vid_duration = tmp.duration;
				    }
				    Util_speaker_init(0, ch, vid_sample_rate);
				    {
					    auto tmp = network_decoder.get_video_info();
					    vid_width = vid_width_org = tmp.width;
					    vid_height = vid_height_org = tmp.height;
					    vid_framerate = tmp.framerate;
					    vid_video_format = tmp.format_name;
					    vid_duration = tmp.duration;
					    vid_frametime = 1000.0 / vid_framerate;

					    if (vid_width % 16 != 0) {
						    vid_width += 16 - vid_width % 16;
					    }
					    if (vid_height % 16 != 0) {
						    vid_height += 16 - vid_height % 16;
					    }
				    }
			    }

			    if (vid_play_request) { // from now on the position is remembered for this video
				    strncpy(vid_resume_id, snap_id.c_str(), sizeof(vid_resume_id) - 1);
				    vid_resume_id[sizeof(vid_resume_id) - 1] = 0;
			    }
			    if (vid_play_request && seek_at_init_request < 0 && !playing_video_info.is_livestream && var_resume_videos) {
				    // continue where the video was left
				    double resume_pos = resume_get(snap_id);
				    if (resume_pos > 0 && vid_duration - resume_pos > 15) {
					    seek_at_init_request = resume_pos;
				    }
			    }
			    if (seek_at_init_request >= 0) {
				    vid_seek_request = true;
				    vid_seek_pos = seek_at_init_request;
				    seek_at_init_request = -1;
			    }

			    osTickCounterUpdate(&counter1);
			    while (vid_play_request && !should_suspend_decoding) {
				    if (vid_seek_request && !vid_change_video_request) {
					    network_waiting_status = "Seeking";
					    Util_speaker_clear_buffer(0);
					    network_decoder_critical_lock.lock(); // the converter thread is now suspended
					    vid_current_pos = vid_seek_pos;
					    while (vid_seek_request && !vid_change_video_request && vid_play_request) {
						    small_resource_lock.lock();
						    double seek_pos_bak = vid_seek_pos;
						    vid_seek_request = false;
						    network_decoder.interrupt = false;
						    small_resource_lock.unlock();

						    result = network_decoder.seek(seek_pos_bak * 1000 * 1000); // nano seconds
						    // if seek failed because of the lock, it's probably another seek request or other requests
						    // (change video etc...), so we can ignore it
						    if (network_decoder.interrupt) {
							    logger.info(DEF_SAPP0_DECODE_THREAD_STR, "seek interrupted");
							    continue;
						    }
						    logger.info(DEF_SAPP0_DECODE_THREAD_STR,
						                "network_decoder.seek()..." + result.string + result.error_description,
						                result.code);
						    if (result.code != 0) {
							    vid_play_request = false;
						    }
						    break;
					    }
					    network_decoder_critical_lock.unlock();
					    if (eof_reached) {
						    vid_pausing = false; // if it was stuck at the end of the video, resume playing after seek
					    }
					    network_waiting_status = NULL;
					    var_need_refresh = true;
				    }
				    if (vid_change_video_request || !vid_play_request) {
					    break;
				    }
				    vid_duration = network_decoder.get_duration();

				    auto type = network_decoder.next_decode_type();

				    if (type == NetworkMultipleDecoder::PacketType::EoF) {
					    // wait for the audio buffer to be empty
					    if (audio_only_mode) {
						    while (!Util_speaker_is_buffer_empty(0) && vid_play_request && !vid_seek_request &&
						           !vid_change_video_request) {
							    usleep(100000);
						    }
					    }
					    vid_pausing = true;
					    bool break_flag = false;
					    if (!eof_reached) { // the first time it reaches EOF
						    small_resource_lock.lock();
						    const bool stop_here = stop_after_video; // "stop after this video" was requested
						    stop_after_video = false;
						    resume_set(playing_video_info.id, 0, 0); // watched until the end : next time from the start
						    resume_save();
						    if (var_offline_autodelete && var_loop_mode == false && playing_video_info.url.compare(0, 8, "offline:") == 0) {
							    pending_offline_delete = playing_video_info.id;
						    }
						    std::string queued_url;
						    if (var_loop_mode == false && !stop_here && play_queue_pop(queued_url)) {
							    // something was queued by the user : play it before any autoplay suggestion
							    send_change_video_request_wo_lock(queued_url, true, false, false);
							    break_flag = true;
						    } else if (var_loop_mode == false && !stop_here) {
							    if ((var_autoplay_level == 2 && playing_video_info.has_next_video()) ||
							        (var_autoplay_level == 1 && playing_video_info.has_next_video_in_playlist())) {
								    if (var_autoplay_delay > 0) { // wait : the countdown is handled by the UI thread
									    static const int delays[4] = {0, 3, 5, 10};
									    autoplay_pending_url = playing_video_info.get_next_video().url;
									    autoplay_pending_for = playing_video_info.id;
									    autoplay_due_ms = osGetTime() + delays[var_autoplay_delay] * 1000;
								    } else {
									    send_change_video_request_wo_lock(playing_video_info.get_next_video().url, true,
									                                      false, false);
									    break_flag = true;
								    }
							    }
						    } else if (var_loop_mode == true) {
							    vid_seek_request = true;
							    vid_seek_pos = 0;
						    }
						    small_resource_lock.unlock();
					    }
					    eof_reached = true;
					    if (break_flag) {
						    break;
					    }
					    usleep(10000);
					    continue;
				    } else {
					    eof_reached = false;
				    }

				    if (type == NetworkMultipleDecoder::PacketType::AUDIO) {
					    double pos;
					    u8 *audio = NULL;
					    osTickCounterUpdate(&counter0);
					    result = network_decoder.decode_audio(&audio_size, &audio, &pos);
					    osTickCounterUpdate(&counter0);
					    vid_audio_time = osTickCounterRead(&counter0);

					    if (result.code == 0) {
						    while (true) {
							    result = Util_speaker_add_buffer(0, ch, audio, audio_size, pos);
							    if (result.code == 0 || !vid_play_request || vid_seek_request ||
							        vid_change_video_request) {
								    break;
							    }
							    // Util_log_save(DEF_SAPP0_DECODE_THREAD_STR, "audio queue full");

							    usleep(10000);
						    }
						    free(audio);
						    audio = NULL;
					    } else if (result.code != DEF_ERR_NEED_MORE_INPUT) { // ignore NEED_MORE_INPUT error
						    logger.error(DEF_SAPP0_DECODE_THREAD_STR,
						                 "Util_audio_decoder_decode()..." + result.string + result.error_description,
						                 result.code);
					    }
				    } else if (type == NetworkMultipleDecoder::PacketType::VIDEO) {
					    osTickCounterUpdate(&counter0);
					    result = network_decoder.decode_video(&w, &h, &key);
					    osTickCounterUpdate(&counter0);

					    while (result.code == DEF_ERR_NEED_MORE_OUTPUT && vid_play_request && !vid_seek_request &&
					           !vid_change_video_request) {
						    usleep(10000);
						    osTickCounterUpdate(&counter0);
						    result = network_decoder.decode_video(&w, &h, &key);
						    osTickCounterUpdate(&counter0);
					    }
					    vid_video_time = osTickCounterRead(&counter0);

					    // get the elapsed time from the previous frame
					    osTickCounterUpdate(&counter1);
					    double cur_frame_internval = osTickCounterRead(&counter1);

					    vid_min_time = std::min(vid_min_time, cur_frame_internval);
					    vid_max_time = std::max(vid_max_time, cur_frame_internval);
					    vid_total_time += cur_frame_internval;
					    vid_total_frames++;
					    vid_recent_time[89] = cur_frame_internval;
					    vid_recent_total_time = std::accumulate(vid_recent_time, vid_recent_time + 90, 0.0);
					    for (int i = 1; i < 90; i++) {
						    vid_recent_time[i - 1] = vid_recent_time[i];
					    }

					    vid_time[0][319] = cur_frame_internval;
					    for (int i = 1; i < 320; i++) {
						    vid_time[0][i - 1] = vid_time[0][i];
					    }

					    if (vid_play_request && !vid_seek_request && !vid_change_video_request) {
						    if (result.code != 0 && result.code != DEF_ERR_NEED_MORE_INPUT) {
							    logger.error(DEF_SAPP0_DECODE_THREAD_STR,
							                 "Util_video_decoder_decode()..." + result.string +
							                     result.error_description,
							                 result.code);
						    }
					    }
				    } else if (type == NetworkMultipleDecoder::PacketType::INTERRUPTED) {
					    continue;
				    } else {
					    logger.error(DEF_SAPP0_DECODE_THREAD_STR, "unknown type of packet");
				    }
			    }

			    network_waiting_status = NULL;

			    while (Util_speaker_is_playing(0) && vid_play_request) {
				    usleep(10000);
			    }
			    Util_speaker_exit(0);

			    if (!vid_change_video_request) {
				    vid_play_request = false;
			    }

			    // make sure the convert thread stops before closing network_decoder
			    network_decoder_critical_lock.lock(); // the converter thread is now suspended
			    network_decoder.deinit();
			    network_decoder_critical_lock.unlock();

			    var_need_refresh = true;
			    vid_pausing = false;
			    vid_seek_request = false;
			    logger.info(DEF_SAPP0_DECODE_THREAD_STR, "deinit complete");
		    } else {
			    usleep(DEF_ACTIVE_THREAD_SLEEP_TIME);
		    }

		    while (vid_thread_run && (should_suspend_decoding ||
		                              (vid_thread_suspend && !vid_play_request && !vid_change_video_request))) {
			    usleep(DEF_INACTIVE_THREAD_SLEEP_TIME);
		    }
	    }

	    logger.info(DEF_SAPP0_DECODE_THREAD_STR, "Thread exit.");
	    threadExit(0);
    }

    static void convert_thread(void *arg) {
	    logger.info(DEF_SAPP0_CONVERT_THREAD_STR, "Thread started.");
	    u8 *yuv_video = NULL;
	    u8 *video = NULL;
	    TickCounter counter0, counter1;
	    Result_with_string result;

	    osTickCounterStart(&counter0);

	    y2rInit();

	    while (vid_thread_run) {
		    if (vid_play_request && !vid_seek_request && !vid_change_video_request) {
			    network_decoder_critical_lock.lock();
			    while (vid_play_request && !vid_seek_request && !vid_change_video_request) {
				    double pts;
				    do {
					    osTickCounterUpdate(&counter1);
					    osTickCounterUpdate(&counter0);
					    result = network_decoder.get_decoded_video_frame(
					        vid_width, vid_height, network_decoder.hw_decoder_enabled ? &video : &yuv_video, &pts);
					    osTickCounterUpdate(&counter0);
					    if (result.code != DEF_ERR_NEED_MORE_INPUT) {
						    break;
					    }
					    if (vid_pausing || vid_pausing_seek) {
						    usleep(10000);
					    } else {
						    usleep(3000);
					    }
				    } while (vid_play_request && !vid_seek_request && !vid_change_video_request && !audio_only_mode);

				    if (audio_only_mode) {
					    while (audio_only_mode && vid_play_request && !vid_seek_request && !vid_change_video_request) {
						    double tmp = Util_speaker_get_current_timestamp(0, vid_sample_rate);
						    if (tmp != -1) {
							    vid_current_pos = tmp;
						    }
						    usleep(50000);
					    }
					    break;
				    }
				    if (!vid_play_request || vid_seek_request || vid_change_video_request) {
					    break;
				    }
				    if (result.code != 0) { // this is an unexpected error
					    logger.error(DEF_SAPP0_CONVERT_THREAD_STR,
					                 "failure getting decoded result" + result.string + result.error_description,
					                 result.code);
					    vid_play_request = false;
					    break;
				    }

				    bool video_need_free = false;
				    vid_copy_time[0] = osTickCounterRead(&counter0);

				    osTickCounterUpdate(&counter0);
				    if (!network_decoder.hw_decoder_enabled) {
					    result = Util_converter_y2r_yuv420p_to_bgr565(yuv_video, &video, vid_width, vid_height, false);
					    video_need_free = true;
				    }
				    osTickCounterUpdate(&counter0);
				    vid_convert_time = osTickCounterRead(&counter0);

				    double cur_convert_time = 0;

				    if (result.code == 0) {
					    // we don't want to include the sleep time in the performance profiling
					    osTickCounterUpdate(&counter0);
					    vid_copy_time[1] = osTickCounterRead(&counter0);
					    osTickCounterUpdate(&counter1);
					    cur_convert_time = osTickCounterRead(&counter1);

					    // sync with sound
					    double cur_sound_pos = Util_speaker_get_current_timestamp(0, vid_sample_rate);
					    const double av_lead = var_av_sync_ms / 1000.0; // user adjustable A/V offset (Settings)
					    if (cur_sound_pos >= 0) { // if sound is not playing, probably because the video is lagging
						                          // behind, draw immediately
						    while (pts - cur_sound_pos - av_lead > 0.003 && vid_play_request && !vid_seek_request &&
						           !vid_change_video_request) {
							    double sleep_microseconds =
							        std::min(0.1, (pts - cur_sound_pos - av_lead - 0.0015) / network_decoder.get_tempo()) *
							        1000000;
							    usleep(sleep_microseconds);
							    cur_sound_pos = Util_speaker_get_current_timestamp(0, vid_sample_rate);
							    if (cur_sound_pos < 0) {
								    break;
							    }
						    }
					    }
					    vid_current_pos = pts;

					    {   // remember the position every 10 s so the video can be continued later
						    static u64 last_resume_save = 0;
						    u64 now = osGetTime();
						    if (now - last_resume_save > 10000 && vid_duration > 60 && vid_resume_id[0] && !vid_change_video_request) {
							    last_resume_save = now;
							    resume_set(std::string(vid_resume_id), pts, vid_duration);
							    resume_save();
						    }
					    }

					    osTickCounterUpdate(&counter0);
					    osTickCounterUpdate(&counter1);

					    if (!video_skip_drawing) {
						    vid_tex_width[texture_index_head] = vid_width_org;
						    vid_tex_height[texture_index_head] = vid_height_org;
						    result = Draw_set_texture_data(&vid_image[texture_index_head], video, vid_width,
						                                   vid_height_org, 1024, 1024, GPU_RGB565);
						    if (result.code != 0) {
							    logger.error(DEF_SAPP0_CONVERT_THREAD_STR,
							                 "Draw_set_texture_data()..." + result.string + result.error_description,
							                 result.code);
						    }
						    texture_index_head = !texture_index_head;
					    }

					    osTickCounterUpdate(&counter0);
					    vid_copy_time[1] += osTickCounterRead(&counter0);

					    var_need_refresh = true;
				    } else {
					    logger.error(DEF_SAPP0_CONVERT_THREAD_STR,
					                 "Util_converter_yuv420p_to_bgr565()..." + result.string + result.error_description,
					                 result.code);
				    }

				    if (video_need_free) {
					    free(video);
				    }
				    video = NULL;
				    yuv_video = NULL; // this is the result of network_decoder.get_decoded_video_frame(), so it should
				                      // not be freed

				    osTickCounterUpdate(&counter1);
				    cur_convert_time += osTickCounterRead(&counter1);
				    vid_time[1][319] = cur_convert_time;
				    for (int i = 1; i < 320; i++) {
					    vid_time[1][i - 1] = vid_time[1][i];
				    }
			    }
			    network_decoder_critical_lock.unlock();
		    } else {
			    usleep(DEF_ACTIVE_THREAD_SLEEP_TIME);
		    }

		    while (vid_thread_run && (should_suspend_decoding ||
		                              (vid_thread_suspend && !vid_play_request && !vid_change_video_request))) {
			    usleep(DEF_INACTIVE_THREAD_SLEEP_TIME);
		    }
	    }

	    y2rExit();

	    logger.info(DEF_SAPP0_CONVERT_THREAD_STR, "Thread exit.");
	    threadExit(0);
    }

    void video_set_show_debug_info(bool show) {
	    if (vid_already_init) {
		    small_resource_lock.lock();
		    if (show && playback_tab_view->views.back() != debug_info_view) {
			    playback_tab_view->views.push_back(debug_info_view);
		    }
		    if (!show && playback_tab_view->views.back() == debug_info_view) {
			    playback_tab_view->views.pop_back();
		    }
		    small_resource_lock.unlock();
	    }
    }

    void VideoPlayer_draw(void) {
	    Hid_info key;
	    Util_hid_query_key_state(&key);

	    thumbnail_set_active_scene(SceneType::VIDEO_PLAYER);

	    bool video_playing_bar_show = video_is_playing();

	    if (var_need_refresh || !var_eco_mode) {
		    var_need_refresh = false;
		    Draw_frame_ready();
		    video_draw_top_screen();

		    Draw_screen_ready(2, DEFAULT_BACK_COLOR);

		    small_resource_lock.lock();
		    main_view->draw();
		    video_draw_playing_bar();
		    draw_overlay_menu(240 - VIDEO_PLAYING_BAR_HEIGHT - TAB_SELECTOR_HEIGHT - OVERLAY_MENU_ICON_SIZE);
		    equalizer_popup_view->draw();
		    if (text_popup_view->is_visible) text_popup_view->draw();
		    if (account_pl_popup_view->is_visible) account_pl_popup_view->draw();
		    if (comment_dialog->is_visible) comment_dialog->draw();
		    notice_draw(240 - VIDEO_PLAYING_BAR_HEIGHT);
		    mini_player_draw();
		    qr_popup_draw();
		    small_resource_lock.unlock();

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
		    net_waiting_snapshot = get_network_waiting_status() != NULL;
		    /* ****************************** LOCK START ******************************  */
		    small_resource_lock.lock();

		    // thumbnail request update (this should be done while `small_resource_lock` is locked)
		    // YES, this section needs refactoring, seriously
		    if (cur_video_info.comments.size()) { // comments
			    std::vector<std::pair<float, PostView *>>
			        comments_list; // list of comment views whose author's thumbnails should be loaded
			    {
				    constexpr int LOW = -800;
				    constexpr int HIGH = 1040;
				    float cur_y = -comment_tab_view->get_offset();
				    for (size_t i = 0; i < comments_main_view->views.size(); i++) {
					    float cur_height = comments_main_view->views[i]->get_height();
					    if (cur_y < HIGH && cur_y + cur_height >= LOW) {
						    auto parent_comment_view = dynamic_cast<PostView *>(comments_main_view->views[i]);
						    if (cur_y + parent_comment_view->get_self_height() >= LOW) {
							    comments_list.push_back({cur_y, parent_comment_view});
						    }
						    auto list = parent_comment_view->get_reply_pos_list(); // {y offset, reply view}
						    for (auto j : list) {
							    float cur_reply_height = j.second->get_height();
							    if (cur_y + j.first < HIGH && cur_y + j.first + cur_reply_height > LOW) {
								    comments_list.push_back({cur_y + j.first, j.second});
							    }
						    }
					    }
					    cur_y += cur_height;
				    }
				    if (comments_list.size() > MAX_COMMENT_ICON_LOAD_REQUEST) {
					    int leftover = comments_list.size() - MAX_COMMENT_ICON_LOAD_REQUEST;
					    comments_list.erase(comments_list.begin(), comments_list.begin() + leftover / 2);
					    comments_list.erase(comments_list.end() - (leftover - leftover / 2), comments_list.end());
				    }
			    }

			    std::set<PostView *> newly_loading_views, cancelling_views;
			    for (auto i : comment_thumbnail_loaded_list) {
				    cancelling_views.insert(i);
			    }
			    for (auto i : comments_list) {
				    newly_loading_views.insert(i.second);
			    }
			    for (auto i : comment_thumbnail_loaded_list) {
				    newly_loading_views.erase(i);
			    }
			    for (auto i : comments_list) {
				    cancelling_views.erase(i.second);
			    }

			    for (auto i : cancelling_views) {
				    thumbnail_cancel_request(i->author_icon_handle);
				    i->author_icon_handle = -1;
				    comment_thumbnail_loaded_list.erase(i);
			    }
			    for (auto i : newly_loading_views) {
				    i->author_icon_handle =
				        thumbnail_request(i->author_icon_url, SceneType::VIDEO_PLAYER, 0, ThumbnailType::ICON);
				    comment_thumbnail_loaded_list.insert(i);
			    }

			    std::vector<std::pair<int, int>> priority_list;
			    auto priority = [&](float y) {
				    if (y < 0) {
					    return 500 + y / 100;
				    }
				    if (y < 240) {
					    return PRIORITY_FOREGROUND + y;
				    }
				    return 500 + (240 - y) / 100;
			    };
			    for (auto i : comments_list) {
				    priority_list.push_back({i.second->author_icon_handle, priority(i.first)});
			    }
			    thumbnail_set_priorities(priority_list);
		    }

		    update_overlay_menu(&key);
		    auto_quality_tick();
		    ab_loop_tick();
		    channel_speed_tick();

		    bool popup_took_input = false; // the keys must not be handled a second time after the lock
		    if (mini_player_update(key)) {
			    popup_took_input = true; // the mini player is open : it takes all the input
		    } else if (qr_popup_update(key)) {
			    popup_took_input = true; // the QR popup is open : it takes all the input
		    } else if (text_popup_view->is_visible) {
			    text_popup_view->update(key);
		    } else if (comment_dialog->is_visible) {
			    comment_dialog->update(key);
		    } else if (account_pl_popup_view->is_visible) {
			    account_pl_popup_view->update(key);
		    } else if (equalizer_popup_view->is_visible) {
			    equalizer_popup_view->update(key);
		    } else {
			    main_view->update(key);
		    }

		    if (channel_id_pressed != "") {
			    global_intent.next_scene = SceneType::CHANNEL;
			    global_intent.arg = channel_id_pressed;
			    channel_id_pressed = "";
		    }
		    if (suggestion_clicked_url != "") {
			    global_intent.next_scene = SceneType::VIDEO_PLAYER;
			    global_intent.arg = suggestion_clicked_url;
			    suggestion_clicked_url = "";
		    }
		    if (bookmarks_rebuild_request && bookmarks_video_id == cur_video_info.id) {
			    bookmarks_rebuild_request = false;
			    rebuild_bookmarks_list();
		    }

		    small_resource_lock.unlock();
		    if (popup_took_input) {
			    key = Hid_info(); // nothing is pressed for the code below
		    }

		    {   // account actions that need the keyboard or a popup : handled here, outside of the view update
			    std::string playlists_video_id;
			    std::vector<AccountPlaylist> account_lists;
			    if (account_playlists_take(playlists_video_id, account_lists)) {
				    show_account_playlists(playlists_video_id, account_lists);
			    }
			    autoplay_countdown_tick(); // the countdown before an automatic next video
			    if (collab_request_id != "") { // collaboration : load the list of channels
				    std::string *id = new std::string(collab_request_id);
				    collab_request_id = "";
				    notice_show(tr("Loading the channels...", "Cargando los canales..."));
				    queue_async_task(load_collab_task, id);
			    }
			    if (chat_request) {
				    chat_request = false;
				    live_chat_open(cur_video_info.id);
				    show_live_chat();
			    } else if (live_chat_is_open()) {
				    if (!account_pl_popup_view->is_visible) live_chat_close(); // the screen was left with B
				    else if (live_chat_version() != chat_shown_version) show_live_chat();
			    }
			    std::vector<YouTubeChannelSuccinct> collab_list;
			    if (collab_take(collab_list)) {
				    show_collab_channels(collab_list);
			    } else if (collab_refresh_request) { // rebuild the open screen so the subscribed marks are up to date
				    collab_refresh_request = false;
				    if (account_pl_popup_view->is_visible && collab_shown.size()) show_collab_channels(std::vector<YouTubeChannelSuccinct>(collab_shown));
			    }
			    std::string translation_title, translation_text;
			    if (translation_take(translation_title, translation_text)) {
				    show_text_popup(translation_title, translation_text);
			    }
			    auto ask_comment_text = [](const std::string &hint, std::string &out) {
				    SwkbdState keyboard;
				    swkbdInit(&keyboard, SWKBD_TYPE_NORMAL, 2, 500);
				    swkbdSetFeatures(&keyboard, SWKBD_DEFAULT_QWERTY | SWKBD_PREDICTIVE_INPUT | SWKBD_MULTILINE);
				    swkbdSetValidation(&keyboard, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
				    swkbdSetHintText(&keyboard, hint.c_str());
				    swkbdSetButton(&keyboard, SWKBD_BUTTON_LEFT, LOCALIZED(CANCEL).c_str(), false);
				    swkbdSetButton(&keyboard, SWKBD_BUTTON_RIGHT, LOCALIZED(OK).c_str(), true);
				    static char buffer[2048];
				    buffer[0] = 0;
				    video_set_skip_drawing(true);
				    auto pressed = swkbdInputText(&keyboard, buffer, sizeof(buffer));
				    video_set_skip_drawing(false);
				    var_need_refresh = true;
				    if (pressed != SWKBD_BUTTON_RIGHT) return false;
				    out = buffer;
				    return out != "";
			    };
			    std::string comments_token = cur_video_info.comments_account_token();
			    youtube_account_set_video(cur_video_info.id); // the account requests may need the video itself
			    if (comment_write_request) {
				    comment_write_request = false;
				    std::string text;
				    if (comments_token == "") {
					    notice_show(tr("Open the comments first", "Abre antes los comentarios"));
				    } else if (account_ready() && ask_comment_text(tr("Write a comment", "Escribe un comentario"), text)) {
					    account_comment_async(comments_token, text);
				    }
			    }
			    if (comment_reply_request) {
				    comment_reply_request = false;
				    std::string text;
				    if (account_ready() && ask_comment_text(tr("Write a reply", "Escribe una respuesta"), text)) {
					    account_comment_reply_async(comment_actions_token(comment_target_id), comment_target_id, text);
				    }
			    }
			    if (chat_write_request) {
				    chat_write_request = false;
				    std::string text;
				    if (account_ready() && ask_comment_text(tr("Write in the chat", "Escribe en el chat"), text)) {
					    account_live_chat_async(cur_video_info.id, text);
				    }
			    }
			    if (comment_menu_request != "") {
				    comment_target_id = comment_menu_request;
				    comment_menu_request = "";
				    comment_dialog->get_message_view()->set_text_lines(std::vector<std::string>{tr("Comment", "Comentario")})->update_y_range(0, 30);
				    bool own_comment = false; // a comment of the signed-in account : Delete takes the place of Translate
				    for (auto &c : cur_video_info.comments) {
					    if (c.id == comment_target_id && OAuth::is_authenticated() && c.author.id != "" && c.author.id == OAuth::get_user_channel_id()) own_comment = true;
				    }
				    comment_dialog->set_buttons<std::function<std::string()>>(
				        {[]() { return tr("Like", "Me gusta"); }, []() { return tr("Reply", "Responder"); },
				         own_comment ? (std::function<std::string()>)[]() { return tr("Delete", "Borrar"); } : (std::function<std::string()>)[]() { return tr("Translate", "Traducir"); },
				         []() { return LOCALIZED(CANCEL); }},
				        [own_comment](OverlayDialogView &, int button) {
					        if (button == 0) {
						        std::string token = comment_actions_token(comment_target_id);
						        youtube_account_set_video(cur_video_info.id);
						        if (token == "") notice_show(tr("Open the comments first", "Abre antes los comentarios"));
						        else account_comment_like_async(token, comment_target_id);
					        } else if (button == 1) {
						        comment_reply_request = true;
					        } else if (button == 2 && own_comment) {
						        std::string token = comment_actions_token(comment_target_id);
						        youtube_account_set_video(cur_video_info.id);
						        if (token == "") notice_show(tr("Open the comments first", "Abre antes los comentarios"));
						        else account_comment_delete_async(token, comment_target_id);
					        } else if (button == 2) {
						        for (auto &c : cur_video_info.comments) { // the text of the long pressed comment
							        if (c.id == comment_target_id) {
								        translate_async(c.author.name, c.content);
								        break;
							        }
						        }
					        }
					        return true;
				        });
				    comment_dialog->set_is_visible(true);
				    var_need_refresh = true;
			    }
		    }

		    if (bookmark_add_request) { // asked from a button : the keyboard must be opened outside of the view update
			    bookmark_add_request = false;
			    std::string vid = playing_video_info.id;
			    double at = vid_current_pos;
			    SwkbdState keyboard;
			    swkbdInit(&keyboard, SWKBD_TYPE_NORMAL, 2, 40);
			    swkbdSetFeatures(&keyboard, SWKBD_DEFAULT_QWERTY);
			    swkbdSetHintText(&keyboard, tr("Bookmark name", "Nombre del marcador").c_str());
			    swkbdSetInitialText(&keyboard, "");
			    char name[64] = {0};
			    video_set_skip_drawing(true);
			    auto pressed = swkbdInputText(&keyboard, name, sizeof(name));
			    video_set_skip_drawing(false);
			    if (pressed == SWKBD_BUTTON_RIGHT && vid != "") {
				    bookmark_add(vid, at, name[0] ? name : Util_convert_seconds_to_time(at));
				    bookmarks_rebuild_request = true;
			    }
			    var_need_refresh = true;
		    }
		    /* ****************************** LOCK END ******************************  */

		    if (video_playing_bar_show) {
			    video_update_playing_bar(key);
		    }
		    if (key.p_a) {
			    if (vid_play_request) {
				    if (vid_pausing) {
					    if (!vid_pausing_seek) {
						    Util_speaker_resume(0);
					    }
					    if (eof_reached) {
						    send_seek_request(0);
					    }
					    vid_pausing = false;
				    } else {
					    if (!vid_pausing_seek) {
						    Util_speaker_pause(0);
					    }
					    vid_pausing = true;
				    }
			    }
		    } else if ((key.h_x && key.p_b) || (key.h_b && key.p_x)) {
			    vid_play_request = false;
		    } else if (key.p_x && var_button_x) {
			    run_button_action(var_button_x);
		    } else if (key.p_y && var_button_y) {
			    run_button_action(var_button_y);
		    } else if (key.p_b) {
			    global_intent.next_scene = SceneType::BACK;
		    } else if (key.p_d_right || key.p_d_left) {
			    if (network_decoder.ready) {
				    double pos = vid_current_pos;
				    pos += key.p_d_right ? 10 : -10;
				    pos = std::max<double>(0, pos);
				    pos = std::min<double>(vid_duration, pos);
				    send_seek_request(pos);
			    }
		    } else if (key.p_r && var_button_r) { // R : next video of the queue by default
			    run_button_action(var_button_r);
		    } else if (key.p_l && var_button_l) { // L : restart by default
			    run_button_action(var_button_l);
		    } else if (key.p_zr || key.p_zl) {
			    if (network_decoder.ready) {
				    double pos = vid_current_pos;
				    pos += key.p_zr ? 5 : -5;
				    pos = std::max<double>(0, pos);
				    pos = std::min<double>(vid_duration, pos);
				    send_seek_request(pos);
			    }
		    }
	    }
    }
