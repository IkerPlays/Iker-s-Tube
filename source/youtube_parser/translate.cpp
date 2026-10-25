#include "headers.hpp"
#include "youtube_parser/translate.hpp"
#include "internal_common.hpp"

static std::string target_language() {
	if (var_lang == "br") return "pt";
	if (var_lang == "ja" || var_lang == "de" || var_lang == "fr" || var_lang == "it" || var_lang == "es") return var_lang;
	return "en";
}

static std::string url_encode_text(const std::string &s) {
	static const char *hex = "0123456789ABCDEF";
	std::string out;
	for (unsigned char c : s) {
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			out += (char)c;
		} else {
			out += '%';
			out += hex[c >> 4];
			out += hex[c & 15];
		}
	}
	return out;
}

// one request : the answer is [[["translated part","original part",...], ...], ...]
static bool translate_chunk(const std::string &text, std::string &out) {
	std::string url = "https://translate.googleapis.com/translate_a/single?client=gtx&sl=auto&tl=" + target_language() +
	                  "&dt=t&q=" + url_encode_text(text);
	auto response = http_get(url);
	if (!response.first || response.second.empty()) {
		return false;
	}
	rapidjson::Document document;
	std::string error;
	std::vector<char> data(response.second.begin(), response.second.end());
	data.push_back('\0');
	RJson root = RJson::parse(document, data.data(), error);
	if (!root.is_valid()) {
		return false;
	}
	std::string result;
	for (auto part : root[(size_t)0].array_items()) {
		result += part[(size_t)0].string_value();
	}
	if (result.empty()) {
		return false;
	}
	out += result;
	return true;
}

bool translate_text(const std::string &text, std::string &out) {
	out.clear();
	// the endpoint takes a limited amount of text per request : cut at line ends into chunks of about 1200 bytes
	const size_t max_chunk = 1200;
	const size_t max_total = 4800;
	std::string chunk;
	size_t used = 0, pos = 0;
	bool any = false;
	while (pos < text.size() && used < max_total) {
		size_t nl = text.find('\n', pos);
		std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos + 1);
		pos = nl == std::string::npos ? text.size() : nl + 1;
		if (line.size() > max_chunk) {
			line.resize(max_chunk);
			while (!line.empty() && ((unsigned char)line.back() & 0xC0) == 0x80) line.pop_back(); // (not in the middle of a letter)
			if (!line.empty() && ((unsigned char)line.back() & 0x80)) line.pop_back();
		}
		if (!chunk.empty() && chunk.size() + line.size() > max_chunk) {
			if (!translate_chunk(chunk, out)) return any;
			any = true;
			chunk.clear();
		}
		chunk += line;
		used += line.size();
	}
	if (!chunk.empty() && translate_chunk(chunk, out)) any = true;
	return any;
}
