#include "data_io/feed_seen.hpp"
#include "ui/notice.hpp"
#include "headers.hpp"
#include "util/pinned_channels.hpp"
#include <vector>
#include <string>
#include <set>
#include <map>
#include <numeric>

#include "scenes/home.hpp"
#include "util/play_queue.hpp"
#include "scenes/search.hpp"
#include "scenes/studio.hpp"
#include "scenes/video_player.hpp"
#include "data_io/subscription_util.hpp"
#include "youtube_parser/parser.hpp"
#include "ui/ui.hpp"
#include "ui/overlay.hpp"
#include "network_decoder/thumbnail_loader.hpp"
#include "util/misc_tasks.hpp"
#include "util/async_task.hpp"
#include "util/result_filter.hpp"
#include "data_io/last_played.hpp"
#include "oauth/oauth.hpp"
#include "rapidjson_wrapper.hpp"

#define MAX_THUMBNAIL_LOAD_REQUEST 12

#define FEED_RELOAD_BUTTON_HEIGHT 18
#define TOP_HEIGHT 25
#define VIDEO_TITLE_MAX_WIDTH (320 - SMALL_MARGIN * 2 - VIDEO_LIST_THUMBNAIL_WIDTH)

namespace Home {
bool thread_suspend = false;
bool already_init = false;
bool exiting = false;

Mutex resource_lock;

YouTubeHomeResult home_info;
std::vector<SubscriptionChannel> subscribed_channels;
std::vector<SubscriptionChannel> oauth_subscribed_channels;
bool clicked_is_video;
std::string clicked_url;

bool last_oauth_state = false;

int feed_loading_progress = 0;
int feed_loading_total = 0;

int CONTENT_Y_HIGH = 240; // changes according to whether the video playing bar is drawn or not

VerticalListView *main_view = NULL;
TabView *main_tab_view = NULL;
ScrollView *home_tab_view = NULL;
VerticalListView *home_videos_list_view = NULL;
View *home_videos_bottom_view = new EmptyView(0, 0, 320, 0);
View *channels_tab_view = NULL;
ScrollView *local_channels_tab_view = NULL;
VerticalListView *local_channels_list_view = NULL;
ScrollView *oauth_channels_tab_view = NULL;
VerticalListView *oauth_channels_list_view = NULL;
View *feed_tab_view = NULL;
ScrollView *local_feed_videos_view = NULL;
VerticalListView *local_feed_videos_list_view = NULL;
ScrollView *oauth_feed_videos_view = NULL;
VerticalListView *oauth_feed_videos_list_view = NULL;
View *oauth_feed_videos_bottom_view = new EmptyView(0, 0, 320, 0);

std::string oauth_feed_continuation_token = "";
bool oauth_feed_has_more = true;
std::string oauth_feed_error = "";
}; // namespace Home
using namespace Home;

static void load_home_page(void *);
static void load_home_page_more(void *);
static void load_subscription_feed(void *);
static SuccinctVideoView *make_feed_view(const YouTubeVideoSuccinct &video, bool is_new);
static std::vector<YouTubeVideoSuccinct> feed_cache_load();
static std::vector<std::string> feed_latest_ids; // ids of the videos of the last local feed load
static void load_oauth_subscription_feed(void *);
static void load_oauth_subscription_feed_more(void *);
static void load_oauth_subscribed_channels(void *);
static void update_subscribed_channels(const std::vector<SubscriptionChannel> &new_subscribed_channels);
static void update_oauth_subscribed_channels(const std::vector<SubscriptionChannel> &new_oauth_channels);

