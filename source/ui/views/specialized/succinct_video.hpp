#pragma once
#include <vector>
#include <string>
#include "network_decoder/thumbnail_loader.hpp"
#include "ui/ui_common.hpp"
#include "../view.hpp"
#include "util/play_queue.hpp"
#include "util/util.hpp"
#include "data_io/history.hpp"
#include "data_io/offline.hpp"
#include "youtube_parser/parser.hpp"

#include "variables.hpp"
#define VIDEO_LIST_THUMBNAIL_HEIGHT (var_large_thumbnails ? 72 : 54)
#define VIDEO_LIST_THUMBNAIL_WIDTH (var_large_thumbnails ? 128 : 96)

struct SuccinctVideoView : public FixedSizeView {
  private:
	static constexpr double DURATION_FONT_SIZE = 0.4; // todo : make this customizable

	std::vector<std::string> title_lines;
	std::vector<std::string> auxiliary_lines;
	std::string bottom_right_overlay;
	bool grid = false; // grid cell : the thumbnail fills the width and the title goes below it
	float get_thumbnail_height() const { return grid ? (x1 - x0) * 9 / 16 : y1 - y0; }
	float get_thumbnail_width() const {
		return grid ? x1 - x0 : get_thumbnail_height() * VIDEO_LIST_THUMBNAIL_WIDTH / VIDEO_LIST_THUMBNAIL_HEIGHT;
	}

  public:
	int thumbnail_handle = -1;
	std::string thumbnail_url;

	bool is_playlist = false;
	std::string queue_url; // set for videos that can be added to the play queue
	mutable int thumbnail_fade = 0; // frames since the thumbnail appeared (fade-in animation)

	SuccinctVideoView(double x0, double y0, double width, double height)
	    : View(x0, y0), FixedSizeView(x0, y0, width, height) {}
	virtual ~SuccinctVideoView() {}

	SuccinctVideoView *set_title_lines(const std::vector<std::string> &title_lines) { // mandatory
		this->title_lines = title_lines;
		return this;
	}
	SuccinctVideoView *set_auxiliary_lines(const std::vector<std::string> &auxiliary_lines) { // mandatory
		this->auxiliary_lines = auxiliary_lines;
		return this;
	}
	SuccinctVideoView *set_bottom_right_overlay(const std::string &bottom_right_overlay) {
		this->bottom_right_overlay = bottom_right_overlay;
		return this;
	}
	SuccinctVideoView *set_thumbnail_url(const std::string &thumbnail_url) {
		this->thumbnail_url = thumbnail_url;
		return this;
	}
	// long press : add to (or remove from) the "play next" queue
	SuccinctVideoView *set_queue_item(const std::string &url, const std::string &title) {
		this->queue_url = url;
		add_on_long_hold(35, [url, title](View &view) {
			play_queue_toggle(url, title);
			view.reset_holding_status(); // the release must not open the video
			var_need_refresh = true;
		});
		return this;
	}
	// grid mode (if enabled in the settings) : half-width cell, the thumbnail on top and the title below
	SuccinctVideoView *auto_grid(const std::string &title) {
		if (!var_grid_view) return this;
		const int cell_w = 158;
		grid = true;
		set_width(cell_w);
		set_height(cell_w * 9 / 16 + (DEFAULT_FONT_INTERVAL - 1) * 2 + 3);
		title_lines = truncate_str(title, cell_w - 4, 2, 0.45, 0.45);
		return this;
	}
	SuccinctVideoView *set_grid(bool grid) {
		this->grid = grid;
		return this;
	}
	SuccinctVideoView *set_is_playlist(bool is_playlist) {
		this->is_playlist = is_playlist;
		return this;
	}
	float get_title_width() const { return x1 - x0 - (get_thumbnail_width() + 3); }

