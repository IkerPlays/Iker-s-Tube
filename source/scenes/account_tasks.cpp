#include "headers.hpp"
#include "scenes/account_tasks.hpp"
#include "oauth/oauth.hpp"
#include "ui/notice.hpp"
#include "util/async_task.hpp"
#include "data_io/string_resource.hpp"
#include "youtube_parser/translate.hpp"

struct AccountJob {
	int type;
	std::string a, b, c;
	int number = 0;
	bool flag = false;
};

static Mutex result_lock;
static bool playlists_ready = false;
static std::string playlists_video_id;
static std::vector<AccountPlaylist> playlists_result;
static bool translation_ready = false;
static std::string translation_title, translation_text;

bool account_ready() {
	if (OAuth::is_authenticated()) {
		return true;
	}
	notice_show(tr("Sign in with OAuth in Settings first", "Inicia sesión con OAuth en Ajustes primero"));
	return false;
}

enum { LIKE, SUBSCRIBE, PLAYLISTS, PLAYLIST_EDIT, COMMENT, COMMENT_LIKE, COMMENT_REPLY, TRANSLATE };

static void run_job(void *arg) {
	AccountJob *job = (AccountJob *)arg;
	bool ok = false;
	std::string done, failed;
	switch (job->type) {
	case LIKE:
		ok = youtube_account_like(job->a, job->number);
		done = job->number > 0 ? tr("Liked", "Me gusta enviado") : job->number < 0 ? tr("Disliked", "No me gusta enviado") : tr("Rating removed", "Valoración quitada");
		failed = tr("Could not rate the video", "No se pudo valorar el vídeo");
		break;
	case SUBSCRIBE:
		ok = youtube_account_subscribe(job->a, job->flag);
		done = job->flag ? tr("Subscribed on your account", "Suscrito en tu cuenta") : tr("Unsubscribed on your account", "Desuscrito en tu cuenta");
		failed = tr("Could not change the subscription", "No se pudo cambiar la suscripción");
		break;
	case PLAYLISTS: {
		std::vector<AccountPlaylist> list;
		ok = youtube_account_playlists(job->a, list);
		if (ok) {
			result_lock.lock();
			playlists_video_id = job->a;
			playlists_result = list;
			playlists_ready = true;
			result_lock.unlock();
			var_need_refresh = true;
			if (list.empty()) {
				notice_show(tr("No playlists found in your account", "No hay listas en tu cuenta"));
			}
		}
		failed = tr("Could not load your playlists", "No se pudieron cargar tus listas");
		break;
	}
	case PLAYLIST_EDIT:
		ok = youtube_account_playlist_edit(job->a, job->b, job->flag);
		done = job->flag ? tr("Added to the playlist", "Añadido a la lista") : tr("Removed from the playlist", "Quitado de la lista");
		failed = tr("Could not edit the playlist", "No se pudo editar la lista");
		break;
	case COMMENT:
		ok = youtube_account_comment_create(job->a, job->b);
		done = tr("Comment sent", "Comentario enviado");
		failed = tr("Could not send the comment", "No se pudo enviar el comentario");
		break;
	case COMMENT_LIKE:
		ok = youtube_account_comment_like(job->a, job->b);
		done = tr("Comment liked", "Me gusta al comentario enviado");
		failed = tr("Could not like the comment (it must be in the first page)", "No se pudo dar me gusta (debe estar en la primera página)");
		break;
	case TRANSLATE: {
		std::string translated;
		ok = translate_text(job->b, translated);
		if (ok) {
			result_lock.lock();
			translation_title = job->a;
			translation_text = translated;
			translation_ready = true;
			result_lock.unlock();
			var_need_refresh = true;
		}
		failed = tr("Could not translate (no connection?)", "No se pudo traducir (¿sin conexión?)");
		break;
	}
	case COMMENT_REPLY:
		ok = youtube_account_comment_reply(job->a, job->b, job->c);
		done = tr("Reply sent", "Respuesta enviada");
		failed = tr("Could not reply (the comment must be in the first page)", "No se pudo responder (el comentario debe estar en la primera página)");
		break;
	}
	if (ok && done != "") {
		notice_show(done);
	} else if (!ok) {
		std::string why = youtube_account_last_error();
		notice_show(failed + (why != "" && job->type != TRANSLATE ? " (" + why.substr(0, 70) + ")" : ""));
	}
	delete job;
}

static void queue_job(AccountJob *job) {
	notice_show(tr("Working...", "Enviando..."));
	queue_async_task(run_job, job);
}

void account_like_async(const std::string &video_id, int rating) {
	if (!account_ready()) return;
	AccountJob *j = new AccountJob{LIKE, video_id, "", ""};
	j->number = rating;
	queue_job(j);
}
void account_subscribe_async(const std::string &channel_id, bool subscribe) {
	if (!OAuth::is_authenticated()) return; // local subscriptions keep working without a login
	AccountJob *j = new AccountJob{SUBSCRIBE, channel_id, "", ""};
	j->flag = subscribe;
	queue_job(j);
}
void account_playlists_async(const std::string &video_id) {
	if (!account_ready()) return;
	queue_job(new AccountJob{PLAYLISTS, video_id, "", ""});
}
void account_playlist_edit_async(const std::string &playlist_id, const std::string &video_id, bool add) {
	if (!account_ready()) return;
	AccountJob *j = new AccountJob{PLAYLIST_EDIT, playlist_id, video_id, ""};
	j->flag = add;
	queue_job(j);
}
void account_comment_async(const std::string &comments_token, const std::string &text) {
	if (!account_ready()) return;
	queue_job(new AccountJob{COMMENT, comments_token, text, ""});
}
void account_comment_like_async(const std::string &comments_token, const std::string &comment_id) {
	if (!account_ready()) return;
	queue_job(new AccountJob{COMMENT_LIKE, comments_token, comment_id, ""});
}
void account_comment_reply_async(const std::string &comments_token, const std::string &comment_id, const std::string &text) {
	if (!account_ready()) return;
	queue_job(new AccountJob{COMMENT_REPLY, comments_token, comment_id, text});
}

bool account_playlists_take(std::string &video_id, std::vector<AccountPlaylist> &out) {
	bool ready;
	result_lock.lock();
	ready = playlists_ready;
	if (ready) {
		video_id = playlists_video_id;
		out = playlists_result;
		playlists_ready = false;
	}
	result_lock.unlock();
	return ready && out.size();
}

void translate_async(const std::string &title, const std::string &text) {
	if (text.empty()) return;
	notice_show(tr("Translating...", "Traduciendo..."));
	queue_async_task(run_job, new AccountJob{TRANSLATE, title, text, ""});
}

bool translation_take(std::string &title, std::string &text) {
	bool ready;
	result_lock.lock();
	ready = translation_ready;
	if (ready) {
		title = translation_title;
		text = translation_text;
		translation_ready = false;
	}
	result_lock.unlock();
	return ready;
}