void Home_init(void) {
	logger.info("subsc/init", "Initializing...");

	home_videos_list_view = (new VerticalListView(0, 0, 320))
	                            ->set_margin(SMALL_MARGIN)
	                            ->set_flow(true)
	                            ->enable_thumbnail_request_update(MAX_THUMBNAIL_LOAD_REQUEST, SceneType::HOME);
	home_tab_view = (new ScrollView(0, 0, 320, 0))
	                    ->set_views({home_videos_list_view, home_videos_bottom_view})
	                    ->set_pull_to_refresh(!var_disable_pull_to_refresh, []() {
		                    if (!is_async_task_running(load_home_page)) {
			                    queue_async_task(load_home_page, NULL);
		                    }
	                    });
	local_channels_list_view = (new VerticalListView(0, 0, 320))
	                               ->set_margin(SMALL_MARGIN)
	                               ->set_flow(true)
	                            ->enable_thumbnail_request_update(MAX_THUMBNAIL_LOAD_REQUEST, SceneType::HOME);
	local_channels_tab_view = (new ScrollView(0, 0, 320, 0))->set_views({local_channels_list_view});
	oauth_channels_list_view = (new VerticalListView(0, 0, 320))
	                               ->set_margin(SMALL_MARGIN)
	                               ->set_flow(true)
	                            ->enable_thumbnail_request_update(MAX_THUMBNAIL_LOAD_REQUEST, SceneType::HOME);
	oauth_channels_tab_view = (new ScrollView(0, 0, 320, 0))
	                              ->set_views({oauth_channels_list_view})
	                              ->set_pull_to_refresh(!var_disable_pull_to_refresh, []() {
		                              if (!is_async_task_running(load_oauth_subscribed_channels)) {
			                              queue_async_task(load_oauth_subscribed_channels, NULL);
		                              }
	                              });

	if (OAuth::is_authenticated()) {
		if (var_disable_pull_to_refresh) {
			View *oauth_channels_view =
			    (new VerticalListView(0, 0, 320))
			        ->set_views({(new TextView(0, 0, 320, FEED_RELOAD_BUTTON_HEIGHT))
			                         ->set_text((std::function<std::string()>)[]() {
				                         auto res = LOCALIZED(RELOAD);
				                         if (is_async_task_running(load_oauth_subscribed_channels)) {
					                         res += " ...";
				                         }
				                         return res;
			                         })
			                         ->set_text_offset(SMALL_MARGIN, -1)
			                         ->set_on_view_released([](View &) {
				                         if (!is_async_task_running(load_oauth_subscribed_channels)) {
					                         queue_async_task(load_oauth_subscribed_channels, NULL);
				                         }
			                         })
			                         ->set_get_background_color([](const View &view) -> u32 {
				                         if (is_async_task_running(load_oauth_subscribed_channels)) {
					                         return LIGHT0_BACK_COLOR;
				                         }
				                         return View::STANDARD_BACKGROUND(view);
			                         }),
			                     (new RuleView(0, 0, 320, SMALL_MARGIN))
			                         ->set_margin(0)
			                         ->set_get_background_color([](const View &) { return DEFAULT_BACK_COLOR; }),
			                     oauth_channels_tab_view})
			        ->set_draw_order({2, 1, 0});
			channels_tab_view =
			    (new TabView(0, 0, 320, 0))
			        ->set_views({oauth_channels_view, local_channels_tab_view})
			        ->set_tab_texts<std::function<std::string()>>(
			            {[]() { return LOCALIZED(ACCOUNT); }, []() { return LOCALIZED(LOCAL_CHANNELS); }})
			        ->set_lr_tab_switch_enabled(false);
		} else {
			channels_tab_view =
			    (new TabView(0, 0, 320, 0))
			        ->set_views({oauth_channels_tab_view, local_channels_tab_view})
			        ->set_tab_texts<std::function<std::string()>>(
			            {[]() { return LOCALIZED(ACCOUNT); }, []() { return LOCALIZED(LOCAL_CHANNELS); }})
			        ->set_lr_tab_switch_enabled(false);
		}
	} else {
		channels_tab_view = local_channels_tab_view;
	}
	local_feed_videos_list_view = (new VerticalListView(0, 0, 320))
	                                  ->set_margin(SMALL_MARGIN)
	                                  ->set_flow(true)
	                            ->enable_thumbnail_request_update(MAX_THUMBNAIL_LOAD_REQUEST, SceneType::HOME);

	if (var_disable_pull_to_refresh) {
		local_feed_videos_view = (new ScrollView(0, 0, 320, 0))->set_views({local_feed_videos_list_view});
		feed_tab_view =
		    (new VerticalListView(0, 0, 320))
		        ->set_views(
		            {(new TextView(0, 0, 320, FEED_RELOAD_BUTTON_HEIGHT))
		                 ->set_text((std::function<std::string()>)[]() {
			                 auto res = LOCALIZED(RELOAD);
			                 if (is_async_task_running(load_subscription_feed)) {
				                 res += " (" + std::to_string(feed_loading_progress) + "/" +
				                        std::to_string(feed_loading_total) + ")";
			                 }
			                 return res;
		                 })
		                 ->set_text_offset(SMALL_MARGIN, -1)
		                 ->set_on_view_released([](View &) {
			                 if (!is_async_task_running(load_subscription_feed)) {
				                 queue_async_task(load_subscription_feed, NULL);
			                 }
		                 })
		                 ->set_get_background_color([](const View &view) -> u32 {
			                 if (is_async_task_running(load_subscription_feed)) {
				                 return LIGHT0_BACK_COLOR;
			                 }
			                 return View::STANDARD_BACKGROUND(view);
		                 }),
		             (new RuleView(0, 0, 320, SMALL_MARGIN))->set_margin(0)->set_get_background_color([](const View &) {
			             return DEFAULT_BACK_COLOR;
		             }),
		             local_feed_videos_view})
		        ->set_draw_order({2, 1, 0});
	} else {
		local_feed_videos_view =
		    (new ScrollView(0, 0, 320, 0))->set_views({local_feed_videos_list_view})->set_pull_to_refresh(true, []() {
			    if (!is_async_task_running(load_subscription_feed)) {
				    queue_async_task(load_subscription_feed, NULL);
			    }
		    });
		feed_tab_view = local_feed_videos_view;
	}

	oauth_feed_videos_list_view = (new VerticalListView(0, 0, 320))
	                                  ->set_margin(SMALL_MARGIN)
	                                  ->set_flow(true)
	                            ->enable_thumbnail_request_update(MAX_THUMBNAIL_LOAD_REQUEST, SceneType::HOME);

	if (var_disable_pull_to_refresh) {
		oauth_feed_videos_view =
		    (new ScrollView(0, 0, 320, 0))->set_views({oauth_feed_videos_list_view, oauth_feed_videos_bottom_view});
	} else {
		oauth_feed_videos_view = (new ScrollView(0, 0, 320, 0))
		                             ->set_views({oauth_feed_videos_list_view, oauth_feed_videos_bottom_view})
		                             ->set_pull_to_refresh(true, []() {
			                             if (!is_async_task_running(load_oauth_subscription_feed)) {
				                             queue_async_task(load_oauth_subscription_feed, NULL);
			                             }
		                             });
	}

	if (OAuth::is_authenticated()) {
		if (var_disable_pull_to_refresh) {
			View *local_feed_tab = feed_tab_view;
			View *oauth_feed_tab =
			    (new VerticalListView(0, 0, 320))
			        ->set_views({(new TextView(0, 0, 320, FEED_RELOAD_BUTTON_HEIGHT))
			                         ->set_text((std::function<std::string()>)[]() {
				                         auto res = LOCALIZED(RELOAD);
				                         if (is_async_task_running(load_oauth_subscription_feed)) {
					                         res += " ...";
				                         }
				                         return res;
			                         })
			                         ->set_text_offset(SMALL_MARGIN, -1)
			                         ->set_on_view_released([](View &) {
				                         if (!is_async_task_running(load_oauth_subscription_feed)) {
					                         queue_async_task(load_oauth_subscription_feed, NULL);
				                         }
			                         })
			                         ->set_get_background_color([](const View &view) -> u32 {
				                         if (is_async_task_running(load_oauth_subscription_feed)) {
					                         return LIGHT0_BACK_COLOR;
				                         }
				                         return View::STANDARD_BACKGROUND(view);
			                         }),
			                     (new RuleView(0, 0, 320, SMALL_MARGIN))
			                         ->set_margin(0)
			                         ->set_get_background_color([](const View &) { return DEFAULT_BACK_COLOR; }),
			                     oauth_feed_videos_view})
			        ->set_draw_order({2, 1, 0});
			feed_tab_view = (new TabView(0, 0, 320, 0))
			                    ->set_views({oauth_feed_tab, local_feed_tab})
			                    ->set_tab_texts<std::function<std::string()>>(
			                        {[]() { return LOCALIZED(ACCOUNT); }, []() { return LOCALIZED(LOCAL_CHANNELS); }})
			                    ->set_lr_tab_switch_enabled(false);
		} else {
			feed_tab_view = (new TabView(0, 0, 320, 0))
			                    ->set_views({oauth_feed_videos_view, local_feed_videos_view})
			                    ->set_tab_texts<std::function<std::string()>>(
			                        {[]() { return LOCALIZED(ACCOUNT); }, []() { return LOCALIZED(LOCAL_CHANNELS); }})
			                    ->set_lr_tab_switch_enabled(false);
		}
	}
	std::vector<View *> main_tab_views = {home_tab_view, channels_tab_view, feed_tab_view};
	std::vector<std::function<std::string()>> main_tab_texts = {[]() { return LOCALIZED(HOME); },
	                                                            []() { return LOCALIZED(SUBSCRIBED_CHANNELS); },
	                                                            []() { return LOCALIZED(NEW_VIDEOS); }};
	if (!var_hide_studio) { // (the setting can hide the Studio tab)
		main_tab_views.push_back(Studio_create_view());
		main_tab_texts.push_back([]() { return std::string("Studio"); });
	}
	main_tab_view = (new TabView(0, 0, 320, CONTENT_Y_HIGH - TOP_HEIGHT))
	                    ->set_views(main_tab_views)
	                    ->set_tab_texts<std::function<std::string()>>(main_tab_texts);
	main_view = (new VerticalListView(0, 0, 320))
	                ->set_views({(new CustomView(0, 0, 320, 25))
	                                 ->set_draw([](const CustomView &) { return Search_get_search_bar_view()->draw(); })
	                                 ->set_update([](const CustomView &, Hid_info key) {
		                                 return Search_get_search_bar_view()->update(key);
	                                 }),
	                             main_tab_view})
	                ->set_draw_order({1, 0});

	queue_async_task(load_home_page, NULL);
	load_subscription();

	// Update subscribed channels after loading subscription data
	resource_lock.lock();
	update_subscribed_channels(get_valid_subscribed_channels());
	resource_lock.unlock();

	if (!var_safe_mode) { // the previous feed, until the fresh one arrives
		auto cached = feed_cache_load();
		resource_lock.lock();
		for (auto &video : cached) {
			if (result_video_hidden(video.url, video.author)) continue; // same filter as the fresh feed
			local_feed_videos_list_view->views.push_back(make_feed_view(video, false));
		}
		resource_lock.unlock();
	}

	queue_async_task(load_subscription_feed, NULL);

	if (OAuth::is_authenticated()) {
		resource_lock.lock();
		update_oauth_subscribed_channels(get_oauth_subscribed_channels());
		resource_lock.unlock();
		queue_async_task(load_oauth_subscription_feed, NULL);
	}

	last_oauth_state = OAuth::is_authenticated();

	Home_resume("");
	already_init = true;
}
void Home_exit(void) {
	already_init = false;
	thread_suspend = false;
	exiting = true;

	resource_lock.lock();

	main_view->recursive_delete_subviews();
	delete main_view;
	main_view = NULL;
	Studio_exit();
	main_tab_view = NULL;
	feed_tab_view = NULL;
	local_feed_videos_view = NULL;
	local_feed_videos_list_view = NULL;
	oauth_feed_videos_view = NULL;
	oauth_feed_videos_list_view = NULL;
	channels_tab_view = NULL;
	local_channels_tab_view = NULL;
	local_channels_list_view = NULL;
	oauth_channels_tab_view = NULL;
	oauth_channels_list_view = NULL;

	resource_lock.unlock();

	logger.info("subsc/exit", "Exited.");
}
void Home_suspend(void) { thread_suspend = true; }

