#include <string.h>
#include "headers.hpp"
#include "system/report.hpp"
#include "oauth/oauth.hpp"
#include "data_io/offline.hpp"
#include "youtube_parser/studio_auth.hpp"
#include "util/play_queue.hpp"
#include "youtube_parser/account.hpp"
#include "util/util.hpp"
#include "data_io/history.hpp"
#include "data_io/user_playlists.hpp"
#include "data_io/watch_later.hpp"
#include "data_io/favorites.hpp"
#include <cstdio>
#include <sys/stat.h>

#ifndef IKT_VERSION
#define IKT_VERSION "?"
#endif

// the last `max_bytes` of a file, copied to `out`
static void append_tail(FILE *out, const char *title, const char *path, long max_bytes) {
	fprintf(out, "\n===== %s =====\n", title);
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(out, "(none)\n");
		return;
	}
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	long start = size > max_bytes ? size - max_bytes : 0;
	fseek(f, start, SEEK_SET);
	// line by line: a line that may carry a login (tokens, cookies, secrets, e-mail addresses) is replaced before it is written
	char line[1100];
	static const char *private_words[] = {"Bearer", "access_token", "refresh_token", "client_secret", "Authorization", "SAPISID", "cookie", "Cookie", "id_token", "@gmail", "@outlook", "@hotmail", "@yahoo"};
	bool hiding_rest_of_line = false; // a long line comes in several pieces: once a piece is hidden, the rest of that line is too
	while (fgets(line, sizeof line, f)) {
		bool hide = hiding_rest_of_line;
		for (const char *word : private_words) {
			if (strstr(line, word)) hide = true;
		}
		bool line_ends = strchr(line, '\n') != NULL;
		hiding_rest_of_line = hide && !line_ends;
		if (hide) {
			if (line_ends) fputs("[line hidden: it may contain private data]\n", out); // (one note per hidden line)
		} else {
			fputs(line, out);
		}
	}
	fclose(f);
}

bool report_create() {
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	FILE *f = fopen("sdmc:/3ds/IkersTube/report.txt", "w");
	if (!f) return false;
	fprintf(f, "Iker's Tube report\n");
	fprintf(f, "version: %s\n", IKT_VERSION);
	fprintf(f, "console: %s, new3ds=%d, model=%d, wifi_state=%d\n", var_is_new3ds ? "New 3DS family" : "Old 3DS family", (int)var_is_new3ds,
	        (int)var_model, (int)var_wifi_state);
	fprintf(f, "language: %s (content %s)\n", var_lang.c_str(), var_lang_content.c_str());
	fprintf(f, "memory free (application): %lu KB, linear: %lu KB\n", (unsigned long)(osGetMemRegionFree(MEMREGION_APPLICATION) / 1024),
	        (unsigned long)(linearSpaceFree() / 1024));
	fprintf(f, "video quality=%d, auto quality=%d, data saver=%d, grid=%d, sponsorblock=%d, incognito=%d\n", (int)var_video_quality,
	        (int)var_auto_quality, (int)var_data_saver, (int)var_grid_view, (int)var_sponsorblock, (int)var_incognito);
	fprintf(f, "account (OAuth): %s, channel id known: %s\n", OAuth::is_authenticated() ? "signed in" : "not signed in",
	        OAuth::get_user_channel_id() != "" ? "yes" : "no");
	fprintf(f, "studio client: %s, studio login: %s\n", studio_auth_has_client() ? (studio_auth_client_built_in() ? "built in" : "set up here") : "none",
	        studio_auth_signed_in() ? "signed in" : "no");
	fprintf(f, "last account error: %s\n", youtube_account_last_error().c_str());
	fprintf(f, "audio probe: %s\n", youtube_audio_probe_info_get().c_str());
	fprintf(f, "sd card free: %lu MB, downloaded this session: %lu KB\n", sd_free_mb(), (unsigned long)(var_data_session_bytes / 1024));
	fprintf(f, "settings: autoplay=%d, auto captions=%d, caption size=%d, seek step=%d, autoplay limit=%d, break reminder=%d, library tab=%d\n", (int)var_autoplay_level,
	        (int)var_auto_captions, (int)var_caption_size, (int)var_seek_step, (int)var_autoplay_limit, (int)var_break_reminder, (int)var_library_tab);
	fprintf(f, "more: theme=%d, start scene=%d, text scale=%d, history=%d, hide shorts=%d, download quality=%d, player response=%d, eco=%d, build=%s\n", (int)var_theme,
	        (int)var_start_scene, (int)var_text_scale, (int)var_history_enabled, (int)var_hide_shorts, (int)var_download_quality, (int)var_player_response, (int)var_eco_mode,
	        DEF_BUILD_TIME);
	fprintf(f, "safe mode: %s, settings.txt.crashed: %s\n", var_safe_mode ? "yes (the last start did not finish)" : "no", access("sdmc:/3ds/IkersTube/settings.txt.crashed", F_OK) == 0 ? "exists" : "no");
	fprintf(f, "saved items: %d, queue: %d\n", (int)offline_list().size(), play_queue_size());
	fprintf(f, "lists: favorites=%d, watch later=%d, playlists=%d, history=%d\n", (int)favorites_get().size(), (int)watch_later_get().size(), (int)playlists_get().size(), (int)get_valid_watch_history().size());
	append_tail(f, "crash_last.txt", "sdmc:/3ds/IkersTube/crash_last.txt", 4000);
	append_tail(f, "log.txt (end)", "sdmc:/3ds/IkersTube/log.txt", 24000);
	append_tail(f, "log_previous.txt (end)", "sdmc:/3ds/IkersTube/log_previous.txt", 8000); // (the log of the run before this one)
	append_tail(f, "crash.txt (end)", "sdmc:/3ds/IkersTube/crash.txt", 4000);
	append_tail(f, "settings.txt", "sdmc:/3ds/IkersTube/settings.txt", 4000);
	fclose(f);
	return true;
}
