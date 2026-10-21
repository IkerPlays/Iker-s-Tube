#pragma once
#include <vector>
#include <string>
#include "network_decoder/thumbnail_loader.hpp"
#include "ui/ui_common.hpp"
#include "../view.hpp"
#include "util/play_queue.hpp"
#include "ui/overlay.hpp"
#include "util/util.hpp"
#include "data_io/history.hpp"
#include "data_io/resume_position.hpp"
#include "data_io/offline.hpp"
#include "youtube_parser/parser.hpp"

#include "variables.hpp"
#define VIDEO_LIST_THUMBNAIL_HEIGHT (var_large_thumbnails ? 72 : 54)
#define VIDEO_LIST_THUMBNAIL_WIDTH (var_large_thumbnails ? 128 : 96)
#define VIDEO_LIST_TEXT_WIDTH (320 - VIDEO_LIST_THUMBNAIL_WIDTH - 15) // the room for the title next to the picture (card margin, gap)

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
		std::string video_id; // the id inside queue_url (worked out once, not at every frame)
		double overlay_seconds = 0; // the length written in the bottom right corner, in seconds (0 : unknown)
	bool badge_new = false; // "NEW": a video of the feed that you had not been told about
	mutable u64 loading_since = 0; // when the placeholder started pulsing
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
		{ // "1:02:03" / "4:05" to seconds
			int parts[3] = {0, 0, 0}, n = 0, value = 0;
			for (char c : bottom_right_overlay + ":") {
				if (c >= '0' && c <= '9') value = value * 10 + (c - '0');
				else if (c == ':' && n < 3) parts[n++] = value, value = 0;
			}
			overlay_seconds = n == 3 ? parts[0] * 3600 + parts[1] * 60 + parts[2] : n == 2 ? parts[0] * 60 + parts[1] : 0;
		}
		return this;
	}
	SuccinctVideoView *set_thumbnail_url(const std::string &thumbnail_url) {
		this->thumbnail_url = thumbnail_url;
		loading_since = 0; // a new picture: the placeholder may pulse again
		return this;
	}
	// long press : add to (or remove from) the "play next" queue
	SuccinctVideoView *set_queue_item(const std::string &url, const std::string &title, const std::string &author = "") {
		this->queue_url = url;
				this->video_id = youtube_get_video_id_by_url(url);
		add_on_long_hold(35, [url, title, author](View &view) {
			overlay_show_video_menu(url, title, author); // the actions of the video (queue, watch later, favorites, save offline...)
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
		title_lines = truncate_str(title, cell_w - 10, 2, 0.45, 0.45);
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
	float get_title_width() const { return (x1 - x0) - ((grid ? 0 : 3) + get_thumbnail_width() + 6) - 6; } // the room for text inside the card

	// the card : a rounded box a few pixels in from the sides of the row (a grid cell fills its whole width)
	void draw_background() const override {
		u32 color = get_background_color ? get_background_color(*this) : background_color;
		if (!(color >> 24)) return;
		float inset = grid ? 0 : 3;
		Draw_round_rect(x0 + inset, y0, get_width() - 2 * inset, get_height(), UI_RADIUS_CARD, color);
	}

	void draw_() const override {
		const float inset = grid ? 0 : 3;
		const float tx = x0 + inset, tw = get_thumbnail_width() - (grid ? 0 : 0), th = get_thumbnail_height();
		const float tw_drawn = grid ? tw : tw; // (the picture keeps its size : the card is what moved)
		bool picture = thumbnail_draw(thumbnail_handle, tx, y0, tw_drawn, th);
		if (picture) {
			// fade-in : a cover with the background color that gets more transparent each frame
			const int FADE_FRAMES = 10;
			if (thumbnail_fade < FADE_FRAMES) {
				u32 alpha = 0xFF * (FADE_FRAMES - thumbnail_fade) / FADE_FRAMES;
				Draw_texture(var_square_image[0], (alpha << 24) | (UI_CARD_COLOR & 0x00FFFFFF), tx, y0, tw_drawn, th);
				thumbnail_fade++;
				var_need_refresh = true;
			}
		} else if (thumbnail_handle >= 0 && var_round_buttons) {
			// still loading : a soft pulsing placeholder (it gives up after 8 seconds so a failed thumbnail does not keep redrawing)
			u64 now = osGetTime();
			if (!loading_since) loading_since = now;
			if (now - loading_since < 8000 && thumbnail_get_status_code(thumbnail_handle) == -2) { // (-2 : no answer yet; a failed download stops the pulsing)
				float pulse = 0.5f + 0.5f * sinf((now - loading_since) / 220.0f);
				Draw_round_rect((int)tx, (int)y0, (int)tw_drawn, (int)th, UI_RADIUS_CARD, ui_mix(UI_CARD_COLOR, UI_RAISED_COLOR, 0.4f + 0.6f * pulse));
				var_need_refresh = true;
			}
		}
		// how much of the video was already watched : a thin accent bar along the bottom of the picture
		if (video_id.size() && overlay_seconds > 0) {
			double seconds = resume_get(video_id);
			if (seconds > 0) {
				float fill = tw_drawn * (float)std::min(1.0, seconds / overlay_seconds);
				Draw_texture(var_square_image[0], 0x99000000, tx, y0 + th - 3, tw_drawn, 3);
				Draw_texture(var_square_image[0], COLOR_ACCENT, tx, y0 + th - 3, fill, 3);
			}
		}
		// the picture takes the rounded corners of the card
		if (picture) ui_mask_corners(tx, y0, tw_drawn, th, UI_RADIUS_CARD, UI_PAGE_COLOR, grid ? 3 : 5);
		// little labels on the picture, from the top left : queue position, NEW
		float lx = tx + 4;
		if (queue_url.size()) {
			int pos = play_queue_position(queue_url);
			if (pos) {
				std::string text = "#" + std::to_string(pos);
				ui_chip(text, lx, y0 + 4, COLOR_ACCENT, UI_ON_ACCENT, 0.42f);
				lx += ui_chip_width(text, 0.42f) + 3;
			}
		}
		if (badge_new) ui_chip("NEW", lx, y0 + 4, COLOR_ACCENT, UI_ON_ACCENT);
		if (var_mark_watched && video_id.size() && history_has_id(video_id)) { // already watched : a check in a dark disc, top right
			float cx = tx + tw_drawn - 10, cy = y0 + 10;
			Draw_round_rect(cx - 6, cy - 6, 12, 12, 6, 0xCC000000);
			ui_check(cx - 3.5f, cy - 3, 0xFF6BE08A);
		}
		if (video_id.size() && offline_has_id(video_id)) { // saved on the SD card
			ui_chip("DL", tx + 4, y0 + th - 17, 0xE0359C54, 0xFFFFFFFF);
		}
		if (bottom_right_overlay.size()) { // the length (LIVE in red), bottom right
			bool live = bottom_right_overlay == "LIVE" || bottom_right_overlay == "SOON";
			float w = ui_chip_width(bottom_right_overlay, DURATION_FONT_SIZE);
			ui_chip(bottom_right_overlay, tx + tw_drawn - w - 4, y0 + th - 17, live ? COLOR_ACCENT : 0xCC000000, live ? (u32)UI_ON_ACCENT : 0xFFFFFFFFu, DURATION_FONT_SIZE);
		}
		if (is_playlist) {
			double overlay_proportion = 0.3;
			Draw_texture(var_square_image[0], 0xB0000000, tx + tw_drawn * (1 - overlay_proportion), y0, tw_drawn * overlay_proportion, th);
			Draw_x_centered(LOCALIZED(PLAYLIST_SHORT), tx + tw_drawn * (1 - overlay_proportion), tx + tw_drawn, (y1 + y0) / 2 - 8, 0.5, 0.5, (u32)-1);
			ui_mask_corners(tx + tw_drawn * (1 - overlay_proportion), y0, tw_drawn * overlay_proportion, th, UI_RADIUS_CARD, UI_PAGE_COLOR, grid ? 2 : 0);
		}
		if (grid) {
			float gy = y0 + th + 2;
			for (auto line : title_lines) {
				Draw(line, x0 + 4, gy, 0.45, 0.45, DEFAULT_TEXT_COLOR);
				gy += DEFAULT_FONT_INTERVAL - 1;
			}
			return;
		}
		const float text_x = tx + tw_drawn + 6;
		float y = y0 + 2;
		for (auto line : title_lines) {
			Draw(line, text_x, y, 0.5, 0.5, DEFAULT_TEXT_COLOR);
			y += DEFAULT_FONT_INTERVAL;
		}
		y += 1;
		int index = 0;
		for (auto line : auxiliary_lines) { // the channel a little stronger than the numbers
			Draw(truncate_first_line(line, (int)get_title_width(), index == 0 ? 0.45 : 0.42), text_x, y, index == 0 ? 0.45 : 0.42, index == 0 ? 0.45 : 0.42, index == 0 ? LIGHT0_TEXT_COLOR : LIGHT1_TEXT_COLOR);
			y += 12;
			index++;
		}
	}
	void update_(Hid_info key) override {}
};
