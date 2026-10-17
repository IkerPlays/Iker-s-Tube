#pragma once
#include <map>
#include <string>
#include <vector>
#include "types.hpp"

Result_with_string Util_parse_file(std::string source_data, int num_of_items, std::string out_data[]);

std::string Util_convert_seconds_to_time(double input_seconds);

std::string Util_encode_to_escape(std::string in_data);

std::map<std::string, std::string> parse_xml_like_text(std::string data);

std::vector<std::string> split_string(const std::string &string, char splitter);

// truncate and wrap into at most `max_lines` lines so that each line fit in `max_width` if drawn with the size of
// `x_size` x `y_size` assumes input_str doesn't contain any linebreaks
// first line of truncate_str() ("" if it returns nothing) : never throws
inline std::string truncate_first_line(const std::string &text, int max_width, double size);
std::vector<std::string> truncate_str(std::string input_str, int max_width, int max_lines, double x_size,
                                      double y_size);

inline std::string truncate_first_line(const std::string &text, int max_width, double size) {
	auto lines = truncate_str(text, max_width, 1, size, size);
	return lines.empty() ? std::string() : lines[0];
}
