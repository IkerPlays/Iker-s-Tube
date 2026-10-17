#include "headers.hpp"
#include "scenes/studio.hpp"
#include "scenes/video_player.hpp"
#include "data_io/string_resource.hpp"
#include "youtube_parser/studio.hpp"
#include "youtube_parser/account.hpp"
#include "youtube_parser/studio_auth.hpp"
#include "oauth/oauth.hpp"
#include "ui/ui.hpp"
#include "ui/notice.hpp"
#include "util/async_task.hpp"
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>

enum StudioMode { MODE_DASH, MODE_PICK, MODE_FORM, MODE_MYVIDEOS, MODE_VIDEO };

static CustomView *root = NULL;
static ScrollView *content = NULL;
static StudioMode mode = MODE_DASH;
static bool mode_dirty = true;

static LightLock state_lock;
static bool state_lock_inited = false;
static StudioStats stats;
static volatile bool stats_loaded = false;
static volatile bool stats_loading = false;
static volatile bool upload_running = false;

static volatile int upload_percent = 0;
static std::string upload_message; // result of the last upload (guarded by state_lock)
static std::string last_uploaded_id; // the video of the last successful upload (guarded by state_lock)

static bool pick_short = false;
static bool pick_image = false; // choosing the thumbnail (an image) instead of a video
static std::string form_thumb;  // path of the chosen thumbnail, "" : none

// your uploads (loaded from the Data API) and the one that is open
static std::vector<StudioVideo> my_videos;
static volatile bool my_loading = false;
static std::string my_error;
static bool my_public_only = false; // the list is the public one (no Studio login)
static StudioVideo sel_video;
static bool delete_armed = false; // the first tap on Delete only asks for a second one
static std::string pick_dir = "sdmc:/";
static std::string form_path, form_title, form_desc;
static int form_privacy = 0; // 0 private, 1 unlisted, 2 public
static int form_category = 0; // index in categories[]
static bool form_short = false;

static const double WIDTH = 300;

struct Category {
	const char *id, *en, *es;
};
static const Category categories[] = {{"22", "People & Blogs", "Gente y blogs"}, {"10", "Music", "Música"},
                                      {"20", "Gaming", "Videojuegos"},          {"24", "Entertainment", "Entretenimiento"},
                                      {"27", "Education", "Educación"},         {"26", "How-to & Style", "Cómo hacer y estilo"},
                                      {"28", "Science & Technology", "Ciencia y tecnología"}, {"23", "Comedy", "Comedia"},
                                      {"1", "Film & Animation", "Cine y animación"}, {"17", "Sports", "Deportes"},
                                      {"25", "News & Politics", "Noticias y política"}, {"19", "Travel & Events", "Viajes y eventos"}};
static const int category_count = sizeof categories / sizeof categories[0];

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
	if (stats_loading || upload_running || !OAuth::is_authenticated()) return;
	stats_loading = true;
	queue_async_task(load_stats_task, NULL);
}

static void start_my_videos_load();

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
	    ->set_corner_radius(7)
	    ->set_on_view_released([on_release](View &) { on_release(); });
}

static bool is_image_file(const std::string &name) {
	size_t dot = name.find_last_of('.');
	if (dot == std::string::npos) return false;
	std::string ext = name.substr(dot + 1);
	for (auto &c : ext) c = tolower(c);
	return ext == "jpg" || ext == "jpeg" || ext == "png";
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
	std::string path, title, description, privacy, category_id, thumb_path;
	bool is_short;
};

