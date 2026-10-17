// clang-format off
#include "data_io/backup.hpp"
#include "headers.hpp"
#include <functional>
#include <sstream>
#include <regex>

#include "data_io/settings.hpp"
#include "data_io/history.hpp"
#include "util/misc_tasks.hpp"
#include "scenes/setting_menu.hpp"
#include "util/app_version.hpp"
#include "scenes/home.hpp"
#include "scenes/video_player.hpp"
#include "ui/overlay.hpp"
#include "ui/ui.hpp"
#include "ui/views/specialized/succinct_channel.hpp"
#include "youtube_parser/parser.hpp"
#include "network_decoder/thumbnail_loader.hpp"
#include "network_decoder/network_io.hpp"
#include "rapidjson_wrapper.hpp"
#include "oauth/oauth.hpp"

struct SectionTitleWithInfoView : public FixedSizeView {
	UI::FlexibleString<SectionTitleWithInfoView> title_text;
	UI::FlexibleString<SectionTitleWithInfoView> info_text;
	int popup_height = 30;
	
	SectionTitleWithInfoView(double x0, double y0, double width, double height) 
		: View(x0, y0), FixedSizeView(x0, y0, width, height) {}
	
	SectionTitleWithInfoView *set_title(std::function<std::string(const SectionTitleWithInfoView &)> title_func) {
		title_text = UI::FlexibleString<SectionTitleWithInfoView>(title_func, *this);
		return this;
	}
	
	SectionTitleWithInfoView *set_info(std::function<std::string(const SectionTitleWithInfoView &)> info_func) {
		info_text = UI::FlexibleString<SectionTitleWithInfoView>(info_func, *this);
		return this;
	}
	
	SectionTitleWithInfoView *set_popup_height(int height) {
		popup_height = height;
		return this;
	}
	
	void draw_() const override {
		double y_pos = (y0 + y1 - DEFAULT_FONT_INTERVAL) / 2 - 2;
		Draw(title_text, x0 + SMALL_MARGIN, y_pos, 0.6, 0.6, DEFAULT_TEXT_COLOR);
		
		double info_x = x0 + SMALL_MARGIN + Draw_get_width(title_text, 0.6) + 5;
		Draw("ⓘ", info_x, y_pos + (0.6 - 0.45) * DEFAULT_FONT_INTERVAL / 2, 0.45, 0.45, 0xFFFF9030);
	}
	
	void update_(Hid_info key) override {
		if (key.p_touch && key.touch_y >= y0 && key.touch_y < y1) {
			double info_x = x0 + SMALL_MARGIN + Draw_get_width(title_text, 0.6) + 5;
			if (key.touch_x >= info_x - 3 && key.touch_x < info_x + Draw_get_width("ⓘ", 0.45) + 3) {
				extern void (*show_info_popup_callback)(const std::string &, int);
				if (show_info_popup_callback) show_info_popup_callback(info_text, popup_height);
			}
		}
	}
};

namespace Settings {
	bool thread_suspend = false;
	bool already_init = false;
	bool exiting = false;
	
	bool is_3dsx = false;
	std::string path_3dsx;
	
	TextView *toast_view;
	int toast_frames_left = 0;
	
	OverlayDialogView *popup_view;
	VerticalListView *main_view;
	TabView *main_tab_view;
	
	ProgressBarView *update_progress_bar_view;
	VerticalListView *release_notes_view;
	
	// OAuth state
	bool oauth_check_timer_active = false;
	int oauth_check_frames = 0;
	int oauth_timeout_counter = 0;
	const int OAUTH_CHECK_INTERVAL_FRAMES = 300;
	const int OAUTH_TIMEOUT_SECONDS = 900;
	
	SuccinctChannelView *oauth_user_view = nullptr;
	
	Thread oauth_worker_thread;
	
	int CONTENT_Y_HIGH = 240;
	constexpr int TOP_HEIGHT = MIDDLE_FONT_INTERVAL + SMALL_MARGIN * 2;
	
	constexpr int DIALOG_WIDTH = 240;
	
	// used for updating of the app
	enum class UpdateState {
		CHECKING_UPDATES,
		FAILED_CHECKING,
		UP_TO_DATE,
		UPDATES_AVAILABLE,
		INSTALLING,
		FAILED_INSTALLING,
		SUCCEEDED_INSTALLING
	};
	UpdateState update_state = UpdateState::CHECKING_UPDATES;
	
	static std::string update_error_message;
	
	static std::string update_url_cia;
	static std::string update_url_3dsx;
	static std::string next_version_str;
	static std::string installed_version_str; // installed during this session : needs a restart, don't offer it again
	static Mutex resource_lock;
	static std::string install_progress_str;
	
	static Thread update_worker_thread;
	
	Thread settings_misc_thread;
};
using namespace Settings;

static std::string format_data_size(double bytes) {
	char buf[32];
	if (bytes >= 1e9) snprintf(buf, sizeof buf, "%.2f GB", bytes / 1e9);
	else snprintf(buf, sizeof buf, "%.1f MB", bytes / 1e6);
	return buf;
}

void (*show_info_popup_callback)(const std::string &, int) = nullptr;

void show_info_popup(const std::string &message, int height = 30) {
	if (!popup_view) return;
	
	popup_view->get_message_view()
		->set_text_lines(split_string(message, '\n'))
		->update_y_range(0, height);
	popup_view->set_buttons<std::function<std::string()>>({
		[]() { return LOCALIZED(OK); }
	}, [](OverlayDialogView &, int) {
		return true;
	});
	popup_view->set_is_visible(true);
	var_need_refresh = true;
}


std::string install_update(NetworkSessionList &session_list) {
	std::string new_error_message = "";
	auto url = is_3dsx ? update_url_3dsx : update_url_cia;
	// auto url = update_url_cia;
	
	resource_lock.lock();
	install_progress_str = LOCALIZED(ACCESSING);
	resource_lock.unlock();
	update_progress_bar_view->set_progress(0);
	update_progress_bar_view->set_is_visible(true);
	
	bool first = true;
	auto result = session_list.perform(HttpRequest::GET(url, {}).with_progress_func([&] (u64 now, u64 total) {
		if (total < 10000) return; // ignore header(?)
		if (first) {
			first = false;
			resource_lock.lock();
			install_progress_str = LOCALIZED(DOWNLOADING);
			resource_lock.unlock();
		}
		update_progress_bar_view->set_progress((double) now / total);
	}));
	
	if (result.fail) return "curl deep fail : " + result.error;
	if (result.status_code != 200) return "http returned " + std::to_string(result.status_code);
	
	resource_lock.lock();
	install_progress_str = LOCALIZED(INSTALLING);
	resource_lock.unlock();
	update_progress_bar_view->set_progress(1.0);
	
	Result libctru_result;
	if (is_3dsx) {
		char *slash_ptr = strrchr(path_3dsx.c_str(), '/');
		if (!slash_ptr) return "no slash in 3dsx path : " + path_3dsx;
		size_t slash_index = slash_ptr - path_3dsx.c_str();
		auto tmp_res = Path(path_3dsx).write_file(result.data.data(), result.data.size());
		if (tmp_res.code != 0) return "Failed to write to .3dsx file : " + tmp_res.string + " " + std::to_string(tmp_res.code);
	} else {
		Handle am_handle = 0;
		if ((libctru_result = AM_StartCiaInstall(MEDIATYPE_SD, &am_handle))) return "AM_StartCiaInstall() returned " + std::to_string(libctru_result);
		
		update_progress_bar_view->set_progress(0.0);
		
		const size_t BLOCK_SIZE = 200000;
		for (size_t i = 0; i < result.data.size(); ) {
			size_t size = std::min(BLOCK_SIZE, result.data.size() - i);
			u32 size_written;
			libctru_result = FSFILE_Write(am_handle, &size_written, i, result.data.data() + i, size, FS_WRITE_FLUSH);
			i += size_written;
			if (libctru_result) return "FSFILE_Write() returned " + std::to_string(libctru_result);
			update_progress_bar_view->set_progress((double) i / result.data.size());
		}
		update_progress_bar_view->set_progress(1.0);
		
		libctru_result = AM_FinishCiaInstall(am_handle);
		if (libctru_result) return "AM_FinishCiaInstall() returned " + std::to_string(libctru_result);
	}
	return "";
}

