// Host-side stand-ins for the 3DS draw layer : every call is recorded as one JSON line, a Python script renders it.
#include "ui/ui.hpp"
#include "network_decoder/thumbnail_loader.hpp"
#include "font_table.h"
#include <vector>
#include <string>
#include <cstdarg>

static FILE *out = NULL;
static int next_handle = 1;
void rec_begin(const char *path, int w, int h) { out = fopen(path, "w"); fprintf(out, "{\"w\":%d,\"h\":%d}\n", w, h); }
void rec_end() { if (out) fclose(out); out = NULL; }

static std::string esc(const std::string &s) {
	std::string r;
	for (char c : s) { if (c == '"' || c == '\\') r += '\\'; if (c == '\n') { r += "\\n"; continue; } r += c; }
	return r;
}
static const float text_scale_table[3] = {0.9f, 1.0f, 1.1f};
static float scale_of() { return text_scale_table[var_text_scale < 0 ? 0 : var_text_scale > 2 ? 2 : var_text_scale]; }
float Draw_get_width_one(u32 c, float sx) {
	sx *= scale_of();
	float adv = (c >= 32 && c < 127) ? ADV[c - 32] / 100.0f : 0.62f;
	return adv * 26.0f * sx;
}
float Draw_get_width(const std::string &text, float sx) {
	float x = 0, mx = 0;
	for (size_t i = 0; i < text.size(); i++) {
		unsigned char c = text[i];
		if (c == '\n') { mx = std::max(mx, x); x = 0; continue; }
		if (c >= 0x80) { while (i + 1 < text.size() && ((unsigned char)text[i + 1] & 0xC0) == 0x80) i++; x += Draw_get_width_one(0x4E00, sx) * 0.9f; continue; }
		x += Draw_get_width_one(c, sx);
	}
	return std::max(mx, x);
}
float Draw_get_height(const std::string &text, float sy) {
	int lines = 1;
	for (char c : text) if (c == '\n') lines++;
	return lines * 20.0f * sy;
}
void Draw(std::string text, float x, float y, float sx, float sy, int color) {
	if (!out) return;
	fprintf(out, "{\"t\":\"text\",\"x\":%.1f,\"y\":%.1f,\"s\":%.3f,\"c\":%u,\"v\":\"%s\"}\n", x, y, sx * scale_of(), (unsigned)color, esc(text).c_str());
}
void Draw_x_centered(std::string text, float x0, float x1, float y, float sx, float sy, int c) { Draw(text, x0 + (x1 - x0 - Draw_get_width(text, sx)) / 2, y, sx, sy, c); }
void Draw_y_centered(std::string text, float x, float y0, float y1, float sx, float sy, int c) { Draw(text, x, y0 + (y1 - y0 - Draw_get_height(text, sx)) / 2, sx, sy, c); }
void Draw_xy_centered(std::string text, float x0, float x1, float y0, float y1, float sx, float sy, int c) { Draw(text, x0 + (x1 - x0 - Draw_get_width(text, sx)) / 2, y0 + (y1 - y0 - Draw_get_height(text, sy)) / 2, sx, sy, c); }
void Draw_right(std::string text, float x1, float y, float sx, float sy, int c) { Draw(text, x1 - Draw_get_width(text, sx), y, sx, sy, c); }
void Draw_texture(C2D_Image image, float x, float y, float w, float h) {
	if (out) fprintf(out, "{\"t\":\"tex\",\"x\":%.1f,\"y\":%.1f,\"w\":%.1f,\"h\":%.1f}\n", x, y, w, h);
}
void Draw_texture(C2D_Image image, int color, float x, float y, float w, float h) {
	if (!out) return;
	if (image.tex == (void *)1) fprintf(out, "{\"t\":\"rect\",\"x\":%.1f,\"y\":%.1f,\"w\":%.1f,\"h\":%.1f,\"c\":%u}\n", x, y, w, h, (unsigned)color);
	else fprintf(out, "{\"t\":\"tex\",\"x\":%.1f,\"y\":%.1f,\"w\":%.1f,\"h\":%.1f}\n", x, y, w, h);
}
void Draw_round_rect(float x, float y, float w, float h, float r, int color) {
	if (out) fprintf(out, "{\"t\":\"rrect\",\"x\":%.1f,\"y\":%.1f,\"w\":%.1f,\"h\":%.1f,\"r\":%.1f,\"c\":%u}\n", x, y, w, h, r, (unsigned)color);
}
void Draw_line(float x0, float y0, int c0, float x1, float y1, int c1, float width) {
	if (out) fprintf(out, "{\"t\":\"line\",\"x\":%.1f,\"y\":%.1f,\"x1\":%.1f,\"y1\":%.1f,\"w\":%.1f,\"c\":%u}\n", x0, y0, x1, y1, width, (unsigned)c0);
}
void harness_circle(float x, float y, float r, unsigned c) { if (out) fprintf(out, "{\"t\":\"circle\",\"x\":%.1f,\"y\":%.1f,\"r\":%.1f,\"c\":%u}\n", x, y, r, c); }
void C2D_DrawCircleSolid(float x, float y, float z, float r, u32 c) { harness_circle(x, y, r, c); }
void C2D_DrawRectSolid(float x, float y, float z, float w, float h, u32 c) { if (out) fprintf(out, "{\"t\":\"rect\",\"x\":%.1f,\"y\":%.1f,\"w\":%.1f,\"h\":%.1f,\"c\":%u}\n", x, y, w, h, c); }

// thumbnails : a coloured placeholder per handle
int thumbnail_request(const std::string &url, SceneType, int, ThumbnailType) { return next_handle++; }
void thumbnail_cancel_request(int) {}
void thumbnail_set_priorities(const std::vector<std::pair<int, int>> &) {}
bool thumbnail_draw(int handle, int x, int y, int w, int h) {
	if (out) fprintf(out, "{\"t\":\"thumb\",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"k\":%d}\n", x, y, w, h, handle);
	return true;
}
void C2D_DrawTriangle(float,float,u32,float,float,u32,float,float,u32,float){}
void (*show_info_popup_callback)(const std::string &, int) = nullptr;
std::string get_string_resource(std::string id) { return id; }
bool history_has_id(const std::string &) { return false; }
bool offline_has_id(const std::string &id) { return id == "dl"; }
int play_queue_position(const std::string &) { return 0; }
double resume_get(const std::string &) { return 0; }
int thumbnail_get_status_code(int) { return -2; }
std::vector<std::string> truncate_str(std::string s, int max_width, int max_lines, double sx, double sy) {
	std::vector<std::string> lines;
	std::string cur;
	size_t i = 0;
	while (i < s.size() && (int)lines.size() < max_lines) {
		cur += s[i++];
		if (Draw_get_width(cur, sx) > max_width) {
			size_t sp = cur.find_last_of(' ');
			if (sp != std::string::npos && (int)lines.size() < max_lines - 1) { i -= (cur.size() - sp - 1); cur = cur.substr(0, sp); }
			else if ((int)lines.size() == max_lines - 1) { while (!cur.empty() && Draw_get_width(cur + "...", sx) > max_width) cur.pop_back(); cur += "..."; lines.push_back(cur); cur.clear(); break; }
			else cur.pop_back(), i--;
			lines.push_back(cur);
			cur.clear();
		}
	}
	if (!cur.empty() && (int)lines.size() < max_lines) lines.push_back(cur);
	return lines;
}
std::string tr(const char *en, const char *es) { return en; }
