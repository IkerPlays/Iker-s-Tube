#include "headers.hpp"
#include <cmath>
#include "data_io/string_resource.hpp"
#include "ui/logo_draw.hpp"

static C2D_SpriteSheet logo_sheet = NULL;
static C2D_Image logo_image;
static bool logo_tried = false;

void logo_draw(float cx, float cy) {
	if (!logo_tried) {
		logo_tried = true;
		logo_sheet = C2D_SpriteSheetLoad("romfs:/gfx/draw/logo.t3x");
		if (logo_sheet) {
			logo_image = C2D_SpriteSheetGetImage(logo_sheet, 0);
		}
	}
	const float size = 112;
	if (logo_sheet) {
		Draw_texture(logo_image, cx - size / 2, cy - size / 2 - 14, size, size);
	}
	Draw_xy_centered("Iker's Tube", cx - 150, cx + 150, cy + size / 2 - 6, cy + size / 2 + 20, 0.7, 0.7,
	                 DEFAULT_TEXT_COLOR);
}