static void update_worker_thread_func(void *) {
	logger.info("updater", "Thread started.");
	
	static NetworkSessionList session_list;
	session_list.init();
	
	while (!exiting) {
		if (update_state == UpdateState::CHECKING_UPDATES) {
			std::string new_error_message;
			
			bool check_success = false;
			auto result = session_list.perform(HttpRequest::GET("https://api.github.com/repos/IkerPlays/Iker-s-YT/releases?per_page=10", {}));
			if (result.fail) new_error_message = "Failed accessing(deep fail) : " + result.fail;
			else if (result.status_code == 200) {
				result.data.push_back('\0');
				rapidjson::Document json_root;
				std::string error;
				RJson result_json = RJson::parse(json_root, (char *) result.data.data(), error);
				if (error != "") {
					logger.error("updater", "failed to parse json : " + error);
					new_error_message = "failed to parse json : " + error;
				} else {
					// /releases (not /releases/latest) also lists pre-releases, which is how devbuilds are published.
					//  - a newer STABLE release (not flagged "pre-release") is installed automatically
					//  - a newer devbuild is only offered (red dot + Update button) : the player decides
					auto has_suffix = [] (const std::string &str, const std::string &suffix) {
						return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
					};
					AppVersion cur_version = app_version_parse(DEF_CURRENT_APP_VER);
					bool allow_dev = cur_version.is_dev || var_show_devbuilds;
					RJson best_stable, best_dev;
					AppVersion best_stable_v, best_dev_v;
					std::string best_stable_str, best_dev_str;
					for (auto item : result_json.array_items()) {
						if (item["draft"].bool_value()) continue;
						bool has_cia = false, has_3dsx = false;
						for (auto asset : item["assets"].array_items()) {
							auto asset_url = asset["browser_download_url"].string_value();
							if (has_suffix(asset_url, ".cia")) has_cia = true;
							if (has_suffix(asset_url, ".3dsx")) has_3dsx = true;
						}
						if (!has_cia || !has_3dsx) continue;
						std::string version_str = item["tag_name"].string_value();
						AppVersion v = app_version_parse(version_str);
						if (!v.valid) {
							version_str = item["name"].string_value();
							v = app_version_parse(version_str);
						}
						if (!v.valid) continue;
						bool is_pre = item["prerelease"].bool_value(); // the GitHub flag decides what is a devbuild
						v.is_dev = is_pre;
						if (!is_pre) v.dev = 0;
						if (is_pre) {
							if (!best_dev.is_valid() || app_version_compare(v, best_dev_v) > 0) best_dev = item, best_dev_v = v, best_dev_str = version_str;
						} else {
							if (!best_stable.is_valid() || app_version_compare(v, best_stable_v) > 0) best_stable = item, best_stable_v = v, best_stable_str = version_str;
						}
					}
					RJson chosen;
					std::string best_version_str;
					bool update_available = false, auto_install = false;
					if (best_stable.is_valid() && app_version_compare(best_stable_v, cur_version) > 0) {
						chosen = best_stable, best_version_str = best_stable_str, update_available = true, auto_install = true;
					} else if (allow_dev && best_dev.is_valid() && app_version_compare(best_dev_v, cur_version) > 0) {
						chosen = best_dev, best_version_str = best_dev_str, update_available = true;
					} else if (best_stable.is_valid() && !cur_version.is_dev) {
						chosen = best_stable, best_version_str = best_stable_str; // up to date : show the notes of the latest release
					} else if (best_dev.is_valid()) {
						chosen = best_dev, best_version_str = best_dev_str;
					}
					if (update_available && best_version_str == installed_version_str) update_available = auto_install = false;
					if (chosen.is_valid()) result_json = chosen;
					update_url_3dsx = update_url_cia;
					for (auto item : result_json["assets"].array_items()) {
						auto url = item["browser_download_url"].string_value();
						if (url.size() >= 5 && url.substr(url.size() - 5, 5) == ".3dsx") update_url_3dsx = url;
						if (url.size() >= 4 && url.substr(url.size() - 4, 4) == ".cia") update_url_cia = url;
					}
					if (update_url_cia == "") new_error_message = "could not find .cia url";
					if (update_url_3dsx == "") new_error_message = "could not find .3dsx url";
					if (update_url_cia != "" && update_url_3dsx != "") {
						std::string release_notes = result_json["body"].string_value();
						{ // the notes are written in Markdown with emoji for GitHub : make them readable on the 3DS font
							std::string clean, raw = release_notes;
							for (size_t i = 0; i < raw.size(); i++) {
								unsigned char c = raw[i];
								if (c < 0x80) {
									if (c != '*' && c != '`' && c != '#' && c != '\r') clean += (char) c;
								} else {
									size_t len = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : 2;
									if (len == 2) { // accented letters are fine
										clean.append(raw, i, 2);
									} else if (c == 0xE2 && i + 2 < raw.size() && raw[i + 1] == (char) 0x80 &&
									           (raw[i + 2] == (char) 0x93 || raw[i + 2] == (char) 0x94)) {
										clean += '-'; // en/em dash
									}
									i += len - 1; // emoji and symbols the font can't draw are dropped
								}
							}
							std::istringstream clean_lines(clean);
							std::string kept, l;
							while (std::getline(clean_lines, l)) {
								if (l.rfind("<sub>", 0) == 0 || l.rfind("---", 0) == 0) continue;
								kept += l + "\n";
							}
							release_notes = kept;
						}
						std::istringstream lines(release_notes);
						std::ostringstream wrapped;
						std::string line;
						size_t line_len = 0;
						const size_t line_length = 47;
						std::vector<std::string> release_notes_lines;

						while (std::getline(lines, line)) {
  						    std::istringstream words(line);
 						    std::string word;
 						    size_t line_len = 0;

						    while (words >> word) {
  						        if (line_len + word.length() > line_length) {
  						            wrapped << "\n";
  						            release_notes_lines.push_back(wrapped.str());
  						            wrapped.str("");
 						            line_len = 0;
  						        }
  						        wrapped << word << " ";
  						        line_len += word.length() + 1;
 						    }
   						wrapped << "\n";
  						release_notes_lines.push_back(wrapped.str());
  						wrapped.str("");
						}

						release_notes = wrapped.str();

   						resource_lock.lock();
   						release_notes_view->set_is_visible(true);
   						while (!release_notes_view->views.empty()) {
 						    delete release_notes_view->views.back();
 						    release_notes_view->views.pop_back();
   						}
   						// only a short summary is shown here, the complete notes stay on the GitHub release
   						const size_t max_note_lines = 10;
   						if (release_notes_lines.size() > max_note_lines) {
   						    release_notes_lines.resize(max_note_lines);
   						    release_notes_lines.back() = "...\n";
   						}
   						for (const auto& note_line : release_notes_lines) {
   						    release_notes_view->views.push_back((new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL))->set_text(note_line));
   						}
   						resource_lock.unlock();

						if (update_available) update_state = UpdateState::UPDATES_AVAILABLE, next_version_str = best_version_str;
						else update_state = UpdateState::UP_TO_DATE;
						var_update_available = update_available;
						var_need_refresh = true;
						check_success = true;
						static bool auto_install_tried = false; // never loop : one automatic attempt per session
						if (update_available && auto_install && !auto_install_tried) {
							auto_install_tried = true;
							var_update_banner = tr_static("Updating...", "Actualizando...");
							update_state = UpdateState::INSTALLING;
						}
					}
				}
			} else new_error_message = result.status_code == 404 ? tr("http 404 : private repository or no releases", "http 404 : repo privado o sin releases") : "http returned " + std::to_string(result.status_code);
			if (!check_success) update_state = UpdateState::FAILED_CHECKING;
			if (new_error_message != "") {
				resource_lock.lock();
				update_error_message = new_error_message;
				resource_lock.unlock();
			}
		}
		if (update_state == UpdateState::INSTALLING) {
			std::string error = install_update(session_list);
			if (error == "") {
				update_state = UpdateState::SUCCEEDED_INSTALLING, var_update_available = false, var_need_refresh = true;
				var_update_banner = tr_static("Updated: restart the app", "Actualizado: reinicia la app");
				installed_version_str = next_version_str; // not offered again before the restart
				if (!is_3dsx) { // installed as a CIA : relaunch the app by itself to finish the update
					var_update_banner = tr_static("Restarting...", "Reiniciando...");
					misc_tasks_request(TASK_SAVE_SETTINGS);
					usleep(1500000); // let the settings be written
					u64 program_id = 0x000400000F1A0200ULL; // 00040000 + (unique id 0xF1A02 << 8)
					APT_GetProgramID(&program_id);
					static u8 jump_param[0x300], jump_hmac[0x20];
					if (R_SUCCEEDED(APT_PrepareToDoApplicationJump(0, program_id, MEDIATYPE_SD))) {
						APT_DoApplicationJump(jump_param, sizeof(jump_param), jump_hmac);
						usleep(5000000);
					}
					var_update_banner = tr_static("Updated: restart the app", "Actualizado: reinicia la app"); // the jump didn't happen
				}
			} else {
				var_update_banner = tr_static("Update failed", "Error al actualizar");
				resource_lock.lock();
				update_error_message = error;
				resource_lock.unlock();
				update_state = UpdateState::FAILED_INSTALLING;
			}
		}
		usleep(100000);
	}
	
	logger.info("updater", "Thread exit.");
	threadExit(0);
}

