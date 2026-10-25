#pragma once
#include <string>

// The first `max_bytes` bytes of `text`, cut back to a letter boundary (never in the middle of a multi-byte character).
inline std::string utf8_prefix(const std::string &text, size_t max_bytes) {
	if (text.size() <= max_bytes) return text;
	size_t n = max_bytes;
	while (n > 0 && ((unsigned char)text[n] & 0xC0) == 0x80) n--; // text[n] is the first byte left out: if it is a continuation byte the cut is inside a letter
	return text.substr(0, n);
}