static void upload_thread(void *arg) {
	UploadJob *job = (UploadJob *)arg;
	std::string video_id, error;
	int last_notice = -1;
	bool ok = youtube_studio_upload(job->path, job->title, job->description, job->privacy, job->is_short, job->category_id,
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
	std::string thumb_error;
	if (ok && job->thumb_path != "" && video_id != "") {
		notice_show(tr("Setting the thumbnail...", "Poniendo la miniatura..."));
		if (!youtube_studio_set_thumbnail(video_id, job->thumb_path, thumb_error)) {
			thumb_error = tr(" (thumbnail failed: ", " (falló la miniatura: ") + thumb_error + ")";
		} else {
			thumb_error = "";
		}
	}
	LightLock_Lock(&state_lock);
	if (ok) {
		upload_message = tr("Uploaded: ", "Subido: ") + "youtube.com/watch?v=" + video_id + thumb_error;
		stats_loaded = false; // the totals changed : reload them when the tab is shown again
		last_uploaded_id = video_id;
	} else {
		upload_message = tr("Upload failed: ", "Error al subir: ") + error;
		if (error.find("403") != std::string::npos || error.find("401") != std::string::npos) {
			upload_message += tr(" (YouTube does not allow uploads with this login)", " (YouTube no permite subir con este inicio de sesión)");
		}
	}
	LightLock_Unlock(&state_lock);
	notice_show(ok ? tr("Upload finished", "Subida terminada") : tr("Could not upload the video", "No se pudo subir el vídeo"));
	delete job;
	upload_running = false;
	var_need_refresh = true;
}

static void start_upload() {
	if (upload_running) return;
	UploadJob *job = new UploadJob{form_path, form_title, form_desc, privacy_name(form_privacy), categories[form_category].id, form_thumb, form_short};
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

struct NewPlaylistJob {
	std::string title, privacy;
};
static void new_playlist_task(void *arg) {
	NewPlaylistJob *job = (NewPlaylistJob *)arg;
	std::string id;
	bool ok = youtube_account_playlist_create(job->title, job->privacy, id);
	notice_show(ok ? tr("Playlist created: ", "Lista creada: ") + job->title : tr("Could not create the playlist", "No se pudo crear la lista"));
	delete job;
}

// ---- Studio login (own Google Cloud client, see Documentation/STUDIO_SETUP.md) ----
static volatile bool login_running = false;
static std::string login_text; // what to do (guarded by state_lock)

static void set_login_text(const std::string &t) {
	LightLock_Lock(&state_lock);
	login_text = t;
	LightLock_Unlock(&state_lock);
	mode_dirty = true;
	var_need_refresh = true;
}

static void login_thread(void *) {
	StudioDeviceCode code;
	std::string error;
	if (!studio_auth_request_code(code, error)) {
		set_login_text(tr("Could not start the login: ", "No se pudo iniciar el acceso: ") + error);
		login_running = false;
		return;
	}
	set_login_text(tr("Open ", "Abre ") + code.url + tr(" and type ", " y escribe ") + code.user_code);
	u64 start = osGetTime();
	while (true) {
		svcSleepThread((s64)code.interval * 1000000000LL);
		if (osGetTime() - start > (u64)(code.expires_in > 0 ? code.expires_in : 600) * 1000) {
			set_login_text(tr("The code expired, try again", "El código caducó, inténtalo de nuevo"));
			break;
		}
		int result = studio_auth_poll(code, error);
		if (result == 1) {
			set_login_text("");
			notice_show(tr("Studio connected", "Studio conectado"));
			stats_loaded = false;
			break;
		}
		if (result < 0) {
			set_login_text(tr("Login failed: ", "Falló el acceso: ") + error);
			break;
		}
	}
	login_running = false;
}

static void start_login() {
	if (login_running) return;
	login_running = true;
	set_login_text(tr("Asking for a code...", "Pidiendo un código..."));
	if (!threadCreate(login_thread, NULL, 64 * 1024, 0x30, -1, true)) {
		login_running = false;
		set_login_text(tr("Could not start the login", "No se pudo iniciar el acceso"));
	}
}

static bool studio_ready_or_notice() {
	if (studio_auth_signed_in()) return true;
	notice_show(tr("Sign in for Studio first (Studio tab)", "Inicia sesión en Studio primero (pestaña Studio)"));
	return false;
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
	if (!studio_auth_has_client()) { // uploading and editing need the owner's own Google Cloud client
		v.push_back(label([]() { return tr("To upload or edit: create studio_client.txt", "Para subir o editar: crea studio_client.txt"); }));
		v.push_back(label([]() { return tr("in 3ds/IkersTube (see STUDIO_SETUP.md)", "en 3ds/IkersTube (mira STUDIO_SETUP.md)"); }));
	} else if (!studio_auth_signed_in()) {
		v.push_back(button([]() { return login_running ? tr("Waiting for the login...", "Esperando el acceso...") : tr("Sign in for Studio", "Iniciar sesión en Studio"); },
		                   []() { start_login(); }));
		v.push_back(label([]() { return locked_copy(login_text); }));
	} else {
		v.push_back(button([]() { return tr("Studio connected - sign out", "Studio conectado - cerrar sesión"); }, []() {
			studio_auth_sign_out();
			mode_dirty = true;
			var_need_refresh = true;
		}));
	}
	v.push_back(button([]() { return upload_running ? tr("Uploading: ", "Subiendo: ") + std::to_string(upload_percent) + "%"
	                                                  : tr("Upload a video", "Subir un vídeo"); },
	                   []() {
		                   if (upload_running || !studio_ready_or_notice()) return;
		                   pick_short = false;
		                   pick_image = false;
		                   go(MODE_PICK);
	                   }));
	v.push_back(button([]() { return tr("Upload a Short", "Subir un Short"); }, []() {
		if (upload_running || !studio_ready_or_notice()) return;
		pick_short = true;
		pick_image = false;
		go(MODE_PICK);
	}));
	v.push_back(button([]() { return tr("Edit my uploads", "Editar mis subidas"); }, []() {
		start_my_videos_load();
		go(MODE_MYVIDEOS);
	}));
	v.push_back(button([]() { return tr("New playlist", "Nueva lista"); }, []() {
		std::string title;
		if (!ask_text(tr("Playlist name", "Nombre de la lista"), "", 100, false, title)) return;
		notice_show(tr("Working...", "Trabajando..."));
		queue_async_task(new_playlist_task, new NewPlaylistJob{title, "PRIVATE"}); // private : change it on YouTube if needed
	}));
	v.push_back(button([]() { return tr("My channel and videos", "Mi canal y vídeos"); }, []() {
		if (OAuth::get_user_channel_id() == "") {
			notice_show(tr("Your channel is not known yet", "Aún no se conoce tu canal"));
			return;
		}
		global_intent.next_scene = SceneType::CHANNEL; // the channel page lists the videos with their views
		global_intent.arg = OAuth::get_user_channel_id();
	}));
	v.push_back(label([]() { return locked_copy(upload_message); }));
	if (locked_copy(last_uploaded_id) != "") { // watch what was just uploaded
		v.push_back(button([]() { return tr("Watch the uploaded video", "Ver el vídeo subido"); }, []() {
			global_intent.next_scene = SceneType::VIDEO_PLAYER;
			global_intent.arg = youtube_get_video_url_by_id(locked_copy(last_uploaded_id));
		}));
	}
	return v;
}

static std::vector<View *> build_pick() {
	std::vector<View *> v;
	v.push_back(button([]() { return LOCALIZED(CANCEL); }, []() { go(pick_image ? MODE_FORM : MODE_DASH); }));
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
			} else if (pick_image ? is_image_file(name) : is_video_file(name)) {
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
			if (pick_image) { // the thumbnail : back to the form of the video
				form_thumb = path;
				pick_image = false;
				go(MODE_FORM);
				return;
			}
			form_path = path;
			form_title = name.substr(0, name.find_last_of('.'));
			if (form_title.size() > 95) form_title.resize(95);
			form_desc = "";
			form_privacy = 0;
			form_category = 0;
			form_thumb = "";
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
	v.push_back(button([]() { return tr("Category: ", "Categoría: ") + tr(categories[form_category].en, categories[form_category].es); },
	                   []() {
		                   form_category = (form_category + 1) % category_count;
		                   var_need_refresh = true;
	                   }));
	v.push_back(button([]() { return tr("Thumbnail: ", "Miniatura: ") + (form_thumb == "" ? std::string("-") : form_thumb.substr(form_thumb.find_last_of('/') + 1)); },
	                   []() {
		                   pick_image = true;
		                   go(MODE_PICK);
	                   }));
	v.push_back(button([]() { return tr("Upload", "Subir"); }, []() { start_upload(); }));
	v.push_back(button([]() { return LOCALIZED(CANCEL); }, []() { go(MODE_DASH); }));
	return v;
}

// ---- your uploads ----
static void load_my_videos_task(void *) {
	std::vector<StudioVideo> list;
	std::string error;
	bool ok;
	if (studio_auth_signed_in()) {
		ok = youtube_studio_list_videos(list, error);
	} else { // without the Studio login only the public videos of the channel page can be listed (no editing)
		YouTubeChannelDetail channel = youtube_load_channel_page(OAuth::get_user_channel_id());
		ok = channel.error == "";
		error = channel.error;
		for (auto &video : channel.videos) list.push_back({youtube_get_video_id_by_url(video.url), video.title, "public"});
	}
	LightLock_Lock(&state_lock);
	my_videos = list;
	my_error = ok ? "" : error;
	my_public_only = !studio_auth_signed_in();
	LightLock_Unlock(&state_lock);
	my_loading = false;
	mode_dirty = true;
	var_need_refresh = true;
}

static void start_my_videos_load() {
	if (my_loading || !OAuth::is_authenticated()) return;
	my_loading = true;
	queue_async_task(load_my_videos_task, NULL);
}

struct EditJob {
	int action; // 0 privacy, 1 title, 2 delete
	std::string id, value;
};

static void edit_task(void *arg) {
	EditJob *job = (EditJob *)arg;
	std::string error;
	bool ok = job->action == 0   ? youtube_studio_set_privacy(job->id, job->value, error)
	          : job->action == 1 ? youtube_studio_set_title(job->id, job->value, error)
	                             : youtube_studio_delete(job->id, error);
	if (ok) {
		LightLock_Lock(&state_lock);
		for (size_t i = 0; i < my_videos.size(); i++) {
			if (my_videos[i].id != job->id) continue;
			if (job->action == 0) my_videos[i].privacy = job->value;
			if (job->action == 1) my_videos[i].title = job->value;
			if (job->action == 2) my_videos.erase(my_videos.begin() + i);
			break;
		}
		if (sel_video.id == job->id) {
			if (job->action == 0) sel_video.privacy = job->value;
			if (job->action == 1) sel_video.title = job->value;
		}
		LightLock_Unlock(&state_lock);
		notice_show(tr("Done", "Hecho"));
		if (job->action == 2) go(MODE_MYVIDEOS);
	} else {
		notice_show(tr("Could not change it: ", "No se pudo cambiar: ") + error);
	}
	mode_dirty = true;
	var_need_refresh = true;
	delete job;
}

static void edit_async(int action, const std::string &id, const std::string &value) {
	if (!studio_ready_or_notice()) return;
	notice_show(tr("Working...", "Trabajando..."));
	queue_async_task(edit_task, new EditJob{action, id, value});
}

static std::vector<View *> build_myvideos() {
	std::vector<View *> v;
	v.push_back(button([]() { return tr("Back", "Atrás"); }, []() { go(MODE_DASH); }));
	v.push_back(label([]() {
		if (my_loading) return tr("Loading ...", "Cargando ...");
		std::string err = locked_copy(my_error);
		if (err != "") return tr("Could not load: ", "No se pudo cargar: ") + err;
		return my_public_only ? tr("Public videos only - sign in for Studio to edit", "Solo vídeos públicos - inicia sesión en Studio para editar")
		                      : tr("Tap a video to edit it", "Toca un vídeo para editarlo");
	}));
	std::vector<StudioVideo> list;
	LightLock_Lock(&state_lock);
	list = my_videos;
	LightLock_Unlock(&state_lock);
	for (auto &video : list) {
		StudioVideo copy = video;
		std::string text = "[" + (copy.privacy == "public" ? tr("Public", "Público") : copy.privacy == "unlisted" ? tr("Unlisted", "No listado") : tr("Private", "Privado")) + "] " + copy.title;
		if (text.size() > 44) text = text.substr(0, 44) + "..";
		v.push_back(button([text]() { return text; }, [copy]() {
			LightLock_Lock(&state_lock);
			sel_video = copy;
			LightLock_Unlock(&state_lock);
			delete_armed = false;
			go(MODE_VIDEO);
		}));
	}
	return v;
}

static std::vector<View *> build_video() {
	std::vector<View *> v;
	v.push_back(button([]() { return tr("Back", "Atrás"); }, []() { go(MODE_MYVIDEOS); }));
	v.push_back(label([]() {
		std::string title = locked_copy(sel_video.title);
		return title.size() > 46 ? title.substr(0, 46) + ".." : title;
	}));
	v.push_back(button([]() {
		std::string p = locked_copy(sel_video.privacy);
		return tr("Visibility: ", "Visibilidad: ") + (p == "public" ? tr("Public", "Público") : p == "unlisted" ? tr("Unlisted", "No listado") : tr("Private", "Privado"));
	}, []() {
		std::string p = locked_copy(sel_video.privacy);
		std::string next = p == "private" ? "unlisted" : p == "unlisted" ? "public" : "private";
		edit_async(0, locked_copy(sel_video.id), next);
	}));
	v.push_back(button([]() { return tr("Change the title", "Cambiar el título"); }, []() {
		std::string out;
		if (ask_text(tr("Title", "Título"), locked_copy(sel_video.title), 95, false, out)) edit_async(1, locked_copy(sel_video.id), out);
		var_need_refresh = true;
	}));
	v.push_back(button([]() { return tr("Watch it", "Verlo"); }, []() {
		global_intent.next_scene = SceneType::VIDEO_PLAYER;
		global_intent.arg = youtube_get_video_url_by_id(locked_copy(sel_video.id));
	}));
	v.push_back(button([]() { return delete_armed ? tr("Tap again to DELETE it for good", "Toca otra vez para BORRARLO") : tr("Delete", "Borrar"); }, []() {
		if (!delete_armed) {
			delete_armed = true;
			var_need_refresh = true;
			return;
		}
		delete_armed = false;
		edit_async(2, locked_copy(sel_video.id), "");
	}));
	return v;
}

static void rebuild() {
	if (content) {
		content->recursive_delete_subviews();
		delete content;
		content = NULL;
	}
	std::vector<View *> rows = mode == MODE_DASH       ? build_dash()
	                          : mode == MODE_PICK     ? build_pick()
	                          : mode == MODE_FORM     ? build_form()
	                          : mode == MODE_MYVIDEOS ? build_myvideos()
	                                                  : build_video();
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