static void oauth_worker_thread_func(void *) {
	logger.info("oauth_worker", "Thread started.");
	oauth_timeout_counter = 0;
	
	while (!exiting) {
		if (OAuth::oauth_state == OAuth::OAuthState::AUTHENTICATING && oauth_check_timer_active) {
			int poll_interval = std::max(OAuth::interval, 8);
			oauth_timeout_counter += poll_interval;
			
			logger.info("oauth_worker", "Checking device flow authentication...");
			OAuth::check_device_flow();
			
			if (OAuth::oauth_state == OAuth::OAuthState::AUTHENTICATED) {
				oauth_check_timer_active = false;
				oauth_timeout_counter = 0;
				var_oauth_enabled = true;
				misc_tasks_request(TASK_SAVE_SETTINGS);

				resource_lock.lock();
				if (oauth_user_view) {
					std::string user_name = OAuth::get_user_account_name();
					std::string handle = OAuth::get_user_handle();
					std::string subscriber_count = OAuth::get_user_subscriber_count();
					std::string photo_url = OAuth::get_user_photo_url();
					
					std::vector<std::string> aux_lines;
					if (!handle.empty()) aux_lines.push_back(handle);
					if (!subscriber_count.empty()) aux_lines.push_back(subscriber_count);
					
					oauth_user_view->set_name(user_name);
					oauth_user_view->set_auxiliary_lines(aux_lines);
					oauth_user_view->set_thumbnail_url(photo_url);
					oauth_user_view->set_height(CHANNEL_ICON_HEIGHT);
					if (!photo_url.empty()) {
						oauth_user_view->thumbnail_handle = thumbnail_request(photo_url, SceneType::SETTINGS, PRIORITY_FOREGROUND, ThumbnailType::ICON);
					}
				}
				if (popup_view->is_visible) {
					std::string success_msg = std::string(LOCALIZED(OAUTH_AUTHENTICATED)) + "\n\n" + LOCALIZED(OAUTH_TOKEN_WARNING);
					popup_view->get_message_view()->set_text_lines(split_string(success_msg, '\n'))->update_y_range(0, 80);
					popup_view->set_buttons<std::function<std::string ()> >({
						[] () { return LOCALIZED(OK); }
					},
					[] (OverlayDialogView &, int button_pressed) {
						return true;
					});
				}
				resource_lock.unlock();
				var_need_refresh = true;
			} else if (OAuth::oauth_state == OAuth::OAuthState::ERROR || oauth_timeout_counter >= OAUTH_TIMEOUT_SECONDS) {
				oauth_check_timer_active = false;
				oauth_timeout_counter = 0;
				
				char log_msg[512];
				snprintf(log_msg, sizeof(log_msg), "OAuth error detected: %s", OAuth::oauth_error_message.c_str());
				logger.info("oauth_worker", log_msg);
				
				if (oauth_timeout_counter >= OAUTH_TIMEOUT_SECONDS) {
					OAuth::device_code = "";
					OAuth::user_code = "";
					OAuth::verification_url = "";
				}
				
				resource_lock.lock();
				if (popup_view->is_visible) {
					std::string error_msg;
					if (oauth_timeout_counter >= OAUTH_TIMEOUT_SECONDS) {
						error_msg = LOCALIZED(OAUTH_TIMEOUT);
					} else {
						error_msg = LOCALIZED(OAUTH_ERROR);
						if (!OAuth::oauth_error_message.empty()) {
							error_msg += "\n\n";
							error_msg += OAuth::oauth_error_message;
						}
					}
					popup_view->get_message_view()->set_text_lines(split_string(error_msg, '\n'))->update_y_range(0, 80);
					popup_view->set_buttons<std::function<std::string ()> >({
						[] () { return LOCALIZED(OK); }
					},
					[] (OverlayDialogView &, int button_pressed) {
						return true;
					});
				}
				resource_lock.unlock();
				var_need_refresh = true;
				
				usleep(5000000);
				resource_lock.lock();
				popup_view->set_is_visible(false);
				resource_lock.unlock();
				var_need_refresh = true;
			}
		}
		
		int sleep_seconds = (OAuth::oauth_state == OAuth::OAuthState::AUTHENTICATING && oauth_check_timer_active) ?
			std::max(OAuth::interval, 8) : 2;
		usleep(sleep_seconds * 1000000);
	}
	
	logger.info("oauth_worker", "Thread exit.");
	threadExit(0);
}

namespace {
	const std::vector<std::string> languages_ui = {"en", "ja", "de", "fr", "it", "es"};
	const std::vector<std::string> languages_content = {"en", "ja", "de", "fr", "it", "es"};

	int get_language_index(const std::vector<std::string>& codes, const std::string& lang) {
		for (size_t i = 0; i < codes.size(); ++i) {
			if (codes[i] == lang) return i;
		}
		return 0;
	}
}

