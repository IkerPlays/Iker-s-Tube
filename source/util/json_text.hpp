#pragma once
#include <cstdlib>
#include <string>

// Helpers to read the text of a JSON string without a full parse (minified answers of YouTube).

inline void utf8_append(std::string &out, unsigned cp) {
	if (cp < 0x80) out += (char)cp;
	else if (cp < 0x800) out += (char)(0xC0 | (cp >> 6)), out += (char)(0x80 | (cp & 0x3F));
	else if (cp < 0x10000) out += (char)(0xE0 | (cp >> 12)), out += (char)(0x80 | ((cp >> 6) & 0x3F)), out += (char)(0x80 | (cp & 0x3F));
	else out += (char)(0xF0 | (cp >> 18)), out += (char)(0x80 | ((cp >> 12) & 0x3F)), out += (char)(0x80 | ((cp >> 6) & 0x3F)), out += (char)(0x80 | (cp & 0x3F));
}

// reads 4 hex digits at s[at..at+4); false if they are not there (a "\uZZZZ" or a cut string)
inline bool json_hex4(const std::string &s, size_t at, unsigned &out) {
	if (at + 4 > s.size()) return false;
	unsigned v = 0;
	for (size_t k = at; k < at + 4; k++) {
		char c = s[k];
		v <<= 4;
		if (c >= '0' && c <= '9') v |= c - '0';
		else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
		else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
		else return false;
	}
	out = v;
	return true;
}

// the end of the JSON string that starts at `from` (index of the closing quote, or s.size()); \" does not end it
inline size_t json_string_end(const std::string &s, size_t from) {
	size_t e = from;
	while (e < s.size() && s[e] != '"') {
		if (s[e] == '\\') e++;
		e++;
	}
	return e < s.size() ? e : s.size();
}

// undoes the escapes of a raw JSON string body: \n \t become a space, \uXXXX (with surrogate pairs) becomes UTF-8
inline std::string json_text_unescape(const std::string &raw) {
	std::string out;
	for (size_t i = 0; i < raw.size(); i++) {
		if (raw[i] != '\\' || i + 1 >= raw.size()) {
			out += raw[i];
			continue;
		}
		char c = raw[++i];
		if (c == 'n' || c == 't' || c == 'r') {
			out += ' ';
		} else if (c == 'u') {
			unsigned cp;
			if (!json_hex4(raw, i + 1, cp)) continue; // (bad hex: the "\u" is dropped, nothing invented)
			i += 4;
			if (cp >= 0xD800 && cp < 0xDC00) { // first half of a pair: needs a real second half
				unsigned low;
				if (i + 6 < raw.size() + 0 && raw[i + 1] == '\\' && raw[i + 2] == 'u' && json_hex4(raw, i + 3, low) && low >= 0xDC00 && low < 0xE000) {
					cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
					i += 6;
				} else {
					continue; // lone half: dropped (it is not valid UTF-8 on its own)
				}
			} else if (cp >= 0xDC00 && cp < 0xE000) {
				continue;
			}
			if (cp != 0) utf8_append(out, cp);
		} else {
			out += c; // \" \\ \/
		}
	}
	return out;
}
