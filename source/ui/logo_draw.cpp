#include "headers.hpp"
#include <cmath>
#include "data_io/string_resource.hpp"
#include "ui/logo_draw.hpp"
#include "network_decoder/image.hpp"
#include <cstdio>

static C2D_SpriteSheet logo_sheet = NULL;
static C2D_Image logo_image;
static bool logo_tried = false;

// optional custom background : sdmc:/3ds/IkersTube/background.png (or .jpg), up to 1024x1024
static Image_data bg_image;
static bool bg_loaded = false, bg_tried = false;
static void load_background() {
	bg_tried = true;
	const char *paths[] = {"sdmc:/3ds/IkersTube/background.png", "sdmc:/3ds/IkersTube/background.jpg"};
	for (auto path : paths) {
		FILE *f = fopen(path, "rb");
		if (!f) continue;
		fseek(f, 0, SEEK_END);
		long size = ftell(f);
		fseek(f, 0, SEEK_SET);
		if (size <= 0 || size > 4 * 1024 * 1024) {
			fclose(f);
			continue;
		}
		u8 *data = (u8 *)malloc(size);
		if (!data) {
			fclose(f);
			continue;
		}
		size_t got = fread(data, 1, size, f);
		fclose(f);
		if (got != (size_t)size) {
			free(data);
			continue;
		}
		int w = 0, h = 0;
		u8 *pixels = Image_decode(data, size, &w, &h);
		free(data);
		if (!pixels) continue;
		int tw = 1, th = 1;
		while (tw < w) tw <<= 1;
		while (th < h) th <<= 1;
		if (w > 1024 || h > 1024 || Draw_c2d_image_init(&bg_image, tw, th, GPU_RGB565).code != 0) {
			free(pixels);
			continue;
		}
		bg_loaded = Draw_set_texture_data(&bg_image, pixels, w, h, tw, th, GPU_RGB565).code == 0;
		free(pixels);
		if (bg_loaded) return;
		Draw_c2d_image_free(bg_image); // the texture was created but could not be filled
	}
}

void logo_draw(float cx, float cy) {
	if (!bg_tried) load_background();
	if (bg_loaded) {
		Draw_texture(bg_image.c2d, 0, 0, 400, 240);
	}
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
