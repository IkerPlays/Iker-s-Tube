#include <regex>
#include "internal_common.hpp"
#include "parser.hpp"

std::string youtube_search_filter_params;

// ReelSequence protobuf : short id (1), params {number = 5} (5), feature 25 (10), feature 0 (13) ; sent as standard base64
std::vector<std::string> youtube_shorts_sequence(const std::string &seed_video_id) {
	std::vector<std::string> ids;
	if (seed_video_id.size() != 11) return ids;
	std::string raw;
	raw += '\x0A';
	raw += (char)seed_video_id.size();
	raw += seed_video_id;
	raw += "\x2A\x02\x18\x05"; // params { number = 5 }
	raw += "\x50\x19";         // feature_2 = 25
	raw.append("\x68\x00", 2); // feature_3 = 0 (the zero byte needs the length : a C string would stop there)
	static const char *tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string b64;
	for (size_t i = 0; i < raw.size(); i += 3) {
		unsigned v = ((unsigned char)raw[i]) << 16;
		if (i + 1 < raw.size()) v |= ((unsigned char)raw[i + 1]) << 8;
		if (i + 2 < raw.size()) v |= (unsigned char)raw[i + 2];
		b64 += tbl[(v >> 18) & 63];
		b64 += tbl[(v >> 12) & 63];
		b64 += i + 1 < raw.size() ? tbl[(v >> 6) & 63] : '=';
		b64 += i + 2 < raw.size() ? tbl[v & 63] : '=';
	}
	std::string body = R"({"context": {"client": {"hl": "%0","gl": "%1","clientName": "WEB","clientVersion": "2.20241126.01.00"}}, "sequenceParams": "%2"})";
	body = std::regex_replace(body, std::regex("%0"), language_code);
	body = std::regex_replace(body, std::regex("%1"), country_code);
	body = std::regex_replace(body, std::regex("%2"), b64);
	auto r = http_post_json(get_innertube_api_url("reel/reel_watch_sequence"), body);
	if (!r.first) {
		logger.warning("shorts", "sequence failed : " + r.second.substr(0, 100));
		return ids;
	}
	std::regex re("\"videoId\"\\s*:\\s*\"([A-Za-z0-9_-]{11})\"");
	for (auto it = std::sregex_iterator(r.second.begin(), r.second.end(), re); it != std::sregex_iterator(); ++it) {
		std::string id = (*it)[1].str();
		if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
	}
	logger.info("shorts", "sequence gave " + std::to_string(ids.size()) + " shorts");
	return ids;
}

