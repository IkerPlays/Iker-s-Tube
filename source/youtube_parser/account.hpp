#pragma once
#include <string>
#include <vector>
#include "youtube_parser/parser.hpp"

// Actions on the signed-in YouTube account (OAuth login needed). They block on the network : call them from a worker thread.
// All of them return false when there is no login or YouTube refused the request.
struct AccountPlaylist {
	std::string id;
	std::string title;
	bool has_video = false; // the video is already in this playlist
};

// the videos YouTube recommends to the account and the videos of its notification inbox (title + url + author when known)
bool youtube_account_recommendations(std::vector<YouTubeVideoSuccinct> &out);
bool youtube_account_notifications(std::vector<YouTubeVideoSuccinct> &out);

bool youtube_account_like(const std::string &video_id, int rating); // 1 like, -1 dislike, 0 remove the rating
bool youtube_account_subscribe(const std::string &channel_id, bool subscribe);
bool youtube_account_playlists(const std::string &video_id, std::vector<AccountPlaylist> &out);
// creates an empty playlist of the account ; privacy : "PRIVATE", "UNLISTED" or "PUBLIC"
bool youtube_account_playlist_create(const std::string &title, const std::string &privacy, std::string &playlist_id);
bool youtube_account_playlist_edit(const std::string &playlist_id, const std::string &video_id, bool add);
// comments : `comments_token` is a continuation token of the comment section of the video (comment_sort_tokens[0])
bool youtube_account_comment_create(const std::string &comments_token, const std::string &text);
bool youtube_account_comment_like(const std::string &comments_token, const std::string &comment_id);
// deletes a comment of the signed-in user (best effort : the delete action is looked for in the menu of the comment)
bool youtube_account_comment_delete(const std::string &comments_token, const std::string &comment_id);
bool youtube_account_comment_reply(const std::string &comments_token, const std::string &comment_id,
                                   const std::string &text);

// why the last request failed (shown next to the error notices), and the video the comment actions work on
std::string youtube_account_last_error();
void youtube_account_set_video(const std::string &video_id);