void Sem_init(void) {
	logger.info("settings/init", "Initializing...");
	Result_with_string result;
	
	load_settings();
	load_string_resources(var_lang);
	
	popup_view = new OverlayDialogView(0, 0, 320, 240);
	popup_view->set_is_visible(false);
	toast_view = new TextView((320 - 150) / 2, 190, 150, DEFAULT_FONT_INTERVAL + SMALL_MARGIN);
	toast_view->set_is_visible(false);
	release_notes_view = new VerticalListView(0, 10, 320);
	release_notes_view->set_is_visible(false);
	
	main_tab_view = (new TabView(0, 0, 320, CONTENT_Y_HIGH - TOP_HEIGHT))
		->set_stretch_subview(true)
		->set_tab_font_size(0.4)
		->set_views({
			// Tab #1 : UI/Display
			(new ScrollView(0, 0, 320, 0))
				->set_views({
					// section: Appearance
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))
						->set_text(tr("Appearance", "Apariencia"))
						->set_font_size(0.6, DEFAULT_FONT_INTERVAL)
						->set_text_offset(0, -2)
						->set_get_text_color([]() { return (u32) COLOR_ACCENT; }),
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return tr("Home", "Inicio"); },
							(std::function<std::string ()>) []() { return tr("Library", "Biblioteca"); },
							(std::function<std::string ()>) []() { return tr("Search", "Búsqueda"); }
						}, var_start_scene)
						->set_title([](const SelectorView &) { return tr("Start screen", "Pantalla de inicio"); })
						->set_on_change([](const SelectorView &view) {
							if (var_start_scene != view.selected_button) {
								var_start_scene = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
								var_need_refresh = true;
							}
						}),
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return LOCALIZED(OFF); },
							(std::function<std::string ()>) []() { return LOCALIZED(ON); }
						}, var_auto_theme)
						->set_title([](const SelectorView &) { return tr("Automatic theme by time", "Tema automático por hora"); })
						->set_on_change([](const SelectorView &view) {
							if (var_auto_theme != view.selected_button) {
								var_auto_theme = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
								var_need_refresh = true;
							}
						}),
					// Dark theme
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return LOCALIZED(OFF); },
							(std::function<std::string ()>) []() { return LOCALIZED(ON); }
						}, var_night_mode)
						->set_title([](const SelectorView &) { return LOCALIZED(DARK_THEME); })
						->set_on_change([](const SelectorView &view) {
							if (var_night_mode != view.selected_button) {
								var_night_mode = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
							}
						}),
					// Color theme
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return "Ruby"; },
							(std::function<std::string ()>) []() { return "Ocean"; },
							(std::function<std::string ()>) []() { return "Forest"; },
							(std::function<std::string ()>) []() { return "Violet"; },
							(std::function<std::string ()>) []() { return "Amber"; },
							(std::function<std::string ()>) []() { return "AMOLED"; }
						}, var_theme)
						->set_title([](const SelectorView &) { return tr("Color theme", "Tema de color"); })
						->set_on_change([](const SelectorView &view) {
							if (var_theme != view.selected_button) {
								var_theme = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
								var_need_refresh = true;
							}
						}),
					// Global text size
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return tr("Small", "Pequeño"); },
							(std::function<std::string ()>) []() { return std::string("Normal"); },
							(std::function<std::string ()>) []() { return tr("Large", "Grande"); }
						}, var_text_scale)
						->set_title([](const SelectorView &) { return tr("Text size", "Tamaño de letra"); })
						->set_info([](const SelectorView &) { return tr("Applies to the whole app. Lists already on screen update when you reopen them.", "Se aplica a toda la app. Las listas ya abiertas se actualizan al volver a abrirlas."); })
						->set_popup_height(60)
						->set_on_change([](const SelectorView &view) {
							if (var_text_scale != view.selected_button) {
								var_text_scale = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
								var_need_refresh = true;
							}
						}),
					// Grid of thumbnails
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return LOCALIZED(OFF); },
							(std::function<std::string ()>) []() { return LOCALIZED(ON); }
						}, var_grid_view)
						->set_title([](const SelectorView &) { return tr("Grid of thumbnails", "Cuadrícula de miniaturas"); })
						->set_info([](const SelectorView &) { return tr("Video lists (search, channels, home) as a grid with two columns. Applies to lists loaded from now on.", "Listas de vídeos (búsqueda, canales, inicio) en cuadrícula de dos columnas. Se aplica a las listas que cargues desde ahora."); })
						->set_popup_height(60)
						->set_on_change([](const SelectorView &view) {
							if (var_grid_view != view.selected_button) {
								var_grid_view = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
							}
						}),
					// Large thumbnails
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return LOCALIZED(OFF); },
							(std::function<std::string ()>) []() { return LOCALIZED(ON); }
						}, var_large_thumbnails)
						->set_title([](const SelectorView &) { return tr("Large thumbnails", "Miniaturas grandes"); })
						->set_info([](const SelectorView &) { return tr("Applies to lists opened from now on.", "Se aplica a las listas que abras a partir de ahora."); })
						->set_on_change([](const SelectorView &view) {
							if (var_large_thumbnails != view.selected_button) {
								var_large_thumbnails = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
							}
						}),
					// Hide pointer option
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return LOCALIZED(OFF); },
							(std::function<std::string ()>) []() { return LOCALIZED(ON); }
						}, var_hide_pointer)
						->set_title([](const SelectorView &) { return LOCALIZED(HIDE_POINTER); })
						->set_on_change([](const SelectorView &view) {
							if (var_hide_pointer != view.selected_button) {
								var_hide_pointer = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
							}
						}),
					// full screen mode
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return LOCALIZED(OFF); },
							(std::function<std::string ()>) []() { return LOCALIZED(ON); }
						}, var_full_screen_mode)
						->set_title([](const SelectorView &) { return LOCALIZED(FULL_SCREEN_MODE); })
						->set_on_change([](const SelectorView &view) {
							if (var_full_screen_mode != view.selected_button) {
								var_full_screen_mode = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
							}
						}),
					// show full dislike/like
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return LOCALIZED(OFF); },
							(std::function<std::string ()>) []() { return LOCALIZED(ON); }
						}, var_full_dislike_like_count)
						->set_title([](const SelectorView &) { return LOCALIZED(SHOW_FULL_DISLIKE); })
						->set_on_change([](const SelectorView &view) {
							if (var_full_dislike_like_count != view.selected_button) {
								var_full_dislike_like_count = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
							}
						}),
					// section: Language
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))
						->set_text(tr("Language", "Idioma"))
						->set_font_size(0.6, DEFAULT_FONT_INTERVAL)
						->set_text_offset(0, -2)
						->set_get_text_color([]() { return (u32) COLOR_ACCENT; }),
					// UI language
					(new GridSelectorView(0, 0, 320, 70, false))
						->set_texts({
							"English",
							"日本語",
							"Deutsch",
							"Français",
							"Italiano",
							"Español"
						}, get_language_index(languages_ui, var_lang))
						->set_row_counts({3, 3})
						->set_title([](const GridSelectorView &) { return LOCALIZED(UI_LANGUAGE); })
						->set_on_change([](const GridSelectorView &view) {
							auto next_lang = languages_ui[view.selected_button];
							if (var_lang != next_lang) {
								var_lang = next_lang;
								misc_tasks_request(TASK_RELOAD_STRING_RESOURCE);
								misc_tasks_request(TASK_SAVE_SETTINGS);
							}
						}),
					// Content language
					(new GridSelectorView(0, 0, 320, 70, false))
						->set_texts({
							"English",
							"日本語",
							"Deutsch",
							"Français",
							"Italiano",
							"Español"
						}, get_language_index(languages_content, var_lang_content))
						->set_row_counts({3, 3})
						->set_title([](const GridSelectorView &) { return LOCALIZED(CONTENT_LANGUAGE); })
						->set_on_change([](const GridSelectorView &view) {
							auto next_lang = languages_content[view.selected_button];
							if (var_lang_content != next_lang) {
								var_lang_content = next_lang;
								misc_tasks_request(TASK_SAVE_SETTINGS);
								youtube_change_content_language(var_lang_content);
							}
						}),
					// section: Screen and battery
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))
						->set_text(tr("Screen and battery", "Pantalla y batería"))
						->set_font_size(0.6, DEFAULT_FONT_INTERVAL)
						->set_text_offset(0, -2)
						->set_get_text_color([]() { return (u32) COLOR_ACCENT; }),
					// LCD Brightness
					(new BarView(0, 0, 320, 40))
						->set_values_sync(15, 163, &var_lcd_brightness)
						->set_title([] (const BarView &view) { return LOCALIZED(LCD_BRIGHTNESS); })
						->set_while_holding([] (const BarView &view) { misc_tasks_request(TASK_CHANGE_BRIGHTNESS); })
						->set_on_release([] (const BarView &view) { misc_tasks_request(TASK_SAVE_SETTINGS); }),
					// Time to turn off LCD
					(new BarView(0, 0, 320, 40))
						->set_values(10, 310, var_time_to_turn_off_lcd != 0 && var_time_to_turn_off_lcd <= 309 ? var_time_to_turn_off_lcd : 310)
						->set_title([] (const BarView &view) { return LOCALIZED(TIME_TO_TURN_OFF_LCD) + " : " +
							(view.get_value() <= 309 ? std::to_string((int) view.get_value()) + " " + LOCALIZED(SECONDS) : LOCALIZED(NEVER_TURN_OFF)); })
						->set_on_release([] (const BarView &view) {
							var_time_to_turn_off_lcd = view.get_value() <= 309 ? view.get_value() : 0;
							misc_tasks_request(TASK_SAVE_SETTINGS);
						}),
					// Screen Toggle Mode
					(new SelectorView(0, 0, 320, 35, false))
						->set_texts({
							(std::function<std::string ()>) []() { return LOCALIZED(SCREEN_TOGGLE_BOTTOM); },
							(std::function<std::string ()>) []() { return LOCALIZED(SCREEN_TOGGLE_TOP); },
							(std::function<std::string ()>) []() { return LOCALIZED(SCREEN_TOGGLE_BOTH); }
						}, var_screen_off_mode)
						->set_title([](const SelectorView &) { return LOCALIZED(SCREEN_TOGGLE_MODE); })
						->set_on_change([](const SelectorView &view) {
							if (var_screen_off_mode != view.selected_button) {
								var_screen_off_mode = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
							}
						}),
					// Auto-dim
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return LOCALIZED(OFF); },
							(std::function<std::string ()>) []() { return LOCALIZED(ON); }
						}, var_auto_dim)
						->set_title([](const SelectorView &) { return tr("Dim bottom screen while playing", "Atenuar pantalla inferior al reproducir"); })
						->set_info([](const SelectorView &) { return tr("After 20 seconds without touching the console, the bottom screen gets dark while a video plays. Touch it to restore.", "Tras 20 segundos sin tocar la consola, la pantalla inferior se oscurece mientras suena un vídeo. Tócala para restaurarla."); })
						->set_popup_height(70)
						->set_on_change([](const SelectorView &view) {
							if (var_auto_dim != view.selected_button) {
								var_auto_dim = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
							}
						}),
					// audio-only playback : screens off after a few seconds (listen with the console closed)
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return LOCALIZED(OFF); },
							(std::function<std::string ()>) []() { return LOCALIZED(ON); }
						}, var_audio_screens_off)
						->set_title([](const SelectorView &) { return std::string(tr("Audio only: turn screens off", "Solo audio: apagar pantallas")); })
						->set_on_change([](const SelectorView &view) {
							if (var_audio_screens_off != view.selected_button) {
								var_audio_screens_off = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
							}
						}),
					// section: Navigation
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))
						->set_text(tr("Navigation", "Navegación"))
						->set_font_size(0.6, DEFAULT_FONT_INTERVAL)
						->set_text_offset(0, -2)
						->set_get_text_color([]() { return (u32) COLOR_ACCENT; }),
					// Disable pull to refresh
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return LOCALIZED(OFF); },
							(std::function<std::string ()>) []() { return LOCALIZED(ON); }
						}, var_disable_pull_to_refresh)
						->set_title([](const SelectorView &) { return LOCALIZED(DISABLE_PULL_TO_REFRESH); })
						->set_info([](const SelectorView &) { return LOCALIZED(INFO_DISABLE_PULL_TO_REFRESH); })
						->set_popup_height(60)
						->set_on_change([](const SelectorView &view) {
							if (var_disable_pull_to_refresh != view.selected_button) {
								var_disable_pull_to_refresh = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
								Home_rebuild_feed_tab();
								Home_rebuild_channels_tab();
								Home_update_pull_to_refresh();
							}
						}),
					// Scroll speed 0
					(new BarView(0, 0, 320, 35))
						->set_values_sync(DPAD_SCROLL_SPEED_MIN, DPAD_SCROLL_SPEED_MAX, &var_dpad_scroll_speed0)
						->set_title([] (const BarView &view) { return LOCALIZED(SCROLL_SPEED0) + " : " + double2str(var_dpad_scroll_speed0, 1); })
						->set_while_holding([] (const BarView &view) {
							var_dpad_scroll_speed1 = std::max(var_dpad_scroll_speed1, var_dpad_scroll_speed0);
						})
						->set_on_release([] (const BarView &view) { misc_tasks_request(TASK_SAVE_SETTINGS); }),
					// Scroll speed 1
					(new BarView(0, 0, 320, 35))
						->set_values_sync(DPAD_SCROLL_SPEED_MIN, DPAD_SCROLL_SPEED_MAX, &var_dpad_scroll_speed1)
						->set_title([] (const BarView &view) { return LOCALIZED(SCROLL_SPEED1) + " : " + double2str(var_dpad_scroll_speed1, 1); })
						->set_while_holding([] (const BarView &view) {
							var_dpad_scroll_speed0 = std::min(var_dpad_scroll_speed0, var_dpad_scroll_speed1);
						})
						->set_on_release([] (const BarView &view) { misc_tasks_request(TASK_SAVE_SETTINGS); }),
					// Scroll speed change threshold
					(new BarView(0, 0, 320, 40))
						->set_values_sync(DPAD_SCROLL_THRESHOLD_MIN, DPAD_SCROLL_THRESHOLD_MAX, &var_dpad_scroll_speed1_threshold)
						->set_title([] (const BarView &view) { return LOCALIZED(SCROLL_SPEED_THRESHOLD) + " : " + double2str(var_dpad_scroll_speed1_threshold, 1); })
						->set_on_release([] (const BarView &view) { misc_tasks_request(TASK_SAVE_SETTINGS); }),
					// Size of images in community posts
					(new BarView(0, 0, 320, 40))
						->set_values_sync(COMMUNITY_IMAGE_SIZE_MIN, COMMUNITY_IMAGE_SIZE_MAX, &var_community_image_size)
						->set_title([] (const BarView &view) { return LOCALIZED(COMMUNITY_POST_IMAGE_SIZE) + " : " + std::to_string(var_community_image_size) + " px"; })
						->set_on_release([] (const BarView &view) { misc_tasks_request(TASK_SAVE_SETTINGS); }),
					(new EmptyView(0, 0, 320, 10))
				}),
			// Tab #2 : Playback
			(new ScrollView(0, 0, 320, 0))
				->set_views({
					// section: Quality and subtitles
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))
						->set_text(tr("Quality and subtitles", "Calidad y subtítulos"))
						->set_font_size(0.6, DEFAULT_FONT_INTERVAL)
						->set_text_offset(0, -2)
						->set_get_text_color([]() { return (u32) COLOR_ACCENT; }),
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return tr("None", "Nada"); },
							(std::function<std::string ()>) []() { return std::string("+30 s"); },
							(std::function<std::string ()>) []() { return tr("Next in queue", "Siguiente de la cola"); },
							(std::function<std::string ()>) []() { return tr("Restart", "Reiniciar"); },
							(std::function<std::string ()>) []() { return std::string("Mini player"); },
							(std::function<std::string ()>) []() { return tr("Bookmark", "Marcador"); },
							(std::function<std::string ()>) []() { return tr("Stop after video", "Parar al acabar"); }
						}, var_button_x)
						->set_title([](const SelectorView &) { return tr("Button X in the player", "Botón X en el reproductor"); })
						->set_on_change([](const SelectorView &view) {
							if (var_button_x != view.selected_button) {
								var_button_x = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
							}
						}),
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return tr("None", "Nada"); },
							(std::function<std::string ()>) []() { return std::string("+30 s"); },
							(std::function<std::string ()>) []() { return tr("Next in queue", "Siguiente de la cola"); },
							(std::function<std::string ()>) []() { return tr("Restart", "Reiniciar"); },
							(std::function<std::string ()>) []() { return std::string("Mini player"); },
							(std::function<std::string ()>) []() { return tr("Bookmark", "Marcador"); },
							(std::function<std::string ()>) []() { return tr("Stop after video", "Parar al acabar"); }
						}, var_button_y)
						->set_title([](const SelectorView &) { return tr("Button Y in the player", "Botón Y en el reproductor"); })
						->set_on_change([](const SelectorView &view) {
							if (var_button_y != view.selected_button) {
								var_button_y = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
							}
						}),
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return LOCALIZED(OFF); },
							(std::function<std::string ()>) []() { return LOCALIZED(ON); }
						}, var_left_handed)
						->set_title([](const SelectorView &) { return tr("Left-handed mini player", "Mini reproductor para zurdos"); })
						->set_on_change([](const SelectorView &view) {
							if (var_left_handed != view.selected_button) {
								var_left_handed = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
							}
						}),
					// Automatic quality
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return LOCALIZED(OFF); },
							(std::function<std::string ()>) []() { return LOCALIZED(ON); }
						}, var_auto_quality)
						->set_title([](const SelectorView &) { return tr("Automatic quality", "Calidad automática"); })
						->set_info([](const SelectorView &) { return tr("Lowers the quality of the next videos if playback keeps stalling, raises it again when the connection is stable.", "Baja la calidad de los siguientes vídeos si la reproducción se atasca y la sube de nuevo cuando la conexión es estable."); })
						->set_popup_height(70)
						->set_on_change([](const SelectorView &view) {
							if (var_auto_quality != view.selected_button) {
								var_auto_quality = view.selected_button;
								var_auto_quality_cap = 0;
								misc_tasks_request(TASK_SAVE_SETTINGS);
							}
						}),
					// Subtitles
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return tr("Small", "Pequeño"); },
							(std::function<std::string ()>) []() { return "Normal"; },
							(std::function<std::string ()>) []() { return tr("Large", "Grande"); }
						}, var_caption_size)
						->set_title([](const SelectorView &) { return tr("Subtitle size", "Tamaño de subtítulos"); })
						->set_on_change([](const SelectorView &view) {
							if (var_caption_size != view.selected_button) {
								var_caption_size = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
								var_need_refresh = true;
							}
						}),
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return tr("White", "Blanco"); },
							(std::function<std::string ()>) []() { return tr("Yellow", "Amarillo"); },
							(std::function<std::string ()>) []() { return tr("Cyan", "Cian"); },
							(std::function<std::string ()>) []() { return tr("Green", "Verde"); }
						}, var_caption_color)
						->set_title([](const SelectorView &) { return tr("Subtitle color", "Color de subtítulos"); })
						->set_on_change([](const SelectorView &view) {
							if (var_caption_color != view.selected_button) {
								var_caption_color = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
								var_need_refresh = true;
							}
						}),
					// section: Player
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))
						->set_text(tr("Player", "Reproductor"))
						->set_font_size(0.6, DEFAULT_FONT_INTERVAL)
						->set_text_offset(0, -2)
						->set_get_text_color([]() { return (u32) COLOR_ACCENT; }),
					// Autoplay
					(new SelectorView(0, 0, 320, 35, false))
						->set_texts({
							(std::function<std::string ()>) []() { return LOCALIZED(OFF); },
							(std::function<std::string ()>) []() { return LOCALIZED(ONLY_IN_PLAYLIST); },
							(std::function<std::string ()>) []() { return LOCALIZED(ON); }
						}, var_autoplay_level)
						->set_title([](const SelectorView &) { return LOCALIZED(AUTOPLAY); })
						->set_on_change([](const SelectorView &view) {
							if (var_autoplay_level != view.selected_button) {
								var_autoplay_level = view.selected_button;
								misc_tasks_request(TASK_CHANGE_BRIGHTNESS);
							}
						}),

					// Forward buffer ratio
					(new BarView(0, 0, 320, 40))
						->set_values_sync(0.1, 1.0, &var_forward_buffer_ratio)
						->set_title([] (const BarView &view) {
							char ratio_str[16];
							snprintf(ratio_str, 16, "%.2f", var_forward_buffer_ratio);
							return LOCALIZED(FORWARD_BUFFER_RATIO) + " : " + ratio_str;
						})
						->set_on_release([] (const BarView &view) { misc_tasks_request(TASK_SAVE_SETTINGS); }),

					// Audio/video sync offset
					(new BarView(0, 0, 320, 40))
						->set_values_sync(-300, 300, &var_av_sync_ms)
						->set_title([] (const BarView &view) {
							char ms_str[24];
							snprintf(ms_str, 24, "%+d ms", (int) var_av_sync_ms);
							return tr("Audio/video sync: ", "Sincronía audio/vídeo: ") + ms_str;
						})
						->set_on_release([] (const BarView &view) { misc_tasks_request(TASK_SAVE_SETTINGS); })
				}),
			// Tab #3 : Data
			(new ScrollView(0, 0, 320, 0))
				->set_views({
					// section: Network
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))
						->set_text(tr("Network", "Red"))
						->set_font_size(0.6, DEFAULT_FONT_INTERVAL)
						->set_text_offset(0, -2)
						->set_get_text_color([]() { return (u32) COLOR_ACCENT; }),
					// data usage
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN))
						->set_text((std::function<std::string ()>) [] () -> std::string {
							return tr("Data downloaded (session): ", "Datos descargados (sesión): ") + format_data_size((double) var_data_session_bytes);
						}),
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN))
						->set_text((std::function<std::string ()>) [] () -> std::string {
							return tr("Data downloaded (total): ", "Datos descargados (total): ") + format_data_size(var_data_total_bytes + (double) var_data_session_bytes);
						}),
					// network status and last errors
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN))
						->set_text((std::function<std::string ()>) [] () -> std::string {
							std::string state = tr("Wi-Fi: ", "Wi-Fi: ");
							if (var_wifi_state != 2) return state + tr("not connected", "sin conexión");
							int level = *(u8 *)0x1FF81066;
							return state + tr("connected, signal ", "conectado, señal ") + std::to_string(level) + "/3";
						}),
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL))
						->set_font_size(0.45, DEFAULT_FONT_INTERVAL)
						->set_text((std::function<std::string ()>) [] () -> std::string { return tr("Last warnings and errors:", "Últimos avisos y errores:"); }),
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL))
						->set_font_size(0.4, DEFAULT_FONT_INTERVAL)
						->set_text((std::function<std::string ()>) [] () -> std::string {
							auto lines = logger.recent(LogLevel::WARNING, 3);
							if (0 >= (int) lines.size()) return "";
							auto cut = truncate_str(lines[0], 310, 1, 0.4, 0.4); return cut.empty() ? std::string() : cut[0];
						}),
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL))
						->set_font_size(0.4, DEFAULT_FONT_INTERVAL)
						->set_text((std::function<std::string ()>) [] () -> std::string {
							auto lines = logger.recent(LogLevel::WARNING, 3);
							if (1 >= (int) lines.size()) return "";
							auto cut = truncate_str(lines[1], 310, 1, 0.4, 0.4); return cut.empty() ? std::string() : cut[0];
						}),
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL))
						->set_font_size(0.4, DEFAULT_FONT_INTERVAL)
						->set_text((std::function<std::string ()>) [] () -> std::string {
							auto lines = logger.recent(LogLevel::WARNING, 3);
							if (2 >= (int) lines.size()) return "";
							auto cut = truncate_str(lines[2], 310, 1, 0.4, 0.4); return cut.empty() ? std::string() : cut[0];
						}),
					(new TextView(10, 0, 220, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))
						->set_text((std::function<std::string ()>) [] () { return tr("Save full log to the SD card", "Guardar el registro completo en la SD"); })
						->set_x_alignment(TextView::XAlign::CENTER)
						->set_text_offset(0, -2)
						->set_get_background_color(View::STANDARD_BACKGROUND)
						->set_on_view_released([] (View &) {
							bool ok = logger.save_full_log();
							toast_view
								->set_text((std::function<std::string ()>) [ok] () { return ok ? std::string("sdmc:/3ds/IkersTube/log_full.txt") : tr("Could not write the log", "No se pudo escribir el registro"); })
								->set_x_alignment(TextView::XAlign::CENTER)
								->set_text_offset(0, -1)
								->set_get_text_color([] () { return (u32) -1; })
								->update_y_range(CONTENT_Y_HIGH - main_tab_view->tab_selector_height - 20, CONTENT_Y_HIGH - main_tab_view->tab_selector_height - 5)
								->set_get_background_color([] (const View &) { return 0x50000000; })
								->set_is_visible(true);
							toast_frames_left = 150;
							var_need_refresh = true;
						}),
					(new TextView(10, 0, 220, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))
						->set_text((std::function<std::string ()>) [] () { return tr("Export backup to the SD card", "Exportar copia a la SD"); })
						->set_x_alignment(TextView::XAlign::CENTER)
						->set_text_offset(0, -2)
						->set_get_background_color(View::STANDARD_BACKGROUND)
						->set_on_view_released([] (View &) {
							int count = backup_export();
							std::string message = count < 0 ? tr("SD card error or no backup found", "Error de la SD o no hay copia") : std::to_string(count) + " " + tr("files saved (sdmc:/3ds/IkersTube/backup)", "archivos guardados (sdmc:/3ds/IkersTube/backup)");
							toast_view
								->set_text((std::function<std::string ()>) [message] () { return message; })
								->set_x_alignment(TextView::XAlign::CENTER)
								->set_text_offset(0, -1)
								->set_get_text_color([] () { return (u32) -1; })
								->update_y_range(CONTENT_Y_HIGH - main_tab_view->tab_selector_height - 20, CONTENT_Y_HIGH - main_tab_view->tab_selector_height - 5)
								->set_get_background_color([] (const View &) { return 0x50000000; })
								->set_is_visible(true);
							toast_frames_left = 180;
							var_need_refresh = true;
						}),
					(new TextView(10, 0, 220, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))
						->set_text((std::function<std::string ()>) [] () { return tr("Import backup (restart the app after)", "Importar copia (reinicia la app después)"); })
						->set_x_alignment(TextView::XAlign::CENTER)
						->set_text_offset(0, -2)
						->set_get_background_color(View::STANDARD_BACKGROUND)
						->set_on_view_released([] (View &) {
							int count = backup_import();
							std::string message = count < 0 ? tr("SD card error or no backup found", "Error de la SD o no hay copia") : std::to_string(count) + " " + tr("files restored - restart the app", "archivos restaurados - reinicia la app");
							toast_view
								->set_text((std::function<std::string ()>) [message] () { return message; })
								->set_x_alignment(TextView::XAlign::CENTER)
								->set_text_offset(0, -1)
								->set_get_text_color([] () { return (u32) -1; })
								->update_y_range(CONTENT_Y_HIGH - main_tab_view->tab_selector_height - 20, CONTENT_Y_HIGH - main_tab_view->tab_selector_height - 5)
								->set_get_background_color([] (const View &) { return 0x50000000; })
								->set_is_visible(true);
							toast_frames_left = 180;
							var_need_refresh = true;
						}),
					// Data saver
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return LOCALIZED(OFF); },
							(std::function<std::string ()>) []() { return LOCALIZED(ON); }
						}, var_data_saver)
						->set_title([](const SelectorView &) { return tr("Data saver", "Ahorro de datos"); })
						->set_info([](const SelectorView &) { return tr("Limits videos to 240p and does not download thumbnails.", "Limita los vídeos a 240p y no descarga miniaturas."); })
						->set_popup_height(50)
						->set_on_change([](const SelectorView &view) {
							if (var_data_saver != view.selected_button) {
								var_data_saver = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
							}
						}),
					// Data limit
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return LOCALIZED(OFF); },
							(std::function<std::string ()>) []() { return std::string("100 MB"); },
							(std::function<std::string ()>) []() { return std::string("250 MB"); },
							(std::function<std::string ()>) []() { return std::string("500 MB"); },
							(std::function<std::string ()>) []() { return std::string("1 GB"); },
							(std::function<std::string ()>) []() { return std::string("2 GB"); }
						}, var_data_limit_index)
						->set_title([](const SelectorView &) { return tr("Data limit per session", "Límite de datos por sesión"); })
						->set_info([](const SelectorView &) { return tr("Shows a warning (red dot in Settings) when this session downloads more than the limit.", "Avisa (punto rojo en Ajustes) cuando esta sesión descarga más que el límite."); })
						->set_popup_height(60)
						->set_on_change([](const SelectorView &view) {
							if (var_data_limit_index != view.selected_button) {
								var_data_limit_index = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
							}
						}),
					// statistics
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))
						->set_text(tr("Statistics", "Estadísticas"))
						->set_font_size(0.6, DEFAULT_FONT_INTERVAL)
						->set_text_offset(0, -2)
						->set_get_text_color([]() { return (u32) COLOR_ACCENT; }),
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN))
						->set_text((std::function<std::string ()>) [] () -> std::string {
							int total = (int) var_total_play_seconds;
							return tr("Time playing: ", "Tiempo reproduciendo: ") + std::to_string(total / 3600) + " h " + std::to_string(total / 60 % 60) + " min";
						}),
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN))
						->set_text((std::function<std::string ()>) [] () -> std::string {
							return tr("Videos in the history: ", "Vídeos en el historial: ") + std::to_string(get_valid_watch_history().size());
						}),
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN))
						->set_text((std::function<std::string ()>) [] () -> std::string {
							std::map<std::string, int> counts;
							for (auto &v : get_valid_watch_history()) if (v.author_name != "") counts[v.author_name] += std::max(1, v.my_view_count);
							std::string best; int best_count = 0;
							for (auto &c : counts) if (c.second > best_count) best = c.first, best_count = c.second;
							if (best == "") return tr("Most watched channel: -", "Canal más visto: -");
							return truncate_str(tr("Most watched channel: ", "Canal más visto: ") + best, 310, 1, 0.5, 0.5).at(0);
						}),
					// section: History
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))
						->set_text(tr("History", "Historial"))
						->set_font_size(0.6, DEFAULT_FONT_INTERVAL)
						->set_text_offset(0, -2)
						->set_get_text_color([]() { return (u32) COLOR_ACCENT; }),
					// History recording
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return LOCALIZED(DISABLED); },
							(std::function<std::string ()>) []() { return LOCALIZED(ENABLED); }
						}, var_history_enabled)
						->set_title([](const SelectorView &) { return LOCALIZED(WATCH_HISTORY); })
						->set_on_change([](const SelectorView &view) {
							if (var_history_enabled != view.selected_button) {
								var_history_enabled = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
							}
						}),
					(new EmptyView(0, 0, 320, 10)),
					// Erase history
					(new TextView(10, 0, 120, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))
						->set_text((std::function<std::string ()>) [] () { return LOCALIZED(REMOVE_ALL_HISTORY); })
						->set_x_alignment(TextView::XAlign::CENTER)
						->set_text_offset(0, -2)
						->set_get_background_color([] (const View &view) {
							int red = std::min<int>(0xFF, 0xD0 + 0x30 * view.touch_darkness);
							int other = 0x30 * (1 - view.touch_darkness);
							return 0xFF000000 | other << 8 | other << 16 | red;
						})
						->set_on_view_released([] (View &view) {
							popup_view->get_message_view()->set_text_lines(split_string(LOCALIZED(REMOVE_ALL_HISTORY_CONFIRM), '\n'))->update_y_range(0, 30);
							popup_view->set_buttons<std::function<std::string ()> >({
								[] () { return LOCALIZED(CANCEL); },
								[] () { return LOCALIZED(OK); }
							},
							[] (OverlayDialogView &, int button_pressed) {
								if (button_pressed == 1) {
									history_erase_all();
									misc_tasks_request(TASK_SAVE_HISTORY);
									
									float tabbed_content_y_high = CONTENT_Y_HIGH - main_tab_view->tab_selector_height;
									toast_view
										->set_text((std::function<std::string ()>) [] () { return LOCALIZED(ALL_HISTORY_REMOVED); })
										->set_x_alignment(TextView::XAlign::CENTER)
										->set_text_offset(0, -1)
										->set_get_text_color([] () { return (u32) -1; })
										->update_y_range(tabbed_content_y_high - 20, tabbed_content_y_high - 5)
										->set_get_background_color([] (const View &) { return 0x50000000; })
										->set_is_visible(true);
									toast_frames_left = 120;
								}
								return true; // close the dialog
							});
							popup_view->set_is_visible(true);
							var_need_refresh = true;
						}),
					(new EmptyView(0, 0, 320, 10))
				}),
			// Tab #4 : Update
			(new ScrollView(0, 0, 320, 0))
				->set_views({
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))
						->set_text((std::function<std::string ()>) [] () { return LOCALIZED(UPDATE) + " - " + DEF_CURRENT_APP_VER; })
						->set_font_size(0.6, DEFAULT_FONT_INTERVAL)
						->set_text_offset(0, -2),
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN))
						->set_text((std::function<std::string ()>) [] () -> std::string {
							if (update_state == UpdateState::CHECKING_UPDATES) return LOCALIZED(CHECKING_FOR_UPDATES);
							if (update_state == UpdateState::FAILED_CHECKING) return LOCALIZED(FAILED_CHECKING_UPDATES) + " : " + update_error_message;
							if (update_state == UpdateState::UP_TO_DATE) return LOCALIZED(APP_UP_TO_DATE);
							if (update_state == UpdateState::UPDATES_AVAILABLE) return LOCALIZED(UPDATES_AVAILABLE) + " : " + next_version_str;
							if (update_state == UpdateState::INSTALLING) return install_progress_str;
							if (update_state == UpdateState::FAILED_INSTALLING) return LOCALIZED(UPDATE_FAIL) + " : " + update_error_message;
							if (update_state == UpdateState::SUCCEEDED_INSTALLING) return LOCALIZED(UPDATE_SUCCESS);
							return "";
						}),
					(new EmptyView(0, 0, 320, SMALL_MARGIN)),
					(update_progress_bar_view = (new ProgressBarView(10, 0, 300, 5)))
						->set_get_color([] () { return DEF_DRAW_BLUE; })
						->set_is_visible(false),
					(new EmptyView(0, 0, 320, SMALL_MARGIN)),
					(new TextView(10, 0, 100, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))
						->set_text((std::function<std::string ()>) [] () -> std::string {
							if (update_state == UpdateState::FAILED_CHECKING || update_state == UpdateState::UP_TO_DATE) return LOCALIZED(RETRY);
							if (update_state == UpdateState::UPDATES_AVAILABLE) return LOCALIZED(UPDATE);
							if (update_state == UpdateState::FAILED_INSTALLING) return LOCALIZED(RETRY);
							return "";
						})
						->set_x_alignment(TextView::XAlign::CENTER)
						->set_text_offset(0, -1)
						->set_on_view_released([] (const View &) {
							if (update_state == UpdateState::FAILED_CHECKING || update_state == UpdateState::UP_TO_DATE) {
								update_state = UpdateState::CHECKING_UPDATES;
								var_need_refresh = true;
							}
							if (update_state == UpdateState::UPDATES_AVAILABLE || update_state == UpdateState::FAILED_INSTALLING) {
								std::vector<std::string> confirm_lines;
								if (is_3dsx) confirm_lines = truncate_str(std::regex_replace(LOCALIZED(OVERWRITE_3DSX_CONFIRM), std::regex("%0"), path_3dsx), DIALOG_WIDTH, 3, 0.5, 0.5);
								else confirm_lines = {LOCALIZED(INSTALL_CIA_CONFIRM)};
								
								popup_view->get_message_view()->set_text_lines(confirm_lines)->update_y_range(0, 45);
								popup_view->set_buttons<std::function<std::string ()> >({
									[] () { return LOCALIZED(CANCEL); },
									[] () { return LOCALIZED(OK); }
								},
								[] (OverlayDialogView &, int button_pressed) {
									if (button_pressed == 1) update_state = UpdateState::INSTALLING;
									return true; // close the dialog
								});
								popup_view->set_is_visible(true);
								var_need_refresh = true;
							}
						})
						->set_get_background_color([] (const View &view) -> u32 {
							if (update_state == UpdateState::FAILED_CHECKING || update_state == UpdateState::UPDATES_AVAILABLE || update_state == UpdateState::UP_TO_DATE) {
								int blue = std::min<int>(0xFF, 0xB0 + 0x30 * view.touch_darkness);
								int other = 0x50 + 0x20 * (1 - view.touch_darkness);
								return 0xFF000000 | blue << 16 | other << 8 | other;
							}
							return DEFAULT_BACK_COLOR;
						}),
					(new RuleView(0, 6, 320, 1))
					    ->set_get_background_color([] (const View &) {return DEFAULT_BACK_COLOR; }),
					// Offer devbuilds (test builds) : stable releases are always installed automatically
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return LOCALIZED(OFF); },
							(std::function<std::string ()>) []() { return LOCALIZED(ON); }
						}, var_show_devbuilds)
						->set_title([](const SelectorView &) { return std::string(tr("Offer devbuilds (testing)", "Ofrecer devbuilds (pruebas)")); })
						->set_on_change([](const SelectorView &view) {
							if (var_show_devbuilds != view.selected_button) {
								var_show_devbuilds = view.selected_button;
								update_state = UpdateState::CHECKING_UPDATES; // check again with the new setting
								misc_tasks_request(TASK_SAVE_SETTINGS);
							}
						}),
					    release_notes_view,
					(new EmptyView(0, 0, 320, DEFAULT_FONT_INTERVAL))
				}),
			// Tab #5 : Advanced
			(new ScrollView(0, 0, 320, 0))
				->set_views({
					// Eco mode
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return LOCALIZED(OFF); },
							(std::function<std::string ()>) []() { return LOCALIZED(ON); }
						}, var_eco_mode)
						->set_title([](const SelectorView &) { return LOCALIZED(ECO_MODE); })
						->set_info([](const SelectorView &) { return LOCALIZED(INFO_ECO_MODE); })
						->set_popup_height(60)
						->set_on_change([](const SelectorView &view) {
							if (var_eco_mode != view.selected_button) {
								var_eco_mode = view.selected_button;
								misc_tasks_request(TASK_SAVE_SETTINGS);
							}
						}),
					// Use linear filter
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return LOCALIZED(OFF); },
							(std::function<std::string ()>) []() { return LOCALIZED(ON); }
						}, var_video_linear_filter)
						->set_title([](const SelectorView &) { return LOCALIZED(LINEAR_FILTER); })
						->set_on_change([](const SelectorView &view) {
							if (var_video_linear_filter != view.selected_button) {
								var_video_linear_filter = view.selected_button;
								video_set_linear_filter_enabled(var_video_linear_filter);
								misc_tasks_request(TASK_SAVE_SETTINGS);
							}
						}),
					(new EmptyView(0, 0, 320, 10)),
					// Debug info in the control tab
					(new SelectorView(0, 0, 320, 35, true))
						->set_texts({
							(std::function<std::string ()>) []() { return LOCALIZED(OFF); },
							(std::function<std::string ()>) []() { return LOCALIZED(ON); }
						}, var_video_show_debug_info)
						->set_title([](const SelectorView &view) { return LOCALIZED(VIDEO_SHOW_DEBUG_INFO); })
						->set_on_change([](const SelectorView &view) {
							if (var_video_show_debug_info != view.selected_button) {
								var_video_show_debug_info = view.selected_button;
								video_set_show_debug_info(var_video_show_debug_info);
								misc_tasks_request(TASK_SAVE_SETTINGS);
							}
						}),
					(new EmptyView(0, 0, 320, 10)),
                    // Select app data to use
                    (new SelectorView(0, 0, 320, 35, false))
                        ->set_texts({
                            (std::function<std::string ()>) []() { return "Android"; },
                            (std::function<std::string ()>) []() { return "Android VR"; },
                            (std::function<std::string ()>) []() { return "visionOS"; }
                        }, var_player_response)
                        ->set_title([](const SelectorView &) { return LOCALIZED(PLAYER_RESPONSE); })
                        ->set_info([](const SelectorView &) { return LOCALIZED(INFO_PLAYER_RESPONSE); })
                        ->set_popup_height(50)
                        ->set_on_change([](const SelectorView &view) {
                            if (var_player_response != view.selected_button) {
                                var_player_response = view.selected_button;
                                if (view.selected_button == 0) {
                                    var_player_response = 0; // Android
                                } else if (view.selected_button == 1) {
                                    var_player_response = 1; // Android VR
                                } else if (view.selected_button == 2) {
                                    var_player_response = 2; // visionOS
                                }
                                misc_tasks_request(TASK_SAVE_SETTINGS);
                            }
                        }),
                    (new EmptyView(0, 0, 320, 10)),
					// OAuth Settings Section
					(new SectionTitleWithInfoView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))
						->set_title([] (const SectionTitleWithInfoView &) { return LOCALIZED(OAUTH); })
						->set_info([] (const SectionTitleWithInfoView &) { return LOCALIZED(INFO_OAUTH); })
						->set_popup_height(85),
					(new TextView(0, 0, 320, DEFAULT_FONT_INTERVAL + SMALL_MARGIN))
						->set_text([] () -> std::string {
							const std::string prefix = LOCALIZED(OAUTH_STATUS) + ": ";
							switch (OAuth::oauth_state) {
								case OAuth::OAuthState::NOT_AUTHENTICATED: return prefix + LOCALIZED(OAUTH_NOT_AUTHENTICATED);
								case OAuth::OAuthState::AUTHENTICATED: return prefix + LOCALIZED(OAUTH_AUTHENTICATED);
								case OAuth::OAuthState::AUTHENTICATING: return prefix + LOCALIZED(OAUTH_AUTHENTICATING);
								case OAuth::OAuthState::ERROR: return prefix + LOCALIZED(OAUTH_ERROR);
								default: return "";
							}
						}),
					(new EmptyView(0, 0, 320, SMALL_MARGIN)),
					(oauth_user_view = (new SuccinctChannelView(0, 0, 320, 0))
						->set_name("")
						->set_auxiliary_lines({})
						->set_thumbnail_url("")),
					(new EmptyView(0, 0, 320, SMALL_MARGIN)),
					(new TextView(10, 0, 120, DEFAULT_FONT_INTERVAL + SMALL_MARGIN * 2))
						->set_text([] () {
							if (OAuth::oauth_state == OAuth::OAuthState::AUTHENTICATED) {
								return LOCALIZED(OAUTH_LOGOUT);
							} else if (OAuth::oauth_state == OAuth::OAuthState::ERROR) {
								return LOCALIZED(RETRY);
							} else {
								return LOCALIZED(OAUTH_LOGIN);
							}
						})
						->set_x_alignment(TextView::XAlign::CENTER)
						->set_text_offset(0, -2)
						->set_get_background_color([] (const View &view) {
							if (OAuth::oauth_state == OAuth::OAuthState::AUTHENTICATED) {
								int red = std::min(0xFF, (int)(0xD0 + 0x30 * view.touch_darkness));
								int other = (int)(0x30 * (1 - view.touch_darkness));
								return 0xFF000000 | other << 8 | other << 16 | red;
							}
							int blue = std::min(0xFF, (int)(0xB0 + 0x30 * view.touch_darkness));
							int other = (int)(0x50 + 0x20 * (1 - view.touch_darkness));
							return 0xFF000000 | blue << 16 | other << 8 | other;
						})
						->set_on_view_released([] (View &) {
							if (OAuth::oauth_state == OAuth::OAuthState::AUTHENTICATED) {
								OAuth::revoke_tokens();
								var_oauth_enabled = false;
								misc_tasks_request(TASK_SAVE_SETTINGS);

								if (oauth_user_view) {
									oauth_user_view->set_name("");
									oauth_user_view->set_auxiliary_lines({});
									oauth_user_view->set_thumbnail_url("");
									oauth_user_view->thumbnail_handle = -1;
									oauth_user_view->set_height(0);
								}
							} else if (OAuth::oauth_state == OAuth::OAuthState::ERROR) {
								OAuth::refresh_access_token();
							} else if (OAuth::oauth_state == OAuth::OAuthState::NOT_AUTHENTICATED) {
								OAuth::start_device_flow();
								if (OAuth::oauth_state == OAuth::OAuthState::AUTHENTICATING) {
									auto cancel_auth = [] () {
										oauth_check_timer_active = false;
										oauth_timeout_counter = 0;
										OAuth::revoke_tokens();
									};
									
									std::string message = LOCALIZED(OAUTH_LOGIN_INSTRUCTION) + "\n\n" +
										std::regex_replace(LOCALIZED(OAUTH_USER_CODE), std::regex("%0"), OAuth::user_code) + "\n" +
										LOCALIZED(OAUTH_VERIFICATION_URL) + "\n\n" + LOCALIZED(OAUTH_WAITING);
									
									popup_view->get_message_view()->set_text_lines(split_string(message, '\n'))->update_y_range(0, 120);
									popup_view->set_buttons<std::function<std::string ()> >({
										[] () { return LOCALIZED(CANCEL); }
									}, [cancel_auth] (OverlayDialogView &, int) {
										cancel_auth();
										return true;
									});
									popup_view->set_on_cancel([cancel_auth](OverlayView &view) {
										cancel_auth();
										view.set_is_visible(false);
										var_need_refresh = true;
									});
									popup_view->set_is_visible(true);
									oauth_check_timer_active = true;
									oauth_timeout_counter = oauth_check_frames = 0;
								}
							}
							var_need_refresh = true;
						}),
					(new EmptyView(0, 0, 320, 10)),
				})
		}, 0)
		->set_tab_texts<std::function<std::string ()> >({
			[] () { return LOCALIZED(SETTINGS_DISPLAY_UI); },
			[] () { return LOCALIZED(PLAYBACK); },
			[] () { return LOCALIZED(SETTINGS_DATA); },
			[] () { return LOCALIZED(UPDATE); },
			[] () { return LOCALIZED(SETTINGS_ADVANCED); }
		});
	main_view = (new VerticalListView(0, 0, 320))
		->set_views({
			// 'Settings'
			(new TextView(0, 0, 320, MIDDLE_FONT_INTERVAL))
				->set_text((std::function<std::string ()>) [] () { return LOCALIZED(SETTINGS); })
				->set_font_size(MIDDLE_FONT_SIZE, MIDDLE_FONT_INTERVAL)
				->set_get_background_color([] (const View &) { return DEFAULT_BACK_COLOR; }),
			// ---------------------
			(new RuleView(0, 0, 320, SMALL_MARGIN * 2))
				->set_get_background_color([] (const View &) { return DEFAULT_BACK_COLOR; }),
			main_tab_view
		})
		->set_draw_order({2, 1, 0});
		
	
	is_3dsx = envIsHomebrew();
	if (is_3dsx) {
		const char *arglist = envGetSystemArgList();
		int argc = *(u32 *) arglist;
		if (argc) {
			arglist += 4;
			path_3dsx = arglist;
			if (path_3dsx.substr(0, 6) == "sdmc:/") path_3dsx = path_3dsx.substr(5, path_3dsx.size() - 5);
		}
	}
	
	update_worker_thread = threadCreate(update_worker_thread_func, (void*)(""), DEF_STACKSIZE, DEF_THREAD_PRIORITY_LOW, 1, false);
	oauth_worker_thread = threadCreate(oauth_worker_thread_func, (void*)(""), DEF_STACKSIZE, DEF_THREAD_PRIORITY_LOW, 1, false);
	
	// Initialize OAuth
	OAuth::init();
	if (OAuth::is_authenticated()) {
		var_oauth_enabled = true;

		std::string user_name = OAuth::get_user_account_name();
		std::string handle = OAuth::get_user_handle();
		std::string subscriber_count = OAuth::get_user_subscriber_count();
		std::string photo_url = OAuth::get_user_photo_url();
		
		if (oauth_user_view && !user_name.empty()) {
			std::vector<std::string> aux_lines;
			if (!handle.empty()) aux_lines.push_back(handle);
			if (!subscriber_count.empty()) aux_lines.push_back(subscriber_count);
			
			oauth_user_view->set_name(user_name);
			oauth_user_view->set_auxiliary_lines(aux_lines);
			oauth_user_view->set_thumbnail_url(photo_url);
			oauth_user_view->set_height(CHANNEL_ICON_HEIGHT);
			if (!photo_url.empty()) {
				oauth_user_view->thumbnail_handle = thumbnail_request(photo_url, SceneType::SETTINGS, PRIORITY_FOREGROUND, ThumbnailType::ICON);
			}
		}
	}
	
	show_info_popup_callback = show_info_popup;
	
	Sem_resume("");
	already_init = true;
}
void Sem_exit(void) {
	already_init = false;
	thread_suspend = false;
	exiting = true;
	
	save_settings();
	OAuth::exit();
	
	u64 time_out = 10000000000;
	logger.info(DEF_MENU_EXIT_STR, "threadJoin()...", threadJoin(update_worker_thread, time_out));
	threadFree(update_worker_thread);
	logger.info(DEF_MENU_EXIT_STR, "oauth threadJoin()...", threadJoin(oauth_worker_thread, time_out));
	threadFree(oauth_worker_thread);
	
	main_view->recursive_delete_subviews();
	delete main_view;
	main_view = NULL;
	main_tab_view = NULL;
	
	logger.info("settings/exit", "Exited.");
}
void Sem_suspend(void) {
	thread_suspend = true;
}
void Sem_resume(std::string arg) {
	overlay_menu_on_resume();
	thread_suspend = false;
	var_need_refresh = true;
}