void Home_rebuild_channels_tab(void) {
	if (!already_init) {
		return;
	}

	resource_lock.lock();

	if (channels_tab_view) {
		std::vector<View *> main_views = main_tab_view->views;

		TabView *old_tab = dynamic_cast<TabView *>(channels_tab_view);
		if (old_tab) {
			old_tab->views.clear();
			delete old_tab;
		}
		channels_tab_view = NULL;

		oauth_channels_tab_view->set_pull_to_refresh(!var_disable_pull_to_refresh, []() {
			if (!is_async_task_running(load_oauth_subscribed_channels)) {
				queue_async_task(load_oauth_subscribed_channels, NULL);
			}
		});

		if (OAuth::is_authenticated()) {
			if (var_disable_pull_to_refresh) {
				View *oauth_channels_view =
				    (new VerticalListView(0, 0, 320))
				        ->set_views({(new TextView(0, 0, 320, FEED_RELOAD_BUTTON_HEIGHT))
				                         ->set_text((std::function<std::string()>)[]() {
					                         auto res = LOCALIZED(RELOAD);
					                         if (is_async_task_running(load_oauth_subscribed_channels)) {
						                         res += " ...";
					                         }
					                         return res;
				                         })
				                         ->set_text_offset(SMALL_MARGIN, -1)
				                         ->set_on_view_released([](View &) {
					                         if (!is_async_task_running(load_oauth_subscribed_channels)) {
						                         queue_async_task(load_oauth_subscribed_channels, NULL);
					                         }
				                         })
				                         ->set_get_background_color([](const View &view) -> u32 {
					                         if (is_async_task_running(load_oauth_subscribed_channels)) {
						                         return LIGHT0_BACK_COLOR;
					                         }
					                         return View::STANDARD_BACKGROUND(view);
				                         }),
				                     (new RuleView(0, 0, 320, SMALL_MARGIN))
				                         ->set_margin(0)
				                         ->set_get_background_color([](const View &) { return DEFAULT_BACK_COLOR; }),
				                     oauth_channels_tab_view})
				        ->set_draw_order({2, 1, 0});
				channels_tab_view =
				    (new TabView(0, 0, 320, 0))
				        ->set_views({oauth_channels_view, local_channels_tab_view})
				        ->set_tab_texts<std::function<std::string()>>(
				            {[]() { return LOCALIZED(ACCOUNT); }, []() { return LOCALIZED(LOCAL_CHANNELS); }})
				        ->set_lr_tab_switch_enabled(false);
			} else {
				channels_tab_view =
				    (new TabView(0, 0, 320, 0))
				        ->set_views({oauth_channels_tab_view, local_channels_tab_view})
				        ->set_tab_texts<std::function<std::string()>>(
				            {[]() { return LOCALIZED(ACCOUNT); }, []() { return LOCALIZED(LOCAL_CHANNELS); }})
				        ->set_lr_tab_switch_enabled(false);
			}
			update_oauth_subscribed_channels(get_oauth_subscribed_channels());
		} else {
			channels_tab_view = local_channels_tab_view;
		}

		main_views[1] = channels_tab_view;
		main_tab_view->views = main_views;
	}

	resource_lock.unlock();
	var_need_refresh = true;
}

void Home_update_local_channels(void) {
	if (!already_init) {
		return;
	}

	resource_lock.lock();
	update_subscribed_channels(get_valid_subscribed_channels());
	resource_lock.unlock();
	var_need_refresh = true;
}

void Home_rebuild_feed_tab(void) {
	if (!already_init) {
		return;
	}

	resource_lock.lock();

	if (feed_tab_view) {
		std::vector<View *> main_views = main_tab_view->views;

		TabView *old_tab = dynamic_cast<TabView *>(feed_tab_view);
		if (old_tab) {
			for (auto view : old_tab->views) {
				VerticalListView *vlist = dynamic_cast<VerticalListView *>(view);
				if (vlist) {
					delete vlist;
				}
			}
			old_tab->views.clear();
			delete old_tab;
		} else {
			VerticalListView *old_list = dynamic_cast<VerticalListView *>(feed_tab_view);
			if (old_list) {
				delete old_list;
			}
		}
		feed_tab_view = NULL;

		if (var_disable_pull_to_refresh) {
			local_feed_videos_view->set_pull_to_refresh(false, nullptr);
			feed_tab_view =
			    (new VerticalListView(0, 0, 320))
			        ->set_views({(new TextView(0, 0, 320, FEED_RELOAD_BUTTON_HEIGHT))
			                         ->set_text((std::function<std::string()>)[]() {
				                         auto res = LOCALIZED(RELOAD);
				                         if (is_async_task_running(load_subscription_feed)) {
					                         res += " (" + std::to_string(feed_loading_progress) + "/" +
					                                std::to_string(feed_loading_total) + ")";
				                         }
				                         return res;
			                         })
			                         ->set_text_offset(SMALL_MARGIN, -1)
			                         ->set_on_view_released([](View &) {
				                         if (!is_async_task_running(load_subscription_feed)) {
					                         queue_async_task(load_subscription_feed, NULL);
				                         }
			                         })
			                         ->set_get_background_color([](const View &view) -> u32 {
				                         if (is_async_task_running(load_subscription_feed)) {
					                         return LIGHT0_BACK_COLOR;
				                         }
				                         return View::STANDARD_BACKGROUND(view);
			                         }),
			                     (new RuleView(0, 0, 320, SMALL_MARGIN))
			                         ->set_margin(0)
			                         ->set_get_background_color([](const View &) { return DEFAULT_BACK_COLOR; }),
			                     local_feed_videos_view})
			        ->set_draw_order({2, 1, 0});
		} else {
			local_feed_videos_view->set_pull_to_refresh(true, []() {
				if (!is_async_task_running(load_subscription_feed)) {
					queue_async_task(load_subscription_feed, NULL);
				}
			});
			feed_tab_view = local_feed_videos_view;
		}

		if (OAuth::is_authenticated()) {
			if (var_disable_pull_to_refresh) {
				oauth_feed_videos_view->set_pull_to_refresh(false, nullptr);
				View *local_feed_tab = feed_tab_view;
				View *oauth_feed_tab =
				    (new VerticalListView(0, 0, 320))
				        ->set_views({(new TextView(0, 0, 320, FEED_RELOAD_BUTTON_HEIGHT))
				                         ->set_text((std::function<std::string()>)[]() {
					                         auto res = LOCALIZED(RELOAD);
					                         if (is_async_task_running(load_oauth_subscription_feed)) {
						                         res += " ...";
					                         }
					                         return res;
				                         })
				                         ->set_text_offset(SMALL_MARGIN, -1)
				                         ->set_on_view_released([](View &) {
					                         if (!is_async_task_running(load_oauth_subscription_feed)) {
						                         queue_async_task(load_oauth_subscription_feed, NULL);
					                         }
				                         })
				                         ->set_get_background_color([](const View &view) -> u32 {
					                         if (is_async_task_running(load_oauth_subscription_feed)) {
						                         return LIGHT0_BACK_COLOR;
					                         }
					                         return View::STANDARD_BACKGROUND(view);
				                         }),
				                     (new RuleView(0, 0, 320, SMALL_MARGIN))
				                         ->set_margin(0)
				                         ->set_get_background_color([](const View &) { return DEFAULT_BACK_COLOR; }),
				                     oauth_feed_videos_view})
				        ->set_draw_order({2, 1, 0});
				feed_tab_view =
				    (new TabView(0, 0, 320, 0))
				        ->set_views({oauth_feed_tab, local_feed_tab})
				        ->set_tab_texts<std::function<std::string()>>(
				            {[]() { return LOCALIZED(ACCOUNT); }, []() { return LOCALIZED(LOCAL_CHANNELS); }})
				        ->set_lr_tab_switch_enabled(false);
			} else {
				oauth_feed_videos_view->set_pull_to_refresh(true, []() {
					if (!is_async_task_running(load_oauth_subscription_feed)) {
						queue_async_task(load_oauth_subscription_feed, NULL);
					}
				});
				feed_tab_view =
				    (new TabView(0, 0, 320, 0))
				        ->set_views({oauth_feed_videos_view, local_feed_videos_view})
				        ->set_tab_texts<std::function<std::string()>>(
				            {[]() { return LOCALIZED(ACCOUNT); }, []() { return LOCALIZED(LOCAL_CHANNELS); }})
				        ->set_lr_tab_switch_enabled(false);
			}
		}

		main_views[2] = feed_tab_view;
		main_tab_view->views = main_views;
	}

	resource_lock.unlock();
	var_need_refresh = true;
}

void Home_update_pull_to_refresh(void) {
	if (!already_init) {
		return;
	}

	resource_lock.lock();

	home_tab_view->set_pull_to_refresh(!var_disable_pull_to_refresh, []() {
		if (!is_async_task_running(load_home_page)) {
			queue_async_task(load_home_page, NULL);
		}
	});

	oauth_channels_tab_view->set_pull_to_refresh(!var_disable_pull_to_refresh, []() {
		if (!is_async_task_running(load_oauth_subscribed_channels)) {
			queue_async_task(load_oauth_subscribed_channels, NULL);
		}
	});

	resource_lock.unlock();
	var_need_refresh = true;
}

