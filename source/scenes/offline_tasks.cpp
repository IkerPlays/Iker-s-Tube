#include "headers.hpp"
#include "scenes/offline_tasks.hpp"
#include "data_io/offline.hpp"
#include "data_io/string_resource.hpp"
#include "network_decoder/network_io.hpp"
#include "ui/notice.hpp"
#include "util/async_task.hpp"
#include <sys/stat.h>

struct DownloadJob {
	std::string id, title, author, url;
};

static volatile bool download_running = false; // one download at a time

static void download_task(void *arg) {
	DownloadJob *job = (DownloadJob *)arg;
	static NetworkSessionList session;
	if (!session.inited) {
		session.init();
	}
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	mkdir("sdmc:/3ds/IkersTube/offline", 0777);
	std::string path = offline_file_path(job->id);
	std::string tmp_path = path + ".part";
	FILE *file = fopen(tmp_path.c_str(), "wb");
	bool ok = file != NULL;
	u64 total = 0, done = 0;
	const u64 CHUNK = 1024 * 1024;
	int last_percent = -1;
	while (ok) {
		u64 end = done + CHUNK - 1;
		auto result = session.perform(HttpRequest::GET(job->url, {{"Range", "bytes=" + std::to_string(done) + "-" + std::to_string(end)}}));
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
		int percent = total ? (int)(done * 100 / total) : 0;
		if (percent != last_percent && percent % 10 == 0) {
			last_percent = percent;
			notice_show(tr("Downloading audio: ", "Descargando audio: ") + std::to_string(percent) + "%");
		}
		if (total && done >= total) break;
	}
	if (file) fclose(file);
	if (ok && total && done >= total) {
		remove(path.c_str());
		rename(tmp_path.c_str(), path.c_str());
		offline_add({job->id, job->title, job->author});
		notice_show(tr("Saved for offline listening", "Guardado para escuchar sin conexión"));
	} else {
		remove(tmp_path.c_str());
		notice_show(tr("Could not download the audio", "No se pudo descargar el audio"));
	}
	delete job;
	download_running = false;
}

void offline_download_async(const std::string &id, const std::string &title, const std::string &author,
                            const std::string &audio_url) {
	if (audio_url == "" || id == "") {
		notice_show(tr("This video has no audio stream to save", "Este vídeo no tiene audio que guardar"));
		return;
	}
	if (download_running) {
		notice_show(tr("Another download is still running", "Ya hay una descarga en curso"));
		return;
	}
	download_running = true;
	notice_show(tr("Downloading audio: 0%", "Descargando audio: 0%"));
	// its own thread : the shared async task thread must stay free for loading pages while the file downloads
	threadCreate(download_task, new DownloadJob{id, title, author, audio_url}, DEF_STACKSIZE, DEF_THREAD_PRIORITY_NORMAL, -1, true);
}
