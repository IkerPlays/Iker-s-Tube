#pragma once
#include <string>
#include <vector>
#include "youtube_parser/account.hpp"

// Background versions of the account actions : they show the result with notice_show().
bool account_ready(); // false (and a notice is shown) when there is no OAuth login
void account_like_async(const std::string &video_id, int rating);
void account_subscribe_async(const std::string &channel_id, bool subscribe);
void account_playlists_async(const std::string &video_id); // fills the list below when done
void account_playlist_edit_async(const std::string &playlist_id, const std::string &video_id, bool add);
void account_comment_async(const std::string &comments_token, const std::string &text);
void account_comment_like_async(const std::string &comments_token, const std::string &comment_id);
void account_comment_delete_async(const std::string &comments_token, const std::string &comment_id);
void account_comment_reply_async(const std::string &comments_token, const std::string &comment_id, const std::string &text);

// translation of a text into the language of the app (Google Translate web endpoint)
void translate_async(const std::string &title, const std::string &text);
bool translation_take(std::string &title, std::string &text); // true once when a result is ready

// result of account_playlists_async : taken by the UI thread
bool account_playlists_take(std::string &video_id, std::vector<AccountPlaylist> &out);
