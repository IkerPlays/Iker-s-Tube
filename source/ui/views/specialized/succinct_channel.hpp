#pragma once
#include <vector>
#include <string>
#include "network_decoder/thumbnail_loader.hpp"
#include "ui/ui_common.hpp"
#include "../view.hpp"
#include "util/util.hpp"
#include <algorithm>

#define CHANNEL_ICON_HEIGHT 54
#define CHANNEL_ICON_WIDTH 96

struct SuccinctChannelView : public FixedSizeView {
  private:
	static constexpr double DURATION_FONT_SIZE = 0.4; // todo : make this customizable
	static constexpr double NAME_FONT_SIZE = 0.6;     // todo : make this customizable
	static constexpr double NAME_FONT_HEIGHT = 17;

	std::string name;
	std::vector<std::string> auxiliary_lines;

  public:
	int thumbnail_handle = -1;
	std::string thumbnail_url;

	SuccinctChannelView(double x0, double y0, double width, double height)
	    : View(x0, y0), FixedSizeView(x0, y0, width, height) {}
	virtual ~SuccinctChannelView() {}

	SuccinctChannelView *set_name(const std::string &name) { // mandatory
		this->name = name;
		return this;
	}
	SuccinctChannelView *set_auxiliary_lines(const std::vector<std::string> &auxiliary_lines) { // mandatory
		this->auxiliary_lines = auxiliary_lines;
		return this;
	}
	SuccinctChannelView *set_thumbnail_url(const std::string &thumbnail_url) {
		this->thumbnail_url = thumbnail_url;
		return this;
	}

	u32 card_color() const { return get_background_color ? get_background_color(*this) : (u32)UI_CARD_COLOR; }
	// the card : a rounded box a few pixels in from the sides of the row
	void draw_background() const override {
		u32 color = card_color();
		if (!(color >> 24)) return;
		Draw_round_rect(x0 + 3, y0, get_width() - 6, get_height(), UI_RADIUS_CARD, color);
	}

	void draw_() const override {
		// the picture is a circle at the left of the card, the text gets the rest of the width
		const float icon_x = x0 + 3 + 4, icon_y = y0 + (get_height() - CHANNEL_ICON_HEIGHT) / 2, text_x = icon_x + CHANNEL_ICON_HEIGHT + 8;
		thumbnail_draw(thumbnail_handle, icon_x, icon_y, CHANNEL_ICON_HEIGHT, CHANNEL_ICON_HEIGHT);
		ui_mask_corners(icon_x, icon_y, CHANNEL_ICON_HEIGHT, CHANNEL_ICON_HEIGHT, CHANNEL_ICON_HEIGHT / 2, card_color());
		const int text_width = std::max<int>(20, (int)(x1 - text_x - 9)); // nothing goes out of the card
		float y = y0 + (get_height() - (NAME_FONT_HEIGHT + 2 + DEFAULT_FONT_INTERVAL * auxiliary_lines.size())) / 2;
		Draw(truncate_first_line(name, text_width, NAME_FONT_SIZE), text_x, y, NAME_FONT_SIZE, NAME_FONT_SIZE, DEFAULT_TEXT_COLOR);
		y += NAME_FONT_HEIGHT + 2;
		for (auto line : auxiliary_lines) {
			Draw(truncate_first_line(line, text_width, 0.5), text_x, y, 0.5, 0.5, LIGHT1_TEXT_COLOR);
			y += DEFAULT_FONT_INTERVAL;
		}
	}
	void update_(Hid_info key) override {}
};