void Sem_draw(void)
{
	Hid_info key;
	Util_hid_query_key_state(&key);
	
	thumbnail_set_active_scene(SceneType::SETTINGS);
	
	bool video_playing_bar_show = video_is_playing();
	CONTENT_Y_HIGH = 240;
	if (video_playing_bar_show) CONTENT_Y_HIGH -= VIDEO_PLAYING_BAR_HEIGHT;
	main_tab_view->update_y_range(0, CONTENT_Y_HIGH - TOP_HEIGHT);
	
	if(var_need_refresh || !var_eco_mode)
	{
		var_need_refresh = false;
		Draw_frame_ready();
		video_draw_top_screen();
		
		Draw_screen_ready(2, DEFAULT_BACK_COLOR);
		
		resource_lock.lock();
		main_view->draw();
		popup_view->draw();
		toast_view->draw();
		resource_lock.unlock();
		
		if (video_playing_bar_show) video_draw_playing_bar();
		draw_overlay_menu(CONTENT_Y_HIGH - main_tab_view->tab_selector_height - OVERLAY_MENU_ICON_SIZE);
		
		if(Util_expl_query_show_flag())
			Util_expl_draw();

		if(Util_err_query_error_show_flag())
			Util_err_draw();

		Draw_touch_pos();

		Draw_apply_draw();
	}
	else
		gspWaitForVBlank();
	
	if (--toast_frames_left <= 0) toast_view->set_is_visible(false);

	if (Util_err_query_error_show_flag()) {
		Util_err_main(key);
	} else if(Util_expl_query_show_flag()) {
		Util_expl_main(key);
	} else {
		update_overlay_menu(&key);
		
		// toast_view is never 'updated'
		resource_lock.lock();
		if (popup_view->is_visible) popup_view->update(key);
		else main_view->update(key);
		resource_lock.unlock();
		
		if (video_playing_bar_show) video_update_playing_bar(key);
		
		if (key.p_b) global_intent.next_scene = SceneType::BACK;
	}
}
