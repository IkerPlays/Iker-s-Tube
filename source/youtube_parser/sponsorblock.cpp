#include "headers.hpp"
#include "youtube_parser/sponsorblock.hpp"
#include "youtube_parser/internal_common.hpp"
#include <mbedtls/sha256.h>
#include <mbedtls/version.h>
#include <cstdlib>

static std::string hash_prefix(const std::string &text) {
	unsigned char digest[32];
#if MBEDTLS_VERSION_MAJOR >= 3
	mbedtls_sha256((const unsigned char *)text.data(), text.size(), digest, 0);
#else
	mbedtls_sha256_ret((const unsigned char *)text.data(), text.size(), digest, 0);
#endif
	char hex[9];
	snprintf(hex, sizeof hex, "%02x%02x%02x%02x", digest[0], digest[1], digest[2], digest[3]);
	return std::string(hex).substr(0, 4);
}

bool sponsorblock_load(const std::string &video_id, int level, std::vector<std::pair<double, double>> &segments) {
	if (video_id.size() != 11 || level <= 0) return true;
	std::string categories = level == 1 ? "[\"sponsor\"]" : "[\"sponsor\",\"selfpromo\",\"interaction\",\"intro\",\"outro\"]";
	std::string url = "https://sponsor.ajay.app/api/skipSegments/" + hash_prefix(video_id) + "?categories=";
	for (char c : categories) { // percent-encode what is not plain
		if (isalnum((unsigned char)c)) url += c;
		else {
			char buf[4];
			snprintf(buf, sizeof buf, "%%%02X", (unsigned char)c);
			url += buf;
		}
	}
	auto response = http_get(url);
	if (!response.first) {
		return false;
	}
	const std::string &json = response.second;
	size_t pos = json.find("\"videoID\":\"" + video_id + "\"");
	if (pos == std::string::npos) return true; // other videos with the same prefix only
	size_t end = json.find("\"videoID\":\"", pos + 20);
	if (end == std::string::npos) end = json.size();
	while ((pos = json.find("\"segment\":[", pos)) != std::string::npos && pos < end) {
		pos += 11;
		char *after = NULL;
		double start = strtod(json.c_str() + pos, &after);
		if (!after || *after != ',') continue;
		double stop = strtod(after + 1, NULL);
		// only the segments that are skipped (the "mute" and "full video" ones have another action type)
		size_t action = json.rfind("\"actionType\":\"", pos);
		bool skip = action == std::string::npos || json.compare(action + 14, 4, "skip") == 0;
		if (skip && stop > start) segments.push_back({start, stop});
	}
	return true;
}
