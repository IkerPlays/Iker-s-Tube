#pragma once
#include <vector>
#include <string>
#include <map>
#include <functional>

struct YouTubeChannelSuccinct {
	std::string name;
	std::string id;
	std::string icon_url;
	std::string subscribers;
	std::string video_num;
	bool is_collaboration = false;
};
struct YouTubeVideoSuccinct {
	std::string url;
	std::string title;
	std::string duration_text;
	std::string publish_date;
	std::string views_str;
	std::string author;
	std::string thumbnail_url;
};
struct YouTubePlaylistSuccinct {
	std::string url;
	std::string title;
	std::string video_count_str;
	std::string thumbnail_url;
};

struct YouTubeSuccinctItem {
	// TODO : use union or std::variant
	enum { VIDEO, CHANNEL, PLAYLIST } type;
	YouTubeVideoSuccinct video;
	YouTubeChannelSuccinct channel;
	YouTubePlaylistSuccinct playlist;

	YouTubeSuccinctItem() = default;
	YouTubeSuccinctItem(YouTubeVideoSuccinct video) : type(VIDEO), video(video) {}
	YouTubeSuccinctItem(YouTubeChannelSuccinct channel) : type(CHANNEL), channel(channel) {}
	YouTubeSuccinctItem(YouTubePlaylistSuccinct playlist) : type(PLAYLIST), playlist(playlist) {}

	// returns channel id in case of type == CHANNEL
	std::string get_url() const { return type == VIDEO ? video.url : type == CHANNEL ? channel.id : playlist.url; }
	std::string get_thumbnail_url() const {
		return type == VIDEO ? video.thumbnail_url : type == CHANNEL ? channel.icon_url : playlist.thumbnail_url;
	}
	std::string get_name() const {
		return type == VIDEO ? video.title : type == CHANNEL ? channel.name : playlist.title;
	}
};

struct YouTubeSearchResult {
	std::string error;
	std::string estimated_result_num;
	std::vector<YouTubeSuccinctItem> results;

	std::string continue_token;

	bool has_more_results() const { return continue_token != ""; }
	void load_more_results();
};
YouTubeSearchResult youtube_load_search(std::string url);

struct YouTubeVideoDetail {
	std::string error;
	std::string url;
	std::string title;
	std::string description;
	YouTubeChannelSuccinct author;
	bool metadata_from_android_vr = false;
	std::string id;
	std::string succinct_thumbnail_url;
	std::string audio_stream_url;
	struct AudioTrack { // dubbed / original audio of the video (only when YouTube offers several)
		std::string name, lang; // "Spanish (Latin America)", "es"
		std::string url;
		bool selected = false;
	};
	std::vector<AudioTrack> audio_tracks;
	std::map<int, std::string> video_stream_urls; // first : video size (144p, 240p, 360p ...)
	std::string both_stream_url;
	int duration_ms = 0;
	bool is_livestream = false;
	enum class LivestreamType {
		PREMIERE,
		LIVESTREAM,
	};
	LivestreamType livestream_type;
	bool is_upcoming = false;
	long long upcoming_start_time = 0; // unix time of a scheduled premiere / live stream (0 : unknown)
	std::string playability_status;
	std::string playability_reason;
	int stream_fragment_len = 0; // used only for livestreams
	std::string like_count_str;
	std::string dislike_count_str;
	std::string publish_date;
	std::string views_str;

	// caption-related data
	struct CaptionBaseLanguage {
		std::string name;     // e.g. "English", "Japanese"
		std::string id;       // e.g. "en", "ja"
		std::string base_url; // empty string for instances in caption_translated_languages
		bool is_translatable;
	};
	struct CaptionTranslationLanguage {
		std::string name;
		std::string id;
	};
	std::vector<CaptionBaseLanguage> caption_base_languages;
	std::vector<CaptionTranslationLanguage> caption_translation_languages;
	struct CaptionPiece {
		float start_time;
		float end_time;
		std::string content;
	};
	using Caption = std::vector<CaptionPiece>;
	std::map<std::pair<std::string, std::string>, Caption> caption_data;

