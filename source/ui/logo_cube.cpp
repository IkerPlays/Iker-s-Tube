#include "headers.hpp"
#include <algorithm>
#include <cmath>
#include "ui/logo_cube.hpp"
#include "ui/colors.hpp"

static C2D_SpriteSheet logo_sheet = NULL;
static C2D_Image logo_image;
static bool logo_tried = false;

namespace {
struct V3 {
	float x, y, z;
};
struct Face {
	int v[4];
	V3 normal;
	int kind; // 0 text, 1 logo, 2 plain
};
} // namespace

static u32 shade(u32 abgr, float k) {
	k = std::max(0.25f, std::min(1.0f, k));
	u32 r = (u32)((abgr & 0xFF) * k), g = (u32)(((abgr >> 8) & 0xFF) * k), b = (u32)(((abgr >> 16) & 0xFF) * k);
	return 0xFF000000 | (b << 16) | (g << 8) | r;
}

void logo_cube_draw(float cx, float cy, float s) {
	if (!logo_tried) {
		logo_tried = true;
		logo_sheet = C2D_SpriteSheetLoad("romfs:/gfx/draw/logo.t3x");
		if (logo_sheet) {
			logo_image = C2D_SpriteSheetGetImage(logo_sheet, 0);
		}
	}
	var_need_refresh = true; // keep animating

	static const V3 corners[8] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
	                              {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}};
	static const Face faces[6] = {
	    {{4, 5, 6, 7}, {0, 0, 1}, 0},  // front  : text
	    {{1, 0, 3, 2}, {0, 0, -1}, 0}, // back   : text
	    {{5, 1, 2, 6}, {1, 0, 0}, 1},  // right  : logo
	    {{0, 4, 7, 3}, {-1, 0, 0}, 1}, // left   : logo
	    {{0, 1, 5, 4}, {0, -1, 0}, 2}, // top
	    {{7, 6, 2, 3}, {0, 1, 0}, 2},  // bottom
	};

	float t = osGetTime() / 1000.0f;
	float ay = t * 0.9f;
	float ax = 0.35f + 0.12f * sinf(t * 0.7f);
	float cay = cosf(ay), say = sinf(ay), cax = cosf(ax), sax = sinf(ax);
	auto rotate = [&](V3 p) {
		V3 a = {p.x * cay + p.z * say, p.y, -p.x * say + p.z * cay};      // around Y
		return V3{a.x, a.y * cax - a.z * sax, a.y * sax + a.z * cax};     // around X
	};

	V3 rv[8];
	float px[8], py[8];
	const float cam = 5.0f;
	for (int i = 0; i < 8; i++) {
		rv[i] = rotate(corners[i]);
		float persp = cam / (cam + rv[i].z * 0.8f);
		px[i] = cx + rv[i].x * s * persp;
		py[i] = cy + rv[i].y * s * persp;
	}

	// painter's algorithm : farthest face first
	int order[6] = {0, 1, 2, 3, 4, 5};
	V3 rn[6];
	float depth[6];
	for (int i = 0; i < 6; i++) {
		rn[i] = rotate(faces[i].normal);
		depth[i] = 0;
		for (int k = 0; k < 4; k++) {
			depth[i] += rv[faces[i].v[k]].z / 4;
		}
	}
	std::sort(order, order + 6, [&](int a, int b) { return depth[a] > depth[b]; });

	u32 accent = COLOR_ACCENT;
	for (int oi = 0; oi < 6; oi++) {
		int fi = order[oi];
		const Face &f = faces[fi];
		if (rn[fi].z > 0) { // facing away from the camera
			continue;
		}
		float light = 0.55f + 0.45f * (-rn[fi].z) * 0.8f + 0.2f * (-rn[fi].y);
		u32 base = f.kind == 2 ? 0xFF2A2020 : accent;
		u32 col = shade(base, light);
		int a = f.v[0], b = f.v[1], c = f.v[2], d = f.v[3];
		C2D_DrawTriangle(px[a], py[a], col, px[b], py[b], col, px[c], py[c], col, 0);
		C2D_DrawTriangle(px[a], py[a], col, px[c], py[c], col, px[d], py[d], col, 0);

		float fx = (px[a] + px[b] + px[c] + px[d]) / 4, fy = (py[a] + py[b] + py[c] + py[d]) / 4;
		float width = fabsf(rn[fi].z) * 1.0f; // how frontal the face is : squeezes the content horizontally
		if (width < 0.18f) {
			continue;
		}
		if (f.kind == 0) {
			float size = 0.55f * s / 60.0f;
			std::string label = "Iker's Tube";
			float w = Draw_get_width(label, size);
			Draw(label, fx - w * width / 2, fy - 8 * size / 0.5f, size * width, size, DEF_DRAW_WHITE);
		} else if (f.kind == 1 && logo_sheet) {
			float img = s * 1.1f;
			Draw_texture(logo_image, fx - img * width / 2, fy - img / 2, img * width, img);
		}
	}
}
