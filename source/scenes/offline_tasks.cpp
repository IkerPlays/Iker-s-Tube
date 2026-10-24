#include <dirent.h>
#include "headers.hpp"
#include "scenes/offline_tasks.hpp"
#include "data_io/offline.hpp"
#include "data_io/string_resource.hpp"
#include "network_decoder/network_io.hpp"
#include "ui/notice.hpp"
#include "util/async_task.hpp"
#include "youtube_parser/parser.hpp"
#include <sys/stat.h>
#include <functional>
#include <vector>

struct DownloadPart {
	std::string url, path;
};
struct DownloadJob {
	std::string id, title, author;
	std::vector<DownloadPart> parts;
	int quality = 0;
	bool combined = false;
};

static volatile bool download_running = false; // one download at a time
static volatile bool download_cancel = false;
static volatile bool downloads_paused = false; // the running download waits between two chunks
static volatile int download_percent = 0;
static std::string download_title; // set before the thread starts, read by the UI

static LightLock pending_lock;
static std::vector<DownloadJob *> pending; // queued while a download runs
static struct PendingLockInit {
	PendingLockInit() { LightLock_Init(&pending_lock); }
} pending_lock_init;

bool offline_download_running() { return download_running; }
int offline_download_queued() {
	LightLock_Lock(&pending_lock);
	int n = pending.size();
	LightLock_Unlock(&pending_lock);
	return n;
}
int offline_download_percent() { return download_percent; }
std::string offline_download_title() { return download_title; }
static std::string running_job_id; // the video that is downloading now (under pending_lock)
static volatile int save_epoch = 0; // grows at every cancel : the "save by id" tasks queued before it do nothing
void offline_download_cancel() { // the running download and the queued ones
	LightLock_Lock(&pending_lock);
	for (auto job : pending) delete job;
	pending.clear();
	LightLock_Unlock(&pending_lock);
	download_cancel = true;
	downloads_paused = false;
	save_epoch = save_epoch + 1;
}

// downloads one file in 1 MB ranged chunks into `<path>.part`, then renames it. `progress` gets 0..100.
void offline_downloads_set_paused(bool paused) { downloads_paused = paused; }
bool offline_downloads_paused() { return downloads_paused; }

static bool download_part(NetworkSessionList &session, const DownloadPart &part, const std::function<void(int)> &progress) {
	std::string tmp_path = part.path + ".part";
	FILE *file = fopen(tmp_path.c_str(), "wb");
	bool ok = file != NULL;
	u64 total = 0, done = 0;
	int failures = 0; // in a row
	const u64 CHUNK = 1024 * 1024;
	while (ok) {
		if (download_cancel) {
			ok = false;
			break;
		}
		while (downloads_paused && !download_cancel) svcSleepThread(500000000LL); // paused : wait here (the file keeps what it has)
		if (download_cancel) {
			ok = false;
			break;
		}
		u64 end = done + CHUNK - 1;
		auto result = session.perform(HttpRequest::GET(part.url, {{"Range", "bytes=" + std::to_string(done) + "-" + std::to_string(end)}}));
		if (result.fail || !result.status_code_is_success() || result.data.empty()) {
			logger.warning("offline", "download failed at " + std::to_string(done) + " : http " + std::to_string(result.status_code) + " " + result.error);
			// a Wi-Fi hiccup : wait a little and ask the same chunk again (the file keeps what was already written)
			if (++failures >= 8 || download_cancel) {
				ok = false;
				break;
			}
			notice_show(tr("Connection problem, trying again...", "Problema de conexión, reintentando..."));
			svcSleepThread((s64)failures * 2000000000LL);
			continue;
		}
		failures = 0;
		if (total == 0) {
			auto content_range = result.get_header("Content-Range");
			size_t slash = content_range.find('/');
			if (slash != std::string::npos) total = strtoull(content_range.c_str() + slash + 1, NULL, 10);
			if (total == 0) {
				if (result.status_code == 206) { // a partial answer without the full size: the end of the file is unknown
					logger.warning("offline", "206 without Content-Range : the size is unknown");
					ok = false;
					break;
				}
				total = result.data.size(); // the server sent everything at once
			}
		}
		if (fwrite(result.data.data(), 1, result.data.size(), file) != result.data.size()) {
			ok = false;
			break;
		}
		done += result.data.size();
		progress(total ? (int)(done * 100 / total) : 0);
		if (total && done >= total) break;
	}
	if (file) fclose(file);
	if (ok && total && done >= total) {
		return true; // complete : stays as `<path>.part` until every part of the job is done (see run_job)
	}
	remove(tmp_path.c_str());
	return false;
}