	std::vector<YouTubeSuccinctItem> suggestions;
	struct Playlist {
		std::string id;
		std::string title;
		std::string author_name;
		int total_videos;
		std::vector<YouTubeVideoSuccinct> videos;
		int selected_index;
	};
	Playlist playlist;

	struct Comment {
		std::string error;
		YouTubeChannelSuccinct author;
		std::string content;
		std::string id;
		std::string publish_date;
		std::string upvotes_str;
		std::string page_token; // the continuation token of the comment page this comment came from (for the account actions)
		bool pinned = false;  // pinned by the creator
		bool hearted = false; // the creator gave it a heart

		int reply_num;
		std::vector<Comment> replies;
		std::string replies_continue_token;
		bool has_more_replies() const { return replies_continue_token != ""; }
		void load_more_replies();
	};
	std::vector<Comment> comments;

	std::string suggestions_continue_token;
	std::string comment_continue_token;
	int comment_continue_type = -1; // -1 : unavailable, 0 : using watch_comments, 1 : using innertube
	bool comments_disabled = false;
	std::string storyboard_spec; // preview thumbnails of the seek bar (empty if YouTube does not offer them)
	std::string comment_first_token; // the first comments continuation token (used for account actions when no sort token exists)
	std::string comment_sort_tokens[2]; // 0 : top comments, 1 : newest first (empty if unknown)
	int comment_sort_type = 0;

	bool has_next_video() const {
		if (playlist.videos.size() && playlist.selected_index != (int)playlist.videos.size() - 1) {
			return true;
		}
		for (auto suggestion : suggestions) {
			if (suggestion.type == YouTubeSuccinctItem::VIDEO) {
				return true;
			}
		}
		return false;
	}
	bool has_next_video_in_playlist() const {
		return playlist.videos.size() && playlist.selected_index != (int)playlist.videos.size() - 1;
	}
	YouTubeVideoSuccinct get_next_video() const {
		if (playlist.videos.size() && playlist.selected_index != (int)playlist.videos.size() - 1) {
			return playlist.videos[std::max(0, playlist.selected_index + 1)];
		}
		for (auto suggestion : suggestions) {
			if (suggestion.type == YouTubeSuccinctItem::VIDEO) {
				return suggestion.video;
			}
		}
		return YouTubeVideoSuccinct();
	}
	bool has_more_suggestions() const { return suggestions_continue_token != ""; }
	std::string comments_account_token() const { return comment_sort_tokens[0] != "" ? comment_sort_tokens[0] : comment_first_token; }
	bool has_more_comments() const { return comment_continue_type != -1; }
	bool needs_timestamp_adjusting() const { return is_livestream && livestream_type == LivestreamType::PREMIERE; }
	bool is_playable() const {
		return playability_status == "OK" &&
		       (both_stream_url != "" || (audio_stream_url != "" && video_stream_urls.size()));
	}

	void load_more_suggestions();
	void load_more_comments();
	void load_caption(const std::string &base_lang_id, const std::string &translation_lang_id);
};
// this function does not load comments; call youtube_video_page_load_more_comments() if necessary
YouTubeVideoDetail youtube_load_video_page(std::string url);
// the channels of a collaboration video (all of them, with their subscribers and icon). Blocks on the network (a big answer).
bool youtube_load_collaborators(const std::string &video_id, std::vector<YouTubeChannelSuccinct> &out, std::string &error);

struct YouTubeChannelDetail {
	std::string id;
	std::string error;
	std::string name;
	std::string handle;
	std::string url;
	std::string url_original;
	std::string icon_url;
	std::string banner_url;
	std::string description;
	std::string subscriber_count_str;
	std::vector<YouTubeVideoSuccinct> videos;
	std::vector<YouTubeVideoSuccinct> streams;
	std::vector<YouTubeVideoSuccinct> shorts;

