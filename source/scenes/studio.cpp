#include "headers.hpp"
#include "scenes/studio.hpp"
#include "scenes/video_player.hpp"
#include "data_io/string_resource.hpp"
#include "youtube_parser/studio.hpp"
#include "oauth/oauth.hpp"
#include "ui/ui.hpp"
#include "ui/notice.hpp"
#include "util/async_task.hpp"
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>

enum StudioMode { MODE_DASH, MODE_PICK, MODE_FORM };

static CustomView *root = NULL;
static ScrollView *content = NULL;
static StudioMode mode = MODE_DASH;
static bool mode_dirty = true;

static LightLock state_lock;
static bool state_lock_inited = false;
static StudioStats stats;
static bool stats_loaded = false;
static volatile bool stats_loading = false;

static volatile bool upload_running = false;
static volatile int upload_percent = 0;
static std::string upload_message; // result of the last upload (guarded by state_lock)

static bool pick_short = false;
static std::string pick_dir = "sdmc:/";
static std::string form_path, form_title, form_desc;
static int form_privacy = 0; // 0 private, 1 unlisted, 2 public
static bool form_short = false;

static const double WIDTH = 300;

static std::string locked_copy(const std::string &s) {
	LightLock_Lock(&state_lock);
	std::string res = s;
	LightLock_Unlock(&state_lock);
	return res;
}

static void load_stats_task(void *) {
	StudioStats res = youtube_studio_stats();
	LightLock_Lock(&state_lock);
	stats = res;
	stats_loaded = true;
	LightLock_Unlock(&state_lock);
	stats_loading = false;
	var_need_refresh = true;
}

static void start_stats_load() {
	if (stats_loading || !OAuth::is_authenticated()) return;
	stats_loading = true;
	queue_async_task(load_stats_task, NULL);
}

static void go(StudioMode m) {
	mode = m;
	mode_dirty = true;
	var_need_refresh = true;
}

static const char *privacy_name(int p) {
	return p == 0 ? "Private" : p == 1 ? "Unlisted" : "Public";
}
static std::string privacy_text(int p) {
	return p == 0 ? tr("Private", "Privado") : p == 1 ? tr("Unlisted", "No listado") : tr("Public", "Público");
}

