#include "headers.hpp"
#include <3ds.h>
#include "ui/qr_scan.hpp"
#include "ui/colors.hpp"
extern "C" {
#include "util/quirc/quirc.h"
}

#define QR_CAM_W 400
#define QR_CAM_H 240

bool qr_scan_run(std::string &out) {
	bool found = false;
	if (R_FAILED(camInit())) {
		return false;
	}
	u32 max_bytes = 0;
	u16 *cam_buf = (u16 *)linearAlloc(QR_CAM_W * QR_CAM_H * 2);
	quirc *qr = quirc_new();
	Image_data preview;
	bool preview_ok = false;

	if (cam_buf && qr && quirc_resize(qr, QR_CAM_W, QR_CAM_H) == 0 &&
	    Draw_c2d_image_init(&preview, 512, 256, GPU_RGB565).code == 0) {
		preview_ok = true;
		CAMU_SetSize(SELECT_OUT1, SIZE_CTR_TOP_LCD, CONTEXT_A);
		CAMU_SetOutputFormat(SELECT_OUT1, OUTPUT_RGB_565, CONTEXT_A);
		CAMU_SetNoiseFilter(SELECT_OUT1, true);
		CAMU_SetAutoExposure(SELECT_OUT1, true);
		CAMU_SetAutoWhiteBalance(SELECT_OUT1, true);
		CAMU_SetTrimming(PORT_CAM1, false);
		CAMU_GetMaxBytes(&max_bytes, QR_CAM_W, QR_CAM_H);
		CAMU_SetTransferBytes(PORT_CAM1, max_bytes, QR_CAM_W, QR_CAM_H);
		CAMU_Activate(SELECT_OUT1);
		CAMU_ClearBuffer(PORT_CAM1);
		CAMU_StartCapture(PORT_CAM1);

		int frame = 0;
		while (aptMainLoop() && !found) {
			Hid_info key;
			Util_hid_query_key_state(&key);
			if (key.p_b || key.p_touch) {
				break;
			}

			Handle evt = 0;
			if (R_FAILED(CAMU_SetReceiving(&evt, cam_buf, PORT_CAM1, QR_CAM_W * QR_CAM_H * 2, (s16)max_bytes))) {
				break;
			}
			Result wait = svcWaitSynchronization(evt, 1000000000LL);
			svcCloseHandle(evt);
			if (R_FAILED(wait)) {
				CAMU_ClearBuffer(PORT_CAM1);
				continue;
			}

			// try to read a code every other frame (decoding is the slow part)
			if ((frame++ & 1) == 0) {
				int w, h;
				u8 *gray = quirc_begin(qr, &w, &h);
				for (int i = 0; i < QR_CAM_W * QR_CAM_H; i++) {
					gray[i] = (u8)(((cam_buf[i] >> 5) & 0x3F) << 2); // green channel as luminance
				}
				quirc_end(qr);
				int count = quirc_count(qr);
				for (int i = 0; i < count && !found; i++) {
					quirc_code code;
					quirc_data data;
					quirc_extract(qr, i, &code);
					if (quirc_decode(&code, &data) == QUIRC_SUCCESS) {
						out.assign((const char *)data.payload, data.payload_len);
						found = true;
					}
				}
			}

			Draw_set_texture_data(&preview, (u8 *)cam_buf, QR_CAM_W, QR_CAM_H, 512, 256, GPU_RGB565);
			Draw_frame_ready();
			Draw_screen_ready(0, DEF_DRAW_BLACK);
			Draw_texture(preview.c2d, 0, 0, QR_CAM_W, QR_CAM_H);
			Draw_xy_centered("Point the camera at a QR code", 0, 400, 205, 235, 0.5, 0.5, DEF_DRAW_WHITE);
			Draw_screen_ready(2, DEFAULT_BACK_COLOR);
			Draw_xy_centered("Scanning...\n\nTap or press B to cancel", 0, 320, 0, 240, 0.55, 0.55,
			                 DEFAULT_TEXT_COLOR);
			Draw_apply_draw();
		}
		CAMU_StopCapture(PORT_CAM1);
		CAMU_Activate(SELECT_NONE);
	}

	if (preview_ok) {
		Draw_c2d_image_free(preview);
	}
	if (qr) {
		quirc_destroy(qr);
	}
	linearFree(cam_buf);
	camExit();
	var_need_refresh = true;
	return found;
}