static void run_job(DownloadJob *job, NetworkSessionList &session) {
	download_title = job->title;
	download_percent = 0;
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	mkdir("sdmc:/3ds/IkersTube/offline", 0777);
	std::string what = job->quality > 0 ? tr("video", "vídeo") : tr("audio", "audio");
	bool ok = true;
	int last_percent = -1;
	for (size_t i = 0; i < job->parts.size() && ok; i++) {
		ok = download_part(session, job->parts[i], [&](int percent) {
			int overall = (int)((i * 100 + percent) / job->parts.size());
			download_percent = overall;
			var_need_refresh = true;
			if (overall != last_percent && overall % 10 == 0) {
				last_percent = overall;
				notice_show(tr("Downloading ", "Descargando ") + what + ": " + std::to_string(overall) + "%");
			}
		});
	}
	if (ok) { // every part is complete : only now the files of an older copy are replaced
		for (auto &part : job->parts) {
			remove(part.path.c_str());
			if (rename((part.path + ".part").c_str(), part.path.c_str()) != 0) ok = false;
		}
	}
	if (ok) {
		OfflineItem item;
		item.id = job->id;
		item.title = job->title;
		item.author = job->author;
		item.quality = job->quality;
		item.combined = job->combined;
		if (offline_add(item)) {
			notice_show(tr("Saved for offline viewing: ", "Guardado para ver sin conexión: ") + job->title.substr(0, 30));
		} else { // the list could not be written (SD card full?): the files would be orphans
			for (auto &part : job->parts) remove(part.path.c_str());
			notice_show(tr("Could not download it", "No se pudo descargar"));
		}
	} else {
		for (auto &part : job->parts) remove((part.path + ".part").c_str()); // only the unfinished files : a saved copy stays
		notice_show(download_cancel ? tr("Download cancelled", "Descarga cancelada") : tr("Could not download it", "No se pudo descargar"));
	}
	delete job;
}

// runs the job it gets and then the ones that were queued while it ran
static void download_task(void *arg) {
	static NetworkSessionList session;
	if (!session.inited) {
		session.init();
	}
	DownloadJob *job = (DownloadJob *)arg;
	while (job) {
		if (var_download_window == 1) { // only at night : the job waits (the app must stay open)
			bool announced = false;
			while (!download_cancel) {
				time_t now = time(NULL);
				int hour = gmtime(&now)->tm_hour; // the console clock is local time
				if (hour >= 1 && hour < 7) break;
				if (!announced) {
					announced = true;
					notice_show(tr("Waiting for the night to download", "Esperando a la noche para descargar"));
				}
				svcSleepThread(5000000000LL);
				if (var_download_window != 1) break; // the setting was changed
			}
		}
		if (download_cancel) { // cancelled while it waited for the night
			delete job;
			notice_show(tr("Download cancelled", "Descarga cancelada"));
		} else {
			run_job(job, session);
		}
		LightLock_Lock(&pending_lock);
		if (pending.size()) {
			job = pending.front();
			pending.erase(pending.begin());
			running_job_id = job->id;
			download_cancel = false; // (a job queued after a cancel is a new request)
		} else {
			job = NULL;
			download_running = false;
			running_job_id = "";
		}
		LightLock_Unlock(&pending_lock);
	}
}

