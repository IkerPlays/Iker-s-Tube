#include "headers.hpp"
#include "scenes/offline_tasks.hpp"
#include "data_io/offline.hpp"
#include "data_io/string_resource.hpp"
#include "network_decoder/network_io.hpp"
#include "ui/notice.hpp"
#include "util/async_task.hpp"
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

// downloads one file in 1 MB ranged chunks into `<path>.part`, then renames it. `progress` gets 0..100.
static bool download_part(NetworkSessionList &session, const DownloadPart &part, const std::function<void(int)> &progress) {
	std::string tmp_path = part.path + ".part";
	FILE *file = fopen(tmp_path.c_str(), "wb");
	bool ok = file != NULL;
	u64 total = 0, done = 0;
	const u64 CHUNK = 1024 * 1024;
	while (ok) {
		u64 end = done + CHUNK - 1;
		auto result = session.perform(HttpRequest::GET(part.url, {{"Range", "bytes=" + std::to_string(done) + "-" + std::to_string(end)}}));
		if (result.fail || !result.status_code_is_success() || result.data.empty()) {
			logger.warning("offline", "download failed at " + std::to_string(done) + " : http " + std::to_string(result.status_code) + " " + result.error);
			ok = false;
			break;
		}
		if (total == 0) {
			auto content_range = result.get_header("Content-Range");
			size_t slash = content_range.find('/');
			if (slash != std::string::npos) total = strtoull(content_range.c_str() + slash + 1, NULL, 10);
			if (total == 0) total = result.data.size(); // the server sent everything at once
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
		remove(part.path.c_str());
		return rename(tmp_path.c_str(), part.path.c_str()) == 0;
	}
	remove(tmp_path.c_str());
	return false;
}

static void download_task(void *arg) {
	DownloadJob *job = (DownloadJob *)arg;
	static NetworkSessionList session;
	if (!session.inited) {
		session.init();
	}
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	mkdir("sdmc:/3ds/IkersTube/offline", 0777);
	std::string what = job->quality > 0 ? tr("video", "vídeo") : tr("audio", "audio");
	bool ok = true;
	int last_percent = -1;
	for (size_t i = 0; i < job->parts.size() && ok; i++) {
		ok = download_part(session, job->parts[i], [&](int percent) {
			int overall = (int)((i * 100 + percent) / job->parts.size());
			if (overall != last_percent && overall % 10 == 0) {
				last_percent = overall;
				notice_show(tr("Downloading ", "Descargando ") + what + ": " + std::to_string(overall) + "%");
			}
		});
	}
	if (ok) {
		OfflineItem item;
		item.id = job->id;
		item.title = job->title;
		item.author = job->author;
		item.quality = job->quality;
		item.combined = job->combined;
		offline_add(item);
		notice_show(tr("Saved for offline viewing", "Guardado para ver sin conexión"));
	} else {
		for (auto &part : job->parts) remove(part.path.c_str());
		notice_show(tr("Could not download it", "No se pudo descargar"));
	}
	delete job;
	download_running = false;
}

void offline_download_async(const std::string &id, const std::string &title, const std::string &author,
                            const std::string &audio_url, bool want_video, const std::string &video_url,
                            const std::string &both_url, int quality) {
	if (id == "") return;
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
	if (download_running) {
		delete job;
		notice_show(tr("Another download is still running", "Ya hay una descarga en curso"));
		return;
	}
	download_running = true;
	notice_show(tr("Downloading: 0%", "Descargando: 0%"));
	// its own thread : the shared async task thread must stay free for loading pages while the file downloads
	if (!threadCreate(download_task, job, DEF_STACKSIZE, DEF_THREAD_PRIORITY_NORMAL, -1, true)) {
		delete job;
		download_running = false;
	}
}