static bool parse_searched_item(RJson content, std::vector<YouTubeSuccinctItem> &res) {
	bool success = true;

	if (content.has_key("compactVideoRenderer") || content.has_key("videoWithContextRenderer")) {
		auto video_content = content.has_key("compactVideoRenderer") ? content["compactVideoRenderer"]
		                                                             : content["videoWithContextRenderer"];
		res.push_back(YouTubeSuccinctItem(parse_succinct_video(video_content)));
	} else if (content.has_key("compactChannelRenderer")) {
		auto channel_renderer = content["compactChannelRenderer"];
		YouTubeChannelSuccinct cur_result;

		if (channel_renderer.has_key("navigationEndpoint") &&
		    channel_renderer["navigationEndpoint"].has_key("browseEndpoint") &&
		    channel_renderer["navigationEndpoint"]["browseEndpoint"].has_key("browseId")) {
			cur_result.id = channel_renderer["navigationEndpoint"]["browseEndpoint"]["browseId"].string_value();
		} else {
			debug_error("Error: Missing browseId");
			success = false;
		}

		cur_result.name = get_text_from_object(channel_renderer["displayName"]);
		cur_result.subscribers = get_text_from_object(channel_renderer["subscriberCountText"]);
		cur_result.video_num = get_text_from_object(channel_renderer["videoCountText"]);
		cur_result.icon_url = get_thumbnail_url_closest(channel_renderer["thumbnail"]["thumbnails"], 70);
		res.push_back(YouTubeSuccinctItem(cur_result));
	} else if (content.has_key("compactRadioRenderer") || content.has_key("compactPlaylistRenderer")) {
		auto playlist_renderer = content.has_key("compactRadioRenderer") ? content["compactRadioRenderer"]
		                                                                 : content["compactPlaylistRenderer"];

		YouTubePlaylistSuccinct cur_list;
		cur_list.title = get_text_from_object(playlist_renderer["title"]);
		cur_list.video_count_str = get_text_from_object(playlist_renderer["videoCountText"]);
		for (auto thumbnail : playlist_renderer["thumbnail"]["thumbnails"].array_items()) {
			if (thumbnail.has_key("url") && thumbnail["url"].string_value().find("/default.jpg") != std::string::npos) {
				cur_list.thumbnail_url = thumbnail["url"].string_value();
			}
		}

		cur_list.url = convert_url_to_mobile(playlist_renderer["shareUrl"].string_value());
		if (!starts_with(cur_list.url, "https://m.youtube.com/watch", 0)) {
			if (starts_with(cur_list.url, "https://m.youtube.com/playlist?", 0)) {
				auto params = parse_parameters(
				    cur_list.url.substr(std::string("https://m.youtube.com/playlist?").size(), cur_list.url.size()));
				auto playlist_id = params["list"];
				auto video_id = get_video_id_from_thumbnail_url(cur_list.thumbnail_url);
				cur_list.url = "https://m.youtube.com/watch?v=" + video_id + "&list=" + playlist_id;
			} else {
				debug_warning("unknown playlist url");
				success = false;
			}
		}

		res.push_back(YouTubeSuccinctItem(cur_list));
	} else if (content.has_key("reelShelfRenderer")) {
		debug_warning("Skipped reelShelfRenderer");
	} else if (content.has_key("showingResultsForRenderer")) {
		debug_warning("Skipped showingResultsForRenderer");
	} else if (content.has_key("gridShelfViewModel")) {
		debug_warning("Skipped gridShelfViewModel");
	} else {
		debug_warning("Error: Unexpected content structure");
		success = false;
	}

	return success;
}

