#include "headers.hpp"
#include "scenes/account_tasks.hpp"
#include "oauth/oauth.hpp"
#include "ui/notice.hpp"
#include "util/async_task.hpp"
#include "data_io/string_resource.hpp"
#include "youtube_parser/translate.hpp"
#include "youtube_parser/live_chat.hpp"

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

enum { LIKE, SUBSCRIBE, PLAYLISTS, PLAYLIST_EDIT, COMMENT, COMMENT_LIKE, COMMENT_REPLY, TRANSLATE, COMMENT_DELETE, NOT_INTERESTED, BELL, LIVE_CHAT, POST_LIKE, RUN_ENDPOINT };

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
	case NOT_INTERESTED:
		ok = youtube_account_not_interested(job->a, job->flag);
		done = job->flag ? tr("You will not be recommended this channel", "No se te recomendará este canal") : tr("Noted: not interested", "Anotado: no me interesa");
		failed = tr("Could not send it", "No se pudo enviar");
		break;
	case POST_LIKE:
		ok = youtube_account_post_like(job->a);
		done = tr("Post liked", "Publicación con Me gusta");
		failed = tr("Could not like the post", "No se pudo dar Me gusta a la publicación");
		break;
	case RUN_ENDPOINT:
		ok = youtube_account_run_endpoint(job->a);
		done = tr("Vote sent", "Voto enviado");
		failed = tr("Could not send the vote", "No se pudo enviar el voto");
		break;
	case BELL:
		ok = youtube_account_channel_bell(job->a, job->number);
		done = job->number == 0 ? tr("Notifications: all", "Notificaciones: todas") : job->number == 1 ? tr("Notifications: personalized", "Notificaciones: personalizadas") : tr("Notifications: none", "Notificaciones: ninguna");
		failed = tr("Could not change the bell", "No se pudo cambiar la campana");
		break;
	case LIVE_CHAT: {
		std::string error;
		ok = live_chat_send(job->a, job->b, error);
		if (!ok) youtube_account_set_error(error);
		done = tr("Message sent", "Mensaje enviado");
		failed = tr("Could not send the message (Studio login needed)", "No se pudo enviar (hace falta el login de Studio)");
		break;
	}
	case COMMENT_DELETE:
		ok = youtube_account_comment_delete(job->a, job->b);
		done = tr("Comment deleted", "Comentario borrado");
		failed = tr("Could not delete the comment", "No se pudo borrar el comentario");
		break;
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
		if (why != "" && (job->type == COMMENT || job->type == COMMENT_REPLY || job->type == COMMENT_DELETE || job->type == LIVE_CHAT)) {
			logger.warning("account", "job failed : " + why);
			result_lock.lock(); // the whole reason on a screen, to read it and send it
			translation_title = tr("Why it failed", "Por qué falló");
			translation_text = why;
			translation_ready = true;
			result_lock.unlock();
			var_need_refresh = true;
		}
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
void account_not_interested_async(const std::string &video_id, bool whole_channel) {
	if (!account_ready()) return;
	AccountJob *j = new AccountJob{NOT_INTERESTED, video_id, "", ""};
	j->flag = whole_channel;
	queue_job(j);
}
void account_bell_async(const std::string &channel_id, int level) {
	if (!account_ready()) return;
	AccountJob *j = new AccountJob{BELL, channel_id, "", ""};
	j->number = level;
	queue_job(j);
}
void account_post_like_async(const std::string &post_id) {
	if (!account_ready()) return;
	queue_job(new AccountJob{POST_LIKE, post_id, "", ""});
}
void account_run_endpoint_async(const std::string &endpoint_json) {
	if (!account_ready()) return;
	queue_job(new AccountJob{RUN_ENDPOINT, endpoint_json, "", ""});
}
void account_live_chat_async(const std::string &video_id, const std::string &text) {
	if (!account_ready()) return;
	queue_job(new AccountJob{LIVE_CHAT, video_id, text, ""});
}
void account_comment_delete_async(const std::string &comments_token, const std::string &comment_id) {
	if (!account_ready()) return;
	queue_job(new AccountJob{COMMENT_DELETE, comments_token, comment_id, ""});
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
