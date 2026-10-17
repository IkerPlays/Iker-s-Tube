#include "headers.hpp"
#include <vector>
#include "ui/qr_popup.hpp"
#include "ui/colors.hpp"
extern "C" {
#include "util/qrcodegen/qrcodegen.h"
}

static bool qr_visible = false;
static int qr_size = 0;
static std::vector<u8> qr_modules; // qr_size * qr_size, 1 = dark
static std::string qr_text;

void qr_popup_show(const std::string &text) {
	static u8 temp[qrcodegen_BUFFER_LEN_MAX];
	static u8 qrcode[qrcodegen_BUFFER_LEN_MAX];
	if (!qrcodegen_encodeText(text.c_str(), temp, qrcode, qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN,
	                          qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true)) {
		return;
	}
	qr_size = qrcodegen_getSize(qrcode);
	qr_modules.assign(qr_size * qr_size, 0);
	for (int y = 0; y < qr_size; y++) {
		for (int x = 0; x < qr_size; x++) {
			qr_modules[y * qr_size + x] = qrcodegen_getModule(qrcode, x, y);
		}
	}
	qr_text = text;
	qr_visible = true;
	var_need_refresh = true;
}

bool qr_popup_visible() {
	return qr_visible;
}

void qr_popup_draw() {
	if (!qr_visible) {
		return;
	}
	Draw_texture(var_square_image[0], DEF_DRAW_WHITE, 0, 0, 320, 240);
	const int quiet = 2; // quiet zone, in modules
	int module_px = std::max(1, 200 / (qr_size + quiet * 2));
	int total = (qr_size + quiet * 2) * module_px;
	int x0 = (320 - total) / 2;
	int y0 = 8;
	for (int y = 0; y < qr_size; y++) {
		for (int x = 0; x < qr_size; x++) {
			if (qr_modules[y * qr_size + x]) {
				Draw_texture(var_square_image[0], DEF_DRAW_BLACK, x0 + (x + quiet) * module_px,
				             y0 + (y + quiet) * module_px, module_px, module_px);
			}
		}
	}
	Draw_xy_centered("Scan with your phone", 0, 320, y0 + total + 4, y0 + total + 24, 0.5, 0.5, DEF_DRAW_BLACK);
	Draw_xy_centered("Tap or press B to close", 0, 320, 215, 235, 0.45, 0.45, DEF_DRAW_GRAY);
}

bool qr_popup_update(Hid_info key) {
	if (!qr_visible) {
		return false;
	}
	if (key.p_touch || key.p_b) {
		qr_visible = false;
		var_need_refresh = true;
	}
	return true;
}