void Home_resume(std::string arg) {
	(void)arg;

	// main_tab_view->on_resume();
	overlay_menu_on_resume();
	thread_suspend = false;
	var_need_refresh = true;

	// Rebuild tabs if login status changed
	bool current_oauth_state = OAuth::is_authenticated();
	if (current_oauth_state != last_oauth_state) {
		resource_lock.lock();
		update_subscribed_channels(get_valid_subscribed_channels());
		resource_lock.unlock();

		Home_rebuild_channels_tab();
		Home_rebuild_feed_tab();

		// Load OAuth subscription feed when logging in
		if (current_oauth_state && !is_async_task_running(load_oauth_subscription_feed)) {
			queue_async_task(load_oauth_subscription_feed, NULL);
		}

		last_oauth_state = current_oauth_state;
	}
}

// async functions
static SuccinctVideoView *convert_video_to_view(const YouTubeVideoSuccinct &video) {
	SuccinctVideoView *res = new SuccinctVideoView(0, 0, 320, VIDEO_LIST_THUMBNAIL_HEIGHT);
	res->set_title_lines(truncate_str(video.title, 320 - (VIDEO_LIST_THUMBNAIL_WIDTH + 3), 2, 0.5, 0.5));
	res->set_auxiliary_lines({video.views_str, video.publish_date});
	res->set_bottom_right_overlay(video.duration_text);
	res->set_queue_item(video.url, video.title, video.author);
	res->set_thumbnail_url(video.thumbnail_url);
	res->auto_grid(video.title);
	res->set_get_background_color(View::STANDARD_BACKGROUND);
	res->set_on_view_released([video](View &view) {
		clicked_url = video.url;
		clicked_is_video = true;
	});
	return res;
}
static void update_home_bottom_view(bool force_show_loading) {
	delete home_videos_bottom_view;
	if (home_info.has_more_results() || force_show_loading) {
		home_videos_bottom_view = (new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))
		                              ->set_text([]() {
			                              if (home_info.error != "") {
				                              return home_info.error;
			                              } else {
				                              return loading_text();
			                              }
		                              })
		                              ->set_x_alignment(TextView::XAlign::CENTER)
		                              ->set_on_drawn([](View &) {
			                              if (!is_async_task_running(load_home_page) &&
			                                  !is_async_task_running(load_home_page_more) && home_info.error == "") {
				                              queue_async_task(load_home_page_more, NULL);
			                              }
		                              });
	} else {
		home_videos_bottom_view = new EmptyView(0, 0, 320, 0);
	}
	home_tab_view->set_views({home_videos_list_view, home_videos_bottom_view});
}

static void update_oauth_feed_bottom_view(bool force_show_loading) {
	delete oauth_feed_videos_bottom_view;
	if (oauth_feed_has_more || force_show_loading) {
		oauth_feed_videos_bottom_view = (new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))
		                                    ->set_text([]() {
			                                    if (oauth_feed_error != "") {
				                                    return oauth_feed_error;
			                                    } else {
				                                    return loading_text();
			                                    }
		                                    })
		                                    ->set_x_alignment(TextView::XAlign::CENTER)
		                                    ->set_on_drawn([](View &) {
			                                    if (!is_async_task_running(load_oauth_subscription_feed) &&
			                                        !is_async_task_running(load_oauth_subscription_feed_more) &&
			                                        oauth_feed_error == "" && OAuth::is_authenticated()) {
				                                    queue_async_task(load_oauth_subscription_feed_more, NULL);
			                                    }
		                                    });
	} else {
		oauth_feed_videos_bottom_view = new EmptyView(0, 0, 320, 0);
	}
	oauth_feed_videos_view->set_views({oauth_feed_videos_list_view, oauth_feed_videos_bottom_view});
}

static void load_home_page(void *) {
	resource_lock.lock();
	update_home_bottom_view(true);
	var_need_refresh = true;
	resource_lock.unlock();

	add_cpu_limit(ADDITIONAL_CPU_LIMIT);
	auto results = youtube_load_home_page();
	remove_cpu_limit(ADDITIONAL_CPU_LIMIT);
	results.videos.erase(std::remove_if(results.videos.begin(), results.videos.end(),
	                                    [](const YouTubeVideoSuccinct &video) { return result_video_hidden(video.url, video.author); }),
	                     results.videos.end());

	logger.info("home", "truncate/view creation start");
	std::vector<View *> new_videos_view;
	for (auto video : results.videos) {
		new_videos_view.push_back(convert_video_to_view(video));
	}
	{ // continue watching : the last video that was played, first in the list
		std::string last_id, last_title;
		if (last_played_get(last_id, last_title)) {
			YouTubeVideoSuccinct last;
			last.url = youtube_get_video_url_by_id(last_id);
			last.title = last_title;
			last.thumbnail_url = youtube_get_video_thumbnail_url_by_id(last_id);
			last.views_str = tr("Continue watching", "Seguir viendo");
			new_videos_view.insert(new_videos_view.begin(), convert_video_to_view(last));
		}
	}
	logger.info("home", "truncate/view creation end");

	resource_lock.lock();
	if (results.error == "") {
		home_info = results;
		home_videos_list_view->recursive_delete_subviews();
		home_videos_list_view->set_views(new_videos_view);
		update_home_bottom_view(false);
	} else {
		home_info.error = results.error;
	}

	if (home_tab_view) {
		home_tab_view->finish_pull_refresh();
	}

	var_need_refresh = true;
	resource_lock.unlock();
}
static void load_home_page_more(void *) {
	auto new_result = home_info;
	new_result.load_more_results();
	{
		size_t old_size = std::min(home_info.videos.size(), new_result.videos.size());
		new_result.videos.erase(std::remove_if(new_result.videos.begin() + old_size, new_result.videos.end(),
		                                       [](const YouTubeVideoSuccinct &video) { return result_video_hidden(video.url, video.author); }),
		                        new_result.videos.end());
	}

	logger.info("home-c", "truncate/view creation start");
	std::vector<View *> new_videos_view;
	for (size_t i = home_info.videos.size(); i < new_result.videos.size(); i++) {
		new_videos_view.push_back(convert_video_to_view(new_result.videos[i]));
	}
	logger.info("home-c", "truncate/view creation end");

	resource_lock.lock();
	home_info = new_result;
	if (new_result.error == "") {
		home_videos_list_view->views.insert(home_videos_list_view->views.end(), new_videos_view.begin(),
		                                    new_videos_view.end());
		update_home_bottom_view(false);
	}
	resource_lock.unlock();
}
#define FEED_CACHE_PATH "sdmc:/3ds/IkersTube/feed_cache.txt"
#define FEED_CACHE_MAX 60

static SuccinctVideoView *make_feed_view(const YouTubeVideoSuccinct &video, bool is_new) {
	SuccinctVideoView *cur_view = (new SuccinctVideoView(0, 0, 320, VIDEO_LIST_THUMBNAIL_HEIGHT));
	cur_view->set_title_lines(truncate_str(video.title, VIDEO_TITLE_MAX_WIDTH, 2, 0.5, 0.5));
	cur_view->set_thumbnail_url(video.thumbnail_url);
	cur_view->set_auxiliary_lines({video.publish_date, video.views_str});
	cur_view->set_bottom_right_overlay(video.duration_text);
	cur_view->set_queue_item(video.url, video.title, video.author);
	cur_view->set_get_background_color(View::STANDARD_BACKGROUND);
	cur_view->badge_new = is_new;
	cur_view->set_on_view_released([video](View &view) {
		clicked_url = video.url;
		clicked_is_video = true;
	});
	return cur_view;
}