// asks a text with the system keyboard. Returns false if the user cancelled.
static bool ask_text(const std::string &hint, const std::string &initial, int max_length, bool allow_empty, std::string &out) {
	SwkbdState keyboard;
	swkbdInit(&keyboard, SWKBD_TYPE_NORMAL, 2, max_length);
	swkbdSetFeatures(&keyboard, SWKBD_DEFAULT_QWERTY);
	swkbdSetValidation(&keyboard, allow_empty ? SWKBD_ANYTHING : SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
	swkbdSetHintText(&keyboard, hint.c_str());
	swkbdSetInitialText(&keyboard, initial.c_str());
	swkbdSetButton(&keyboard, SWKBD_BUTTON_LEFT, LOCALIZED(CANCEL).c_str(), false);
	swkbdSetButton(&keyboard, SWKBD_BUTTON_RIGHT, LOCALIZED(OK).c_str(), true);
	std::vector<char> text(max_length + 8, 0);
	video_set_skip_drawing(true);
	auto pressed = swkbdInputText(&keyboard, text.data(), text.size());
	video_set_skip_drawing(false);
	if (pressed != SWKBD_BUTTON_RIGHT) return false;
	out = text.data();
	return true;
}

static TextView *label(const std::function<std::string()> &text) {
	return (new TextView(0, 0, WIDTH, 22))
	    ->set_text(text)
	    ->set_y_alignment(TextView::YAlign::CENTER)
	    ->set_text_offset(SMALL_MARGIN, 0);
}

static View *button(const std::function<std::string()> &text, std::function<void()> on_release) {
	return (new TextView(0, 0, WIDTH, 26))
	    ->set_text(text)
	    ->set_x_alignment(TextView::XAlign::CENTER)
	    ->set_y_alignment(TextView::YAlign::CENTER)
	    ->set_get_background_color(View::STANDARD_BACKGROUND)
	    ->set_on_view_released([on_release](View &) { on_release(); });
}

static bool is_video_file(const std::string &name) {
	size_t dot = name.find_last_of('.');
	if (dot == std::string::npos) return false;
	std::string ext = name.substr(dot + 1);
	for (auto &c : ext) c = tolower(c);
	return ext == "mp4" || ext == "m4v" || ext == "mov" || ext == "avi" || ext == "mkv" || ext == "webm" || ext == "3gp" ||
	       ext == "flv" || ext == "wmv" || ext == "mpg" || ext == "mpeg";
}

static std::string parent_dir(const std::string &dir) {
	std::string d = dir;
	while (d.size() > 6 && d.back() == '/') d.pop_back();
	size_t slash = d.find_last_of('/');
	if (slash == std::string::npos || slash < 6) return "sdmc:/";
	return d.substr(0, slash + 1);
}

struct UploadJob {
	std::string path, title, description, privacy;
	bool is_short;
};

static void upload_thread(void *arg) {
	UploadJob *job = (UploadJob *)arg;
	std::string video_id, error;
	int last_notice = -1;
	bool ok = youtube_studio_upload(job->path, job->title, job->description, job->privacy, job->is_short,
	                                [&](u64 done, u64 total) {
		                                int percent = total ? (int)(done * 100 / total) : 0;
		                                upload_percent = percent;
		                                var_need_refresh = true;
		                                if (percent / 10 != last_notice) {
			                                last_notice = percent / 10;
			                                notice_show(tr("Uploading: ", "Subiendo: ") + std::to_string(percent) + "%");
		                                }
	                                },
	                                video_id, error);
	LightLock_Lock(&state_lock);
	if (ok) {
		upload_message = tr("Uploaded: ", "Subido: ") + "youtube.com/watch?v=" + video_id;
	} else {
		upload_message = tr("Upload failed: ", "Error al subir: ") + error;
	}
	LightLock_Unlock(&state_lock);
	notice_show(ok ? tr("Upload finished", "Subida terminada") : tr("Could not upload the video", "No se pudo subir el vídeo"));
	delete job;
	upload_running = false;
	var_need_refresh = true;
}

static void start_upload() {
	if (upload_running) return;
	UploadJob *job = new UploadJob{form_path, form_title, form_desc, privacy_name(form_privacy), form_short};
	for (auto &c : job->privacy) c = tolower(c);
	upload_running = true;
	upload_percent = 0;
	LightLock_Lock(&state_lock);
	upload_message = "";
	LightLock_Unlock(&state_lock);
	if (!threadCreate(upload_thread, job, 64 * 1024, 0x30, -1, true)) {
		delete job;
		upload_running = false;
		notice_show(tr("Could not start the upload", "No se pudo iniciar la subida"));
		return;
	}
	notice_show(tr("Uploading: 0%", "Subiendo: 0%"));
	go(MODE_DASH);
}

static std::vector<View *> build_dash() {
	std::vector<View *> v;
	v.push_back((new TextView(0, 0, WIDTH, 24))
	                ->set_text((std::function<std::string()>)[]() {
		                std::string name = OAuth::is_authenticated() ? OAuth::user_account_name : "";
		                return std::string("Studio") + (name != "" ? " - " + name : "");
	                })
	                ->set_x_alignment(TextView::XAlign::CENTER)
	                ->set_y_alignment(TextView::YAlign::CENTER)
	                ->set_get_background_color([](const View &) { return LIGHT0_BACK_COLOR; }));
	if (!OAuth::is_authenticated()) {
		v.push_back(label([]() { return tr("Sign in to your account in Settings to use Studio", "Inicia sesión en Ajustes para usar Studio"); }));
		return v;
	}
	v.push_back(label([]() {
		return tr("Subscribers: ", "Suscriptores: ") + locked_copy(stats.subscribers);
	}));
	v.push_back(label([]() { return tr("Total views: ", "Visitas totales: ") + locked_copy(stats.views); }));
	v.push_back(label([]() { return tr("Total videos: ", "Vídeos totales: ") + locked_copy(stats.videos); }));
	v.push_back(label([]() {
		std::string err = locked_copy(stats.error);
		if (stats_loading) return tr("Loading ...", "Cargando ...");
		return err == "" ? std::string("") : tr("Statistics unavailable (", "Estadísticas no disponibles (") + err + ")";
	}));
	v.push_back(button([]() { return LOCALIZED(RELOAD); }, []() { start_stats_load(); }));
	v.push_back(button([]() { return upload_running ? tr("Uploading: ", "Subiendo: ") + std::to_string(upload_percent) + "%"
	                                                  : tr("Upload a video", "Subir un vídeo"); },
	                   []() {
		                   if (upload_running) return;
		                   pick_short = false;
		                   go(MODE_PICK);
	                   }));
	v.push_back(button([]() { return tr("Upload a Short", "Subir un Short"); }, []() {
		if (upload_running) return;
		pick_short = true;
		go(MODE_PICK);
	}));
	v.push_back(label([]() { return locked_copy(upload_message); }));
	return v;
}

static std::vector<View *> build_pick() {
	std::vector<View *> v;
	v.push_back(button([]() { return LOCALIZED(CANCEL); }, []() { go(MODE_DASH); }));
	v.push_back(label([]() { return pick_dir; }));
	if (pick_dir != "sdmc:/") {
		v.push_back(button([]() { return std::string("[..]"); }, []() {
			pick_dir = parent_dir(pick_dir);
			go(MODE_PICK);
		}));
	}
	std::vector<std::string> dirs, files;
	std::vector<u64> sizes;
	DIR *dir = opendir(pick_dir.c_str());
	if (dir) {
		while (struct dirent *entry = readdir(dir)) {
			std::string name = entry->d_name;
			if (name == "." || name == ".." || name[0] == '.') continue;
			if (entry->d_type == DT_DIR) {
				dirs.push_back(name);
			} else if (is_video_file(name)) {
				files.push_back(name);
			}
			if (dirs.size() + files.size() > 400) break;
		}
		closedir(dir);
	}
	std::sort(dirs.begin(), dirs.end());
	std::sort(files.begin(), files.end());
	for (auto &d : dirs) {
		std::string name = d;
		v.push_back(button([name]() { return "[" + name + "]"; }, [name]() {
			pick_dir += name + "/";
			go(MODE_PICK);
		}));
	}
	for (auto &f : files) {
		std::string name = f, path = pick_dir + f;
		struct stat st;
		u64 size = stat(path.c_str(), &st) == 0 ? (u64)st.st_size : 0;
		std::string text = name + " (" + std::to_string(size / (1024 * 1024)) + " MB)";
		v.push_back(button([text]() { return text; }, [name, path]() {
			form_path = path;
			form_title = name.substr(0, name.find_last_of('.'));
			if (form_title.size() > 95) form_title.resize(95);
			form_desc = "";
			form_privacy = 0;
			form_short = pick_short;
			go(MODE_FORM);
		}));
	}
	if (dirs.empty() && files.empty()) {
		v.push_back(label([]() { return tr("No videos in this folder", "No hay vídeos en esta carpeta"); }));
	}
	return v;
}

static std::vector<View *> build_form() {
	std::vector<View *> v;
	v.push_back(label([]() { return (form_short ? tr("Short: ", "Short: ") : tr("Video: ", "Vídeo: ")) + form_path.substr(form_path.find_last_of('/') + 1); }));
	v.push_back(button([]() { return tr("Title: ", "Título: ") + form_title; }, []() {
		std::string out;
		if (ask_text(tr("Title", "Título"), form_title, 95, false, out)) form_title = out;
		var_need_refresh = true;
	}));
	v.push_back(button([]() { return tr("Description: ", "Descripción: ") + (form_desc == "" ? "-" : form_desc.substr(0, 40)); }, []() {
		std::string out;
		if (ask_text(tr("Description", "Descripción"), form_desc, 400, true, out)) form_desc = out;
		var_need_refresh = true;
	}));
	v.push_back(button([]() { return tr("Visibility: ", "Visibilidad: ") + privacy_text(form_privacy); }, []() {
		form_privacy = (form_privacy + 1) % 3;
		var_need_refresh = true;
	}));
	v.push_back(button([]() { return tr("Upload", "Subir"); }, []() { start_upload(); }));
	v.push_back(button([]() { return LOCALIZED(CANCEL); }, []() { go(MODE_DASH); }));
	return v;
}

static void rebuild() {
	if (content) {
		content->recursive_delete_subviews();
		delete content;
		content = NULL;
	}
	std::vector<View *> rows = mode == MODE_DASH ? build_dash() : mode == MODE_PICK ? build_pick() : build_form();
	content = (new ScrollView(0, 0, 320, root ? root->height : 200))
	              ->set_views({(new VerticalListView(0, 0, 320))->set_margin(SMALL_MARGIN)->set_views(rows)});
	mode_dirty = false;
}

View *Studio_create_view() {
	if (!state_lock_inited) {
		LightLock_Init(&state_lock);
		state_lock_inited = true;
	}
	mode = MODE_DASH;
	mode_dirty = true;
	content = NULL;
	root = (new CustomView(0, 0, 320, 200))
	           ->set_draw([](const CustomView &self) {
		           if (content) content->draw(self.x0, self.y0);
	           })
	           ->set_update([](CustomView &self, Hid_info key) {
		           static bool last_auth = false;
		           if (last_auth != OAuth::is_authenticated()) {
			           last_auth = OAuth::is_authenticated();
			           mode_dirty = true;
			           stats_loaded = false;
		           }
		           if (!stats_loaded && !stats_loading) start_stats_load();
		           if (mode_dirty || !content) rebuild();
		           content->update_y_range(0, self.height);
		           content->update(key, self.x0, self.y0);
	           });
	return root;
}

void Studio_set_height(double height) {
	if (root) root->update_y_range(0, height);
}

void Studio_exit() {
	if (content) {
		content->recursive_delete_subviews();
		delete content;
		content = NULL;
	}
	root = NULL;
}