	std::string continue_token;
	std::string videos_continue_token;
	std::string streams_continue_token;
	std::string shorts_continue_token;
	std::string video_sort_token_newest;
	std::string video_sort_token_popular;
	std::string video_sort_token_oldest;
	int current_video_sort_type = 0; // 0: newest, 1: popular, 2: oldest
	std::string streams_sort_token_newest;
	std::string streams_sort_token_popular;
	std::string streams_sort_token_oldest;
	int current_streams_sort_type = 0; // 0: newest, 1: popular, 2: oldest
	std::string shorts_sort_token_newest;
	std::string shorts_sort_token_popular;
	std::string shorts_sort_token_oldest;
	int current_shorts_sort_type = 0; // 0: newest, 1: popular, 2: oldest
	std::string playlist_tab_browse_id;
	std::string playlist_tab_params;

	std::vector<std::pair<std::string, std::vector<YouTubePlaylistSuccinct>>>
	    playlists; // {category title, list of playlists}

	struct CommunityPost {
		std::string author_name;
		std::string author_icon_url;
		std::string time;
		std::string upvotes_str;
		std::string message;
		std::string image_url; // "" if no image
		std::string poll_total_votes;
		std::vector<std::string> poll_choices;
		YouTubeVideoSuccinct video;
	};
	std::vector<CommunityPost> community_posts;
	bool community_loaded = false;
	std::string community_continuation_token;

	bool streams_loaded = false;
	bool shorts_loaded = false;

	bool has_more_videos() const { return videos_continue_token != ""; }
	bool has_more_streams() const { return streams_continue_token != ""; }
	bool has_more_shorts() const { return shorts_continue_token != ""; }
	bool has_playlists_to_load() const { return playlist_tab_browse_id != "" && playlist_tab_params != ""; }
	bool has_community_posts_to_load() const { return !community_loaded || community_continuation_token != ""; }

	void load_more_videos();
	void load_more_streams();
	void load_more_shorts();
	void load_playlists();
	void load_more_community_posts();
};
YouTubeChannelDetail youtube_load_channel_page(std::string url_or_id);
YouTubeChannelDetail youtube_load_channel_streams_page(std::string url_or_id);
YouTubeChannelDetail youtube_load_channel_shorts_page(std::string url_or_id);
std::vector<YouTubeChannelDetail> youtube_load_channel_page_multi(std::vector<std::string> ids,
                                                                  std::function<void(int, int)> progress);

struct YouTubeHomeResult {
	std::string error;
	std::vector<YouTubeVideoSuccinct> videos;
	std::string continue_token;
	std::string visitor_data;
	bool has_more_results() const { return continue_token != "" && visitor_data != ""; }
	void load_more_results();
};
YouTubeHomeResult youtube_load_home_page();

void youtube_change_content_language(std::string language_code);

/* -------------------------------- utils.cpp -------------------------------- */
std::string youtube_get_video_id_by_url(const std::string &url);
std::string youtube_get_playlist_id_by_url(const std::string &url);
std::string youtube_get_video_thumbnail_url_by_id(const std::string &id);
std::string youtube_get_video_url_by_id(const std::string &id);
// set to true to make a running youtube_load_video_page() give up at its next checkpoint (a newer request is waiting)
extern volatile bool youtube_load_cancel;
// "params" of the search request (type / upload date / duration filter), empty : no filter
extern std::string youtube_search_filter_params;
std::string get_video_id_from_thumbnail_url(const std::string &url);
std::string convert_webp_thumbnail_to_jpg(const std::string &url);
bool youtube_is_valid_video_id(const std::string &id);
bool is_youtube_url(const std::string &url);
bool is_youtube_thumbnail_url(const std::string &url);
int64_t extract_stream_length(const std::string &url);

enum class YouTubePageType { VIDEO, CHANNEL, SEARCH, INVALID };
YouTubePageType youtube_get_page_type(std::string url);

struct YouTubeDislikeData {
	std::string id;
	int64_t likes;
	int64_t dislikes;
	double rating;
	int64_t view_count;
};

// The language the audio should be in when the video has several tracks ("" : the original one). Kept in sdmc:/3ds/IkersTube/audio_lang.txt.
std::string youtube_audio_lang_get();
void youtube_audio_lang_set(const std::string &lang);