// the last feed is kept on the SD card so the Feed tab has something to show the moment the app opens
static void feed_cache_save(const std::vector<YouTubeVideoSuccinct> &videos) {
	if (videos.empty() || var_incognito) return;
	auto clean = [](std::string text) {
		for (auto &c : text) if (c == '\t' || c == '\n' || c == '\r') c = ' ';
		return text;
	};
	std::string tmp = std::string(FEED_CACHE_PATH) + ".tmp";
	FILE *f = fopen(tmp.c_str(), "w");
	if (!f) return;
	for (size_t i = 0; i < videos.size() && i < FEED_CACHE_MAX; i++) {
		auto &v = videos[i];
		fprintf(f, "%s\t%s\t%s\t%s\t%s\t%s\t%s\n", clean(v.url).c_str(), clean(v.title).c_str(), clean(v.author).c_str(), clean(v.duration_text).c_str(),
		        clean(v.publish_date).c_str(), clean(v.views_str).c_str(), clean(v.thumbnail_url).c_str());
	}
	fclose(f);
	remove(FEED_CACHE_PATH);
	rename(tmp.c_str(), FEED_CACHE_PATH);
}

static std::vector<YouTubeVideoSuccinct> feed_cache_load() {
	std::vector<YouTubeVideoSuccinct> videos;
	FILE *f = fopen(FEED_CACHE_PATH, "r");
	if (!f) return videos;
	char line[2048];
	while (videos.size() < FEED_CACHE_MAX && fgets(line, sizeof line, f)) {
		std::string text = line;
		while (text.size() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
		std::vector<std::string> parts;
		size_t start = 0;
		while (true) {
			size_t tab = text.find('\t', start);
			if (tab == std::string::npos) { parts.push_back(text.substr(start)); break; }
			parts.push_back(text.substr(start, tab - start));
			start = tab + 1;
		}
		if (parts.size() != 7 || parts[0] == "") continue;
		YouTubeVideoSuccinct v;
		v.url = parts[0], v.title = parts[1], v.author = parts[2], v.duration_text = parts[3];
		v.publish_date = parts[4], v.views_str = parts[5], v.thumbnail_url = parts[6];
		videos.push_back(v);
	}
	fclose(f);
	return videos;
}

static void load_subscription_feed(void *) {
	resource_lock.lock();
	auto channels = subscribed_channels;
	resource_lock.unlock();

	std::vector<std::string> ids;
	for (auto channel : channels) {
		ids.push_back(channel.id);
	}
	add_cpu_limit(ADDITIONAL_CPU_LIMIT);
	auto results = youtube_load_channel_page_multi(ids, [](int cur, int total) {
		feed_loading_progress = cur;
		feed_loading_total = total;
		var_need_refresh = true;
	});
	remove_cpu_limit(ADDITIONAL_CPU_LIMIT);

	std::map<std::pair<int, int>, std::vector<YouTubeVideoSuccinct>> loaded_videos;
	for (auto result : results) {
		// update the subscription metadata at the same time
		if (result.name != "") {
			SubscriptionChannel new_info;
			new_info.id = result.id;
			new_info.url = result.url;
			new_info.name = result.name;
			new_info.icon_url = result.icon_url;
			new_info.subscriber_count_str = result.subscriber_count_str;
			subscription_unsubscribe(result.id);
			subscription_subscribe(new_info);
		}

		int loaded_cnt = 0;
		for (auto video : result.videos) {
			std::string date_number_str;
			for (auto c : video.publish_date) {
				if (isdigit(c)) {
					date_number_str.push_back(c);
				}
			}

			// 1 : seconds
			char *end;
			int number = strtoll(date_number_str.c_str(), &end, 10);
			if (*end) {
				logger.error("subsc", "failed to parse the integer in date : " + video.publish_date);
				continue;
			}
			int unit = -1;
			std::vector<std::vector<std::string>> unit_list = {{"second"}, {"minute"}, {"hour"}, {"day"},
			                                                   {"week"},   {"month"},  {"year"}};
			if (var_lang_content == "ja") {
				unit_list = {{"秒"}, {"分"}, {"時間"}, {"日"}, {"週間"}, {"月"}, {"年"}};
			} else if (var_lang_content == "de") {
				unit_list = {{"Sekunde"}, {"Minute"}, {"Stunde"}, {"Tag"}, {"Woche"}, {"Monat"}, {"Jahr"}};
			} else if (var_lang_content == "fr") {
				unit_list = {{"seconde"}, {"minute"}, {"heure"}, {"jour"}, {"semaine"}, {"mois"}, {"an"}};
			} else if (var_lang_content == "it") {
				unit_list = {{"second"}, {"minut"}, {"ora", "ore"}, {"giorn"}, {"settiman"}, {"mes"}, {"ann"}};
			} else if (var_lang_content == "es") {
				unit_list = {{"segundo"}, {"minuto"}, {"hora"}, {"día"}, {"semana"}, {"mes"}, {"año"}};
			} else if (var_lang_content != "en") {
				logger.error("i18n", "Units not found.");
			}
			for (size_t i = 0; i < unit_list.size(); i++) {
				bool matched = false;
				for (auto pattern : unit_list[i]) {
					if (video.publish_date.find(pattern) != std::string::npos) {
						matched = true;
						break;
					}
				}
				if (matched) {
					unit = i;
					break;
				}
			}
			if (unit == -1) {
				logger.error("subsc", "failed to parse the unit of date : " + video.publish_date);
				continue;
			}
			if (std::pair<int, int>{unit, number} > std::pair<int, int>{5, 2}) {
				continue; // more than 2 months old
			}
			loaded_cnt++;
			if (video.author == "") video.author = result.name; // (the channel pages do not name the author of their videos: the hide filters need it)
			loaded_videos[{unit, number}].push_back(video);
		}
		logger.info("subsc", "loaded " + result.name + " : " + std::to_string(loaded_cnt) + " video(s)");
	}

	std::vector<View *> new_feed_video_views;
	std::set<std::string> new_feed_ids;
	{ // new videos since the last time the feed tab was opened : red dot on the menu icon
		std::vector<std::string> latest_ids; // (built here and handed over under the lock: the UI thread reads feed_latest_ids)
		for (auto &i : loaded_videos) {
			for (auto &video : i.second) latest_ids.push_back(youtube_get_video_id_by_url(video.url));
		}
		resource_lock.lock();
		feed_latest_ids = latest_ids;
		resource_lock.unlock();
		new_feed_ids = feed_seen_new_set(latest_ids); // (asked before feed_seen_count_new, which records the ids on the very first run)
		var_new_feed_videos = feed_seen_count_new(latest_ids);
		static bool told_this_session = false; // once per session: how many new videos your channels have
		if (var_new_feed_videos > 0 && !told_this_session) {
			told_this_session = true;
			notice_show(std::to_string(var_new_feed_videos) + tr(" new videos from your channels", " vídeos nuevos de tus canales"));
		}
		var_need_refresh = true;
	}
	std::vector<YouTubeVideoSuccinct> shown_videos;
	for (auto &i : loaded_videos) {
		for (auto video : i.second) {
			if (result_video_hidden(video.url, video.author)) continue; // the settings hide Shorts / watched videos
			new_feed_video_views.push_back(make_feed_view(video, new_feed_ids.count(youtube_get_video_id_by_url(video.url)) > 0));
			shown_videos.push_back(video);
		}
	}
	feed_cache_save(shown_videos);
	{ // a button to play only the NEW ones, one after the other
		std::vector<std::pair<std::string, std::string>> fresh;
		for (auto &video : shown_videos) {
			if (new_feed_ids.count(youtube_get_video_id_by_url(video.url))) fresh.push_back({video.url, video.title});
		}
		if (shown_videos.size() >= 6) { // one random video of the feed
			std::vector<std::string> urls;
			for (auto &video : shown_videos) urls.push_back(video.url);
			new_feed_video_views.insert(new_feed_video_views.begin(),
			                            (new TextView(0, 0, 320, 24))
			                                ->set_text(tr("Surprise me from my feed", "Sorpréndeme con mi feed"))
			                                ->set_x_alignment(TextView::XAlign::CENTER)
			                                ->set_y_alignment(TextView::YAlign::CENTER)
			                                ->set_get_background_color(View::STANDARD_BACKGROUND)
			                                ->set_corner_radius(6)
			                                ->set_on_view_released([urls](View &) {
				                                srand((unsigned)osGetTime());
				                                clicked_url = urls[rand() % urls.size()];
				                                clicked_is_video = true;
			                                }));
		}
		if (fresh.size() >= 2) {
			new_feed_video_views.insert(new_feed_video_views.begin(),
			                            (new TextView(0, 0, 320, 24))
			                                ->set_text(tr("Play the new videos (", "Ver los vídeos nuevos (") + std::to_string(fresh.size()) + ")")
			                                ->set_x_alignment(TextView::XAlign::CENTER)
			                                ->set_y_alignment(TextView::YAlign::CENTER)
			                                ->set_get_background_color(View::STANDARD_BACKGROUND)
			                                ->set_corner_radius(6)
			                                ->set_on_view_released([fresh](View &) {
				                                play_queue_clear();
				                                for (size_t i = 1; i < fresh.size(); i++) play_queue_toggle(fresh[i].first, fresh[i].second);
				                                clicked_url = fresh[0].first;
				                                clicked_is_video = true;
			                                }));
		}
	}

	misc_tasks_request(TASK_SAVE_SUBSCRIPTION);

	resource_lock.lock();
	if (exiting) { // app shut down while loading
		resource_lock.unlock();
		if (local_feed_videos_view && !var_disable_pull_to_refresh) {
			local_feed_videos_view->finish_pull_refresh();
		}
		return;
	}
	update_subscribed_channels(get_valid_subscribed_channels());

	if (channels.empty()) remove(FEED_CACHE_PATH); // no subscriptions: nothing to remember
	if (new_feed_video_views.empty() && !local_feed_videos_list_view->views.empty() && !channels.empty()) {
		// nothing could be loaded (offline?): keep what is on screen (the cached feed)
	} else {
		local_feed_videos_list_view->recursive_delete_subviews();
		local_feed_videos_list_view->views = new_feed_video_views;
	}

	if (local_feed_videos_view && !var_disable_pull_to_refresh) {
		local_feed_videos_view->finish_pull_refresh();
	}

	resource_lock.unlock();
}

static void load_oauth_subscription_feed(void *) {
	if (!OAuth::is_authenticated()) {
		resource_lock.lock();
		if (oauth_feed_videos_view && !var_disable_pull_to_refresh) {
			oauth_feed_videos_view->finish_pull_refresh();
		}
		resource_lock.unlock();
		return;
	}

	resource_lock.lock();
	oauth_feed_continuation_token = "";
	oauth_feed_has_more = true;
	oauth_feed_error = "";
	update_oauth_feed_bottom_view(true);
	var_need_refresh = true;
	resource_lock.unlock();

	add_cpu_limit(ADDITIONAL_CPU_LIMIT);
	RJson data = OAuth::fetch_browse_data("FEsubscriptions");
	remove_cpu_limit(ADDITIONAL_CPU_LIMIT);

	logger.info("home/oauth_feed", "truncate/view creation start");
	std::vector<View *> new_oauth_feed_video_views;
	std::vector<View *> live_views, upcoming_views; // shown first : what is on air now, then what starts soon
	std::vector<long long> upcoming_times;
	std::string soon_title; // an upcoming stream/premiere that starts within 15 minutes
	long long now_unix = (long long)time(NULL);

	if (data.is_valid()) {
		auto contents = data["contents"]["singleColumnBrowseResultsRenderer"]["tabs"][(size_t)0]["tabRenderer"]
		                    ["content"]["sectionListRenderer"]["contents"];

		for (auto section : contents.array_items()) {
			if (!section.has_key("itemSectionRenderer")) {
				continue;
			}

			auto items = section["itemSectionRenderer"]["contents"];
			for (auto item : items.array_items()) {
				if (!item.has_key("compactVideoRenderer")) {
					continue;
				}

				auto renderer = item["compactVideoRenderer"];

				if (!renderer.has_key("videoId")) {
					continue;
				}

				std::string video_id = renderer["videoId"].string_value();
				std::string title = "";
				std::string publish_date = "";
				std::string views_str = "";
				std::string duration_text = "";
				std::string thumbnail_url = "";

				if (renderer.has_key("title") && renderer["title"].has_key("runs") &&
				    renderer["title"]["runs"].array_items().size() > 0) {
					title = renderer["title"]["runs"][(size_t)0]["text"].string_value();
				}

				if (renderer.has_key("publishedTimeText") && renderer["publishedTimeText"].has_key("runs") &&
				    renderer["publishedTimeText"]["runs"].array_items().size() > 0) {
					publish_date = renderer["publishedTimeText"]["runs"][(size_t)0]["text"].string_value();
				}

				if (renderer.has_key("shortViewCountText") && renderer["shortViewCountText"].has_key("accessibility") &&
				    renderer["shortViewCountText"]["accessibility"].has_key("accessibilityData") &&
				    renderer["shortViewCountText"]["accessibility"]["accessibilityData"].has_key("label")) {
					views_str =
					    renderer["shortViewCountText"]["accessibility"]["accessibilityData"]["label"].string_value();
				}

				if (renderer.has_key("lengthText") && renderer["lengthText"].has_key("runs") &&
				    renderer["lengthText"]["runs"].array_items().size() > 0) {
					duration_text = renderer["lengthText"]["runs"][(size_t)0]["text"].string_value();
				}

				if (renderer.has_key("thumbnail") && renderer["thumbnail"].has_key("thumbnails")) {
					auto thumbnails = renderer["thumbnail"]["thumbnails"];
					if (thumbnails.array_items().size() > 0) {
						if (thumbnails.array_items().size() >= 2) {
							thumbnail_url = thumbnails[(size_t)1]["url"].string_value();
						} else if (thumbnails.array_items().size() >= 1) {
							thumbnail_url = thumbnails[(size_t)0]["url"].string_value();
						}
					}
				}

				bool is_live = false;
				long long start_time = 0;
				if (renderer.has_key("badges")) {
					for (auto badge : renderer["badges"].array_items()) {
						if (badge.has_key("metadataBadgeRenderer") && badge["metadataBadgeRenderer"]["style"].string_value() == "BADGE_STYLE_TYPE_LIVE_NOW") is_live = true;
					}
				}
				if (renderer.has_key("upcomingEventData")) {
					start_time = atoll(renderer["upcomingEventData"]["startTime"].string_value().c_str());
				}

				if (!video_id.empty() && !title.empty() && !result_video_hidden("https://m.youtube.com/watch?v=" + video_id)) {
					SuccinctVideoView *cur_view = (new SuccinctVideoView(0, 0, 320, VIDEO_LIST_THUMBNAIL_HEIGHT));
					if (is_live) {
						duration_text = tr("LIVE", "DIRECTO");
					} else if (start_time > 0) {
						long long left = start_time - now_unix;
						if (left > 0 && left < 900 && soon_title == "") soon_title = title;
						std::string when = left <= 0 ? tr("starting", "empezando") : left < 3600 ? std::to_string(left / 60 + 1) + " min" : left < 86400 ? std::to_string(left / 3600) + " h" : std::to_string(left / 86400) + tr(" d", " d");
						publish_date = tr("Upcoming: ", "Próximo: ") + when;
						duration_text = tr("SOON", "PRONTO");
					}

					cur_view->set_title_lines(truncate_str(title, VIDEO_TITLE_MAX_WIDTH, 2, 0.5, 0.5));
					cur_view->set_thumbnail_url(thumbnail_url);
					cur_view->set_auxiliary_lines({publish_date, views_str});
					cur_view->set_bottom_right_overlay(duration_text);
					cur_view->set_get_background_color(View::STANDARD_BACKGROUND);
					cur_view->set_on_view_released([video_id](View &view) {
						clicked_url = "https://m.youtube.com/watch?v=" + video_id;
						clicked_is_video = true;
					});

					if (is_live) live_views.push_back(cur_view);
					else if (start_time > 0) {
						size_t at = 0;
						while (at < upcoming_times.size() && upcoming_times[at] <= start_time) at++;
						upcoming_views.insert(upcoming_views.begin() + at, cur_view);
						upcoming_times.insert(upcoming_times.begin() + at, start_time);
					} else new_oauth_feed_video_views.push_back(cur_view);
				}
			}
		}
		new_oauth_feed_video_views.insert(new_oauth_feed_video_views.begin(), upcoming_views.begin(), upcoming_views.end());
		new_oauth_feed_video_views.insert(new_oauth_feed_video_views.begin(), live_views.begin(), live_views.end());
		if (soon_title != "") notice_show(tr("Starting soon: ", "Empieza pronto: ") + soon_title.substr(0, 40));

		if (data.has_key("contents") && data["contents"].has_key("singleColumnBrowseResultsRenderer") &&
		    data["contents"]["singleColumnBrowseResultsRenderer"].has_key("tabs")) {
			auto tabs = data["contents"]["singleColumnBrowseResultsRenderer"]["tabs"];
			if (tabs.array_items().size() > 0 && tabs[(size_t)0].has_key("tabRenderer") &&
			    tabs[(size_t)0]["tabRenderer"].has_key("content") &&
			    tabs[(size_t)0]["tabRenderer"]["content"].has_key("sectionListRenderer") &&
			    tabs[(size_t)0]["tabRenderer"]["content"]["sectionListRenderer"].has_key("continuations")) {
				auto continuations = tabs[(size_t)0]["tabRenderer"]["content"]["sectionListRenderer"]["continuations"];
				if (continuations.array_items().size() > 0 &&
				    continuations[(size_t)0].has_key("nextContinuationData") &&
				    continuations[(size_t)0]["nextContinuationData"].has_key("continuation")) {
					oauth_feed_continuation_token =
					    continuations[(size_t)0]["nextContinuationData"]["continuation"].string_value();
				} else {
					oauth_feed_has_more = false;
				}
			} else {
				oauth_feed_has_more = false;
			}
		} else {
			oauth_feed_has_more = false;
		}
	} else {
		oauth_feed_error = "Failed to load subscriptions";
		oauth_feed_has_more = false;
	}

	logger.info("home/oauth_feed", "truncate/view creation end");

	resource_lock.lock();

	if (exiting) {
		resource_lock.unlock();
		if (oauth_feed_videos_view && !var_disable_pull_to_refresh) {
			oauth_feed_videos_view->finish_pull_refresh();
		}
		return;
	}

	oauth_feed_videos_list_view->recursive_delete_subviews();
	oauth_feed_videos_list_view->set_views(new_oauth_feed_video_views);
	update_oauth_feed_bottom_view(false);

	if (oauth_feed_videos_view && !var_disable_pull_to_refresh) {
		oauth_feed_videos_view->finish_pull_refresh();
	}

	var_need_refresh = true;
	resource_lock.unlock();
}

static void load_oauth_subscription_feed_more(void *) {
	if (!OAuth::is_authenticated() || oauth_feed_continuation_token.empty() || !oauth_feed_has_more) {
		return;
	}

	add_cpu_limit(ADDITIONAL_CPU_LIMIT);
	RJson data = OAuth::fetch_browse_data_with_continuation("FEsubscriptions", oauth_feed_continuation_token);
	remove_cpu_limit(ADDITIONAL_CPU_LIMIT);

	std::vector<View *> new_oauth_feed_video_views;

	if (data.is_valid()) {
		auto continuation_contents = data["continuationContents"]["sectionListContinuation"]["contents"];

		for (auto section : continuation_contents.array_items()) {
			if (!section.has_key("itemSectionRenderer")) {
				continue;
			}

			auto items = section["itemSectionRenderer"]["contents"];
			for (auto item : items.array_items()) {
				if (!item.has_key("compactVideoRenderer")) {
					continue;
				}

				auto renderer = item["compactVideoRenderer"];

				if (!renderer.has_key("videoId")) {
					continue;
				}

				std::string video_id = renderer["videoId"].string_value();
				std::string title = "";
				std::string publish_date = "";
				std::string views_str = "";
				std::string duration_text = "";
				std::string thumbnail_url = "";

				if (renderer.has_key("title") && renderer["title"].has_key("runs") &&
				    renderer["title"]["runs"].array_items().size() > 0) {
					title = renderer["title"]["runs"][(size_t)0]["text"].string_value();
				}

				if (renderer.has_key("publishedTimeText") && renderer["publishedTimeText"].has_key("runs") &&
				    renderer["publishedTimeText"]["runs"].array_items().size() > 0) {
					publish_date = renderer["publishedTimeText"]["runs"][(size_t)0]["text"].string_value();
				}

				if (renderer.has_key("shortViewCountText") && renderer["shortViewCountText"].has_key("accessibility") &&
				    renderer["shortViewCountText"]["accessibility"].has_key("accessibilityData") &&
				    renderer["shortViewCountText"]["accessibility"]["accessibilityData"].has_key("label")) {
					views_str =
					    renderer["shortViewCountText"]["accessibility"]["accessibilityData"]["label"].string_value();
				}

				if (renderer.has_key("lengthText") && renderer["lengthText"].has_key("runs") &&
				    renderer["lengthText"]["runs"].array_items().size() > 0) {
					duration_text = renderer["lengthText"]["runs"][(size_t)0]["text"].string_value();
				}

				if (renderer.has_key("thumbnail") && renderer["thumbnail"].has_key("thumbnails")) {
					auto thumbnails = renderer["thumbnail"]["thumbnails"];
					if (thumbnails.array_items().size() > 0) {
						if (thumbnails.array_items().size() >= 2) {
							thumbnail_url = thumbnails[(size_t)1]["url"].string_value();
						} else if (thumbnails.array_items().size() >= 1) {
							thumbnail_url = thumbnails[(size_t)0]["url"].string_value();
						}
					}
				}

				if (!video_id.empty() && !title.empty() && !result_video_hidden("https://m.youtube.com/watch?v=" + video_id)) {
					SuccinctVideoView *cur_view = (new SuccinctVideoView(0, 0, 320, VIDEO_LIST_THUMBNAIL_HEIGHT));

					cur_view->set_title_lines(truncate_str(title, VIDEO_TITLE_MAX_WIDTH, 2, 0.5, 0.5));
					cur_view->set_thumbnail_url(thumbnail_url);
					cur_view->set_auxiliary_lines({publish_date, views_str});
					cur_view->set_bottom_right_overlay(duration_text);
					cur_view->set_get_background_color(View::STANDARD_BACKGROUND);
					cur_view->set_on_view_released([video_id](View &view) {
						clicked_url = "https://m.youtube.com/watch?v=" + video_id;
						clicked_is_video = true;
					});

					new_oauth_feed_video_views.push_back(cur_view);
				}
			}
		}

		oauth_feed_continuation_token = "";
		if (data.has_key("continuationContents") && data["continuationContents"].has_key("sectionListContinuation") &&
		    data["continuationContents"]["sectionListContinuation"].has_key("continuations")) {
			auto continuations = data["continuationContents"]["sectionListContinuation"]["continuations"];
			if (continuations.array_items().size() > 0 && continuations[(size_t)0].has_key("nextContinuationData") &&
			    continuations[(size_t)0]["nextContinuationData"].has_key("continuation")) {
				oauth_feed_continuation_token =
				    continuations[(size_t)0]["nextContinuationData"]["continuation"].string_value();
			} else {
				oauth_feed_has_more = false;
			}
		} else {
			oauth_feed_has_more = false;
		}

		if (oauth_feed_continuation_token.empty()) {
			oauth_feed_has_more = false;
		}
	} else {
		oauth_feed_error = "Failed to load more subscriptions";
		oauth_feed_has_more = false;
	}

	resource_lock.lock();

	if (exiting) {
		resource_lock.unlock();
		return;
	}

	oauth_feed_videos_list_view->views.insert(oauth_feed_videos_list_view->views.end(),
	                                          new_oauth_feed_video_views.begin(), new_oauth_feed_video_views.end());
	update_oauth_feed_bottom_view(false);

	var_need_refresh = true;
	resource_lock.unlock();
}

static void update_subscribed_channels(const std::vector<SubscriptionChannel> &new_subscribed_channels) {
	subscribed_channels = new_subscribed_channels;

	// prepare new views
	std::vector<View *> new_views;
	std::vector<SubscriptionChannel> sorted_channels = new_subscribed_channels; // alphabetical, like the apps people know
	if (!var_channels_unsorted) std::stable_sort(sorted_channels.begin(), sorted_channels.end(), [](const SubscriptionChannel &a, const SubscriptionChannel &b) {
		std::string x = a.name, y = b.name;
		for (auto &c : x) c = tolower(c);
		for (auto &c : y) c = tolower(c);
		return x < y;
	});
	std::stable_partition(sorted_channels.begin(), sorted_channels.end(), [](const SubscriptionChannel &c) { return pinned_channel_is(c.id); }); // pinned first
	for (auto channel : sorted_channels) {
		SuccinctChannelView *cur_view = (new SuccinctChannelView(0, 0, 320, CHANNEL_ICON_HEIGHT));
		cur_view->set_name((pinned_channel_is(channel.id) ? "* " : "") + channel.name);
		cur_view->set_auxiliary_lines({channel.subscriber_count_str});
		cur_view->set_thumbnail_url(channel.icon_url);
		cur_view->set_get_background_color(View::STANDARD_BACKGROUND);
		cur_view->add_on_long_hold(35, [channel](View &v) { // subscribe / bell
			overlay_show_channel_menu(channel.id, channel.name, channel.icon_url, channel.subscriber_count_str);
			v.reset_holding_status();
		});
		cur_view->set_on_view_released([channel](View &view) {
			clicked_url = channel.url;
			clicked_is_video = false;
		});
		new_views.push_back(cur_view);
	}

	local_channels_list_view->recursive_delete_subviews();
	local_channels_list_view->set_views(new_views);
}

static void update_oauth_subscribed_channels(const std::vector<SubscriptionChannel> &new_oauth_channels) {
	oauth_subscribed_channels = new_oauth_channels;

	std::vector<View *> new_views;
	std::vector<SubscriptionChannel> ordered_channels = new_oauth_channels;
	std::stable_partition(ordered_channels.begin(), ordered_channels.end(), [](const SubscriptionChannel &c) { return pinned_channel_is(c.id); }); // pinned first
	for (auto channel : ordered_channels) {
		SuccinctChannelView *cur_view = (new SuccinctChannelView(0, 0, 320, CHANNEL_ICON_HEIGHT));
		cur_view->set_name((pinned_channel_is(channel.id) ? "* " : "") + channel.name);
		cur_view->set_auxiliary_lines({channel.subscriber_count_str});
		cur_view->set_thumbnail_url(channel.icon_url);
		cur_view->set_get_background_color(View::STANDARD_BACKGROUND);
		cur_view->add_on_long_hold(35, [channel](View &v) { // subscribe / bell
			overlay_show_channel_menu(channel.id, channel.name, channel.icon_url, channel.subscriber_count_str);
			v.reset_holding_status();
		});
		cur_view->set_on_view_released([channel](View &view) {
			clicked_url = channel.url;
			clicked_is_video = false;
		});
		new_views.push_back(cur_view);
	}
	oauth_channels_list_view->swap_views(new_views);
}

static void load_oauth_subscribed_channels(void *) {
	if (!OAuth::is_authenticated()) {
		resource_lock.lock();
		if (oauth_channels_tab_view) {
			oauth_channels_tab_view->finish_pull_refresh();
		}
		resource_lock.unlock();
		return;
	}

	resource_lock.lock();
	auto new_oauth_channels = get_oauth_subscribed_channels();
	update_oauth_subscribed_channels(new_oauth_channels);

	if (oauth_channels_tab_view) {
		oauth_channels_tab_view->finish_pull_refresh();
	}

	var_need_refresh = true;
	resource_lock.unlock();
}

void Home_draw(void) {
	Hid_info key;
	Util_hid_query_key_state(&key);

	thumbnail_set_active_scene(SceneType::HOME);

	bool video_playing_bar_show = video_is_playing();
	{
		int nav_bottom = video_playing_bar_show ? 240 - VIDEO_PLAYING_BAR_HEIGHT : 240;
		overlay_set_nav(true, nav_bottom);
		CONTENT_Y_HIGH = nav_bottom - NAV_BAR_HEIGHT;
	}
	main_tab_view->update_y_range(0, CONTENT_Y_HIGH - TOP_HEIGHT);

	if (var_new_feed_videos > 0 && main_tab_view->selected_tab == 2) { // the user is looking at the new videos tab
		resource_lock.lock(); // (the loader replaces feed_latest_ids under this lock)
		std::vector<std::string> to_mark = feed_latest_ids;
		resource_lock.unlock();
		feed_seen_mark(to_mark);
		var_new_feed_videos = 0;
		var_need_refresh = true;
	}

	if (var_need_refresh || !var_eco_mode) {
		var_need_refresh = false;
		Draw_frame_ready();
		video_draw_top_screen();

		Draw_screen_ready(2, DEFAULT_BACK_COLOR);

		resource_lock.lock();
		main_view->draw();
		resource_lock.unlock();

		if (video_playing_bar_show) {
			video_draw_playing_bar();
		}
		draw_overlay_menu(CONTENT_Y_HIGH - OVERLAY_MENU_ICON_SIZE - main_tab_view->tab_selector_height);

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

	resource_lock.lock();

	if (Util_err_query_error_show_flag()) {
		Util_err_main(key);
	} else if (Util_expl_query_show_flag()) {
		Util_expl_main(key);
	} else {
		update_overlay_menu(&key);

		home_tab_view->update_y_range(0, CONTENT_Y_HIGH - TOP_HEIGHT - main_tab_view->tab_selector_height);

		TabView *channels_tab_as_tabview = dynamic_cast<TabView *>(channels_tab_view);
		if (channels_tab_as_tabview) {
			local_channels_tab_view->update_y_range(0, CONTENT_Y_HIGH - TOP_HEIGHT -
			                                               main_tab_view->tab_selector_height -
			                                               channels_tab_as_tabview->tab_selector_height);
			oauth_channels_tab_view->update_y_range(0, CONTENT_Y_HIGH - TOP_HEIGHT -
			                                               main_tab_view->tab_selector_height -
			                                               channels_tab_as_tabview->tab_selector_height);
		} else {
			local_channels_tab_view->update_y_range(0,
			                                        CONTENT_Y_HIGH - TOP_HEIGHT - main_tab_view->tab_selector_height);
		}

		FixedHeightView *channels_fixed = dynamic_cast<FixedHeightView *>(channels_tab_view);
		if (channels_fixed) {
			channels_fixed->update_y_range(0, CONTENT_Y_HIGH - TOP_HEIGHT - main_tab_view->tab_selector_height);
		}

		TabView *feed_tab_as_tabview = dynamic_cast<TabView *>(feed_tab_view);
		if (feed_tab_as_tabview) {
			if (var_disable_pull_to_refresh) {
				local_feed_videos_view->update_y_range(
				    0, CONTENT_Y_HIGH - TOP_HEIGHT - main_tab_view->tab_selector_height -
				           feed_tab_as_tabview->tab_selector_height - FEED_RELOAD_BUTTON_HEIGHT - SMALL_MARGIN);
				oauth_feed_videos_view->update_y_range(
				    0, CONTENT_Y_HIGH - TOP_HEIGHT - main_tab_view->tab_selector_height -
				           feed_tab_as_tabview->tab_selector_height - FEED_RELOAD_BUTTON_HEIGHT - SMALL_MARGIN);
			} else {
				local_feed_videos_view->update_y_range(0, CONTENT_Y_HIGH - TOP_HEIGHT -
				                                              main_tab_view->tab_selector_height -
				                                              feed_tab_as_tabview->tab_selector_height);
				oauth_feed_videos_view->update_y_range(0, CONTENT_Y_HIGH - TOP_HEIGHT -
				                                              main_tab_view->tab_selector_height -
				                                              feed_tab_as_tabview->tab_selector_height);
			}
		} else {
			VerticalListView *feed_list = dynamic_cast<VerticalListView *>(feed_tab_view);
			if (feed_list && var_disable_pull_to_refresh) {
				local_feed_videos_view->update_y_range(0, CONTENT_Y_HIGH - TOP_HEIGHT -
				                                              main_tab_view->tab_selector_height -
				                                              FEED_RELOAD_BUTTON_HEIGHT - SMALL_MARGIN);
			} else {
				local_feed_videos_view->update_y_range(0, CONTENT_Y_HIGH - TOP_HEIGHT -
				                                              main_tab_view->tab_selector_height);
			}
		}

		FixedHeightView *feed_fixed = dynamic_cast<FixedHeightView *>(feed_tab_view);
		if (feed_fixed) {
			feed_fixed->update_y_range(0, CONTENT_Y_HIGH - TOP_HEIGHT - main_tab_view->tab_selector_height);
		}
		Studio_set_height(CONTENT_Y_HIGH - TOP_HEIGHT - main_tab_view->tab_selector_height);
		main_view->update(key);
		if (clicked_url != "") {
			global_intent.next_scene = clicked_is_video ? SceneType::VIDEO_PLAYER : SceneType::CHANNEL;
			global_intent.arg = clicked_url;
			clicked_url = "";
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
	resource_lock.unlock();
}