YouTubeSearchResult youtube_load_search(std::string url) {
	YouTubeSearchResult res;
	res.error = "";

	int skipped_items = 0;
	bool success = true;

	std::string query_word;
	auto pos = url.find("?search_query=");
	if (pos == std::string::npos) {
		pos = url.find("&search_query=");
	}
	if (pos != std::string::npos) {
		size_t head = pos + std::string("?search_query=").size();
		while (head < url.size() && url[head] != '&') {
			query_word.push_back(url[head++]);
		}
	}
	url = convert_url_to_mobile(url);
	std::string new_query_word;
	for (size_t i = 0; i < query_word.size();) {
		auto hex_to_int = [](char c) {
			if (isdigit(c)) {
				return c - '0';
			}
			if (isupper(c)) {
				return c - 'A' + 10;
			}
			if (islower(c)) {
				return c - 'a' + 10;
			}
			return 0;
		};
		if (query_word[i] == '%' && i + 2 < query_word.size()) {
			new_query_word.push_back(hex_to_int(query_word[i + 1]) * 16 + hex_to_int(query_word[i + 2]));
			i += 3;
		} else {
			new_query_word.push_back(query_word[i++]);
		}
	}
	{ // the word goes inside a JSON string: quotes, backslashes and control characters are escaped
		std::string escaped;
		for (unsigned char c : new_query_word) {
			if (c == '"' || c == '\\') escaped += '\\', escaped += (char)c;
			else if (c < 0x20) escaped += ' ';
			else escaped += (char)c;
		}
		new_query_word = escaped;
	}
	query_word = new_query_word;

	std::string post_content =
	    R"({"context": {"client": {"hl": "%0", "gl": "%1", "clientName": "MWEB", "clientVersion": "2.20241202.07.00"}}, "query": ")" +
	    query_word + "\"";
	if (youtube_search_filter_params.size()) {
		post_content += ", \"params\": \"" + youtube_search_filter_params + "\"";
	}
	post_content += "}";
	post_content = std::regex_replace(post_content, std::regex("%0"), language_code);
	post_content = std::regex_replace(post_content, std::regex("%1"), country_code);

	access_and_parse_json(
	    [&]() { return http_post_json(get_innertube_api_url("search"), post_content); },
	    [&](Document &, RJson yt_result) {
		    if (yt_result.has_key("estimatedResults")) {
			    res.estimated_result_num = yt_result["estimatedResults"].string_value();
		    } else {
			    res.estimated_result_num = "unknown";
		    }

		    res.continue_token = "";
		    if (yt_result.has_key("contents") && yt_result["contents"].has_key("sectionListRenderer") &&
		        yt_result["contents"]["sectionListRenderer"].has_key("contents")) {
			    for (auto i : yt_result["contents"]["sectionListRenderer"]["contents"].array_items()) {
				    if (i.has_key("itemSectionRenderer")) {
					    for (auto j : i["itemSectionRenderer"]["contents"].array_items()) {
						    if (j.has_key("didYouMeanRenderer")) {
							    debug_warning("Skipped didYouMeanRenderer");
							    continue;
						    }
						    if (j.has_key("horizontalCardListRenderer")) {
							    debug_warning("Skipped horizontalCardListRenderer");
							    continue;
						    }
						    if (!parse_searched_item(j, res.results)) {
							    debug_error("Error parsing search result item"); // (an item of a kind that is not known is skipped, the rest is fine)
							    skipped_items++;
						    }
					    }
				    }
				    if (i.has_key("continuationItemRenderer")) {
					    res.continue_token =
					        i["continuationItemRenderer"]["continuationEndpoint"]["continuationCommand"]["token"]
					            .string_value();
				    }
			    }
		    } else {
			    debug_caution("unexpected result structure");
			    success = false;
		    }
		    if (res.continue_token == "") {
			    debug_caution("failed to get next continue token");
		    }
	    },
	    [&](const std::string &error) {
		    res.error = "[se] " + error;
		    success = false;
	    });

	if (!success || (skipped_items > 0 && res.results.empty())) {
		res.error = "[se] Unknown error occurred during search load";
	}

	return res;
}

void YouTubeSearchResult::load_more_results() {
	if (continue_token == "") {
		error = "continue token empty";
		return;
	}

	// POST to get more results
	std::string post_content =
	    R"({"context": {"client": {"hl": "%0", "gl": "%1", "clientName": "MWEB", "clientVersion": "2.20241202.07.00", "utcOffsetMinutes": 0}, "request": {}, "user": {}}, "continuation": ")" +
	    continue_token + "\"}";
	post_content = std::regex_replace(post_content, std::regex("%0"), language_code);
	post_content = std::regex_replace(post_content, std::regex("%1"), country_code);

	access_and_parse_json(
	    [&]() { return http_post_json(get_innertube_api_url("search"), post_content); },
	    [&](Document &, RJson yt_result) {
		    estimated_result_num = yt_result["estimatedResults"].string_value();
		    continue_token = "";
		    for (auto i : yt_result["onResponseReceivedCommands"].array_items()) {
			    for (auto j : i["appendContinuationItemsAction"]["continuationItems"].array_items()) {
				    if (j.has_key("itemSectionRenderer")) {
					    for (auto item : j["itemSectionRenderer"]["contents"].array_items()) {
						    parse_searched_item(item, results);
					    }
				    } else if (j.has_key("continuationItemRenderer")) {
					    continue_token =
					        j["continuationItemRenderer"]["continuationEndpoint"]["continuationCommand"]["token"]
					            .string_value();
				    }
			    }
		    }
		    if (continue_token == "") {
			    debug_caution("failed to get next continue token");
		    }
	    },
	    [&](const std::string &error) { debug_error((this->error = "[se+] " + error)); });
}