void offline_download_async(const std::string &id, const std::string &title, const std::string &author,
                            const std::string &audio_url, bool want_video, const std::string &video_url,
                            const std::string &both_url, int quality) {
	if (id == "") return;
	{ // not enough room on the SD card : do not start (a full SD card breaks saves of the lists and settings too)
		FS_ArchiveResource resource;
		if (R_SUCCEEDED(FSUSER_GetArchiveResource(&resource, SYSTEM_MEDIATYPE_SD)) && (u64)resource.freeClusters * resource.clusterSize < 64ULL * 1024 * 1024) {
			notice_show(tr("The SD card is almost full (less than 64 MB)", "La SD está casi llena (menos de 64 MB)"));
			return;
		}
	}
	DownloadJob *job = new DownloadJob();
	job->id = id;
	job->title = title;
	job->author = author;
	if (want_video) {
		if (video_url != "" && audio_url != "") {
			job->parts = {{video_url, offline_video_path(id)}, {audio_url, offline_file_path(id)}};
			job->quality = quality;
		} else if (both_url != "") {
			job->parts = {{both_url, offline_video_path(id)}};
			job->quality = 360; // the stream that has video and sound together is 360p
			job->combined = true;
		} else {
			delete job;
			notice_show(tr("This video has no stream to save", "Este vídeo no tiene nada que guardar"));
			return;
		}
	} else {
		if (audio_url == "") {
			delete job;
			notice_show(tr("This video has no audio stream to save", "Este vídeo no tiene audio que guardar"));
			return;
		}
		job->parts = {{audio_url, offline_file_path(id)}};
	}
	LightLock_Lock(&pending_lock);
	bool duplicate = download_running && running_job_id == id;
	for (auto queued_job : pending) duplicate = duplicate || queued_job->id == id;
	if (duplicate) { // already downloading or waiting in the queue
		LightLock_Unlock(&pending_lock);
		delete job;
		notice_show(tr("Already in the download queue", "Ya está en la cola de descargas"));
		return;
	}
	if (download_running) { // it will be downloaded after the current one
		pending.push_back(job);
		int queued = pending.size();
		LightLock_Unlock(&pending_lock);
		notice_show(tr("Added to the download queue: ", "Añadido a la cola de descargas: ") + std::to_string(queued));
		return;
	}
	download_running = true;
	running_job_id = id;
	download_cancel = false; // (a cancel of the previous downloads must not stop this one)
	download_title = title;
	download_percent = 0;
	LightLock_Unlock(&pending_lock);
	notice_show(tr("Downloading: 0%", "Descargando: 0%"));
	if (DIR *dir = opendir("sdmc:/3ds/IkersTube/offline")) { // leftovers of a download that was cut (nothing else is downloading now)
		while (struct dirent *entry = readdir(dir)) {
			std::string name = entry->d_name;
			if (name.size() > 5 && name.compare(name.size() - 5, 5, ".part") == 0) remove(("sdmc:/3ds/IkersTube/offline/" + name).c_str());
		}
		closedir(dir);
	}
	// its own thread : the shared async task thread must stay free for loading pages while the file downloads
	if (!threadCreate(download_task, job, DEF_STACKSIZE, DEF_THREAD_PRIORITY_NORMAL, -1, true)) {
		delete job;
		download_running = false;
	}
}

struct SaveByIdJob {
	std::string id, title, author;
	bool want_video;
	int epoch;
};

static void save_by_id_task(void *arg) {
	SaveByIdJob *job = (SaveByIdJob *)arg;
	if (job->epoch != save_epoch) { // cancelled while it waited in the queue
		delete job;
		return;
	}
	YouTubeVideoDetail info = youtube_load_video_page(youtube_get_video_url_by_id(job->id));
	if (!info.is_playable()) {
		notice_show(tr("Could not load the video", "No se pudo cargar el vídeo"));
	} else {
		// the saved quality : 360p at most (240p on the old 3DS), the closest one that exists
		int wanted = var_download_quality == 1 ? 144 : var_download_quality == 2 ? 240 : var_download_quality == 3 ? 360 : (var_is_new3ds ? 360 : 240), quality = 0;
		std::string video_url;
		for (auto &i : info.video_stream_urls) {
			if (i.first <= wanted && i.first > quality) {
				quality = i.first;
				video_url = i.second;
			}
		}
		offline_download_async(job->id, job->title != "" ? job->title : info.title, job->author != "" ? job->author : info.author.name,
		                       info.audio_stream_url, job->want_video, video_url, var_is_new3ds ? info.both_stream_url : "", quality);
	}
	delete job;
}

void offline_save_by_id_async(const std::string &id, const std::string &title, const std::string &author, bool want_video) {
	if (id == "") return;
	{ // already on the SD card (a saved audio can still be upgraded to the video)
		OfflineItem saved;
		if (offline_get(id, saved) && (!want_video || saved.quality > 0)) {
			notice_show(tr("Already saved", "Ya está guardado"));
			return;
		}
	}
	notice_show(tr("Loading the video...", "Cargando el vídeo..."));
	queue_async_task(save_by_id_task, new SaveByIdJob{id, title, author, want_video, save_epoch});
}
