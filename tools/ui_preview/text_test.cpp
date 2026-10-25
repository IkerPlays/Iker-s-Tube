// Host test of the text helpers (JSON string decoding, cutting on letter boundaries).
#include <cstdio>
#include "util/json_text.hpp"
#include "util/utf8.hpp"

static int bad = 0;
static void check(const char *name, const std::string &got, const std::string &want) {
	if (got != want) {
		printf("FAIL %s -> [%s] (wanted [%s])\n", name, got.c_str(), want.c_str());
		bad++;
	}
}

int main() {
	check("escaped quotes", json_text_unescape("He said \\\"hi\\\""), "He said \"hi\"");
	check("emoji pair", json_text_unescape("\\ud83d\\ude00"), "\xF0\x9F\x98\x80");
	check("bad hex", json_text_unescape("a\\uZZZZb"), "aZZZZb");
	check("lone high half", json_text_unescape("a\\ud83dx"), "ax");
	check("lone low half", json_text_unescape("a\\ude00x"), "ax");
	check("accent", json_text_unescape("caf\\u00e9"), "caf\xC3\xA9");
	check("ampersand", json_text_unescape("Tom \\u0026 Jerry"), "Tom & Jerry");
	check("nul dropped", json_text_unescape("a\\u0000b"), "ab");
	check("newline", json_text_unescape("a\\nb"), "a b");
	std::string s = "xx\"ab\\\"c\"zz";
	check("string end", std::to_string(json_string_end(s, 3)), "8");
	std::string e = "ab\xC3\xA9" "cd";
	check("prefix inside letter", utf8_prefix(e, 3), "ab");
	check("prefix after letter", utf8_prefix(e, 4), "ab\xC3\xA9");
	check("prefix longer", utf8_prefix(e, 20), e);
	printf(bad ? "FAILED\n" : "OK\n");
	return bad ? 1 : 0;
}