	void draw_() const override {
		if (thumbnail_draw(thumbnail_handle, x0, y0, get_thumbnail_width(), get_thumbnail_height())) {
			// fade-in : a cover with the background color that gets more transparent each frame
			const int FADE_FRAMES = 10;
			if (thumbnail_fade < FADE_FRAMES) {
				u32 alpha = 0xFF * (FADE_FRAMES - thumbnail_fade) / FADE_FRAMES;
				Draw_texture(var_square_image[0], (alpha << 24) | (DEFAULT_BACK_COLOR & 0x00FFFFFF), x0, y0,
				             get_thumbnail_width(), get_thumbnail_height());
				thumbnail_fade++;
				var_need_refresh = true;
			}
		}
		if (var_mark_watched && queue_url.size() && history_has_id(youtube_get_video_id_by_url(queue_url))) {
			// already watched : a small check mark on the thumbnail
			float bx = x0 + get_thumbnail_width() - 14;
			Draw_texture(var_square_image[0], 0xE0000000, bx, y0, 14, 12);
			Draw("v", bx + 3, y0 - 2, 0.45, 0.45, 0xFF60E060);
		}
		if (queue_url.size() && offline_has_id(youtube_get_video_id_by_url(queue_url))) {
			// saved on the SD card : a small tag at the bottom left of the thumbnail
			float tag_width = Draw_get_width("DL", 0.4) + 4;
			Draw_texture(var_square_image[0], 0xE0408040, x0, y0 + get_thumbnail_height() - 10, tag_width, 10);
			Draw("DL", x0 + 2, y0 + get_thumbnail_height() - 11, 0.4, 0.4, (u32)-1);
		}
		if (queue_url.size()) {
			int pos = play_queue_position(queue_url);
			if (pos) {
				std::string text = "#" + std::to_string(pos);
				float w = Draw_get_width(text, 0.45) + 6;
				Draw_texture(var_square_image[0], 0xE03B3BE6, x0, y0, w, 12);
				Draw(text, x0 + 3, y0 - 1, 0.45, 0.45, (u32)-1);
			}
		}
		if (bottom_right_overlay.size()) {
			float overlay_width = Draw_get_width(bottom_right_overlay, DURATION_FONT_SIZE);
			Draw_texture(var_square_image[0], 0xBB000000, x0 + get_thumbnail_width() - overlay_width - 2,
			             y0 + get_thumbnail_height() - 10, overlay_width + 2, 10);
			Draw(bottom_right_overlay, x0 + get_thumbnail_width() - overlay_width - 1, y0 + get_thumbnail_height() - 11,
			     DURATION_FONT_SIZE, DURATION_FONT_SIZE, (u32)-1);
		}
		if (is_playlist) {
			double overlay_proportion = 0.3;
			Draw_texture(var_square_image[0], 0xA0000000, x0 + get_thumbnail_width() * (1 - overlay_proportion), y0,
			             get_thumbnail_width() * overlay_proportion, get_thumbnail_height());
			Draw_x_centered(LOCALIZED(PLAYLIST_SHORT), x0 + get_thumbnail_width() * (1 - overlay_proportion),
			                x0 + get_thumbnail_width(), (y1 + y0) / 2 - 8, 0.5, 0.5, (u32)-1);
		}
		if (grid) {
			float gy = y0 + get_thumbnail_height() + 1;
			for (auto line : title_lines) {
				Draw(line, x0 + 2, gy, 0.45, 0.45, DEFAULT_TEXT_COLOR);
				gy += DEFAULT_FONT_INTERVAL - 1;
			}
			return;
		}
		float y = y0;
		for (auto line : title_lines) {
			Draw(line, x0 + get_thumbnail_width() + 3, y, 0.5, 0.5, DEFAULT_TEXT_COLOR);
			y += DEFAULT_FONT_INTERVAL;
		}
		y += 2;
		for (auto line : auxiliary_lines) {
			Draw(line, x0 + get_thumbnail_width() + 3, y, 0.5, 0.5, LIGHT0_TEXT_COLOR);
			y += DEFAULT_FONT_INTERVAL;
		}
	}
	void update_(Hid_info key) override {}
};
