#include "headers.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"

// returns in BGR565 format, should be freed
u8 *Image_decode(u8 *input, size_t input_len, int *width, int *height) {
	int image_ch = 0;
	{ // a picture that is too big is not decoded (the pixels, the 565 copy and the texture would use tens of MB, and the loader has little stack)
		int info_w = 0, info_h = 0, info_ch = 0;
		if (!stbi_info_from_memory(input, input_len, &info_w, &info_h, &info_ch) || info_w <= 0 || info_h <= 0 || info_w > 2048 || info_h > 2048 ||
		    (long)info_w * info_h > 1500000L) {
			logger.error("image-dec", "picture too big or not valid : " + std::to_string(info_w) + "x" + std::to_string(info_h));
			return NULL;
		}
	}
	u8 *rgb_image = stbi_load_from_memory(input, input_len, width, height, &image_ch, STBI_rgb);
	if (!rgb_image) {
		logger.error("image-dec", "stbi load failed : " + std::string(stbi_failure_reason()));
		return NULL;
	}
	u8 *bgr_image = (u8 *)malloc(*width * *height * 2);
	if (!bgr_image) {
		stbi_image_free(rgb_image);
		return NULL;
	}
	int in_size = *width * *height * 3;
	u16 *out_head = (u16 *)bgr_image;
	for (int i = 0; i < in_size; i += 3) {
		u16 r = rgb_image[i + 0];
		u16 g = rgb_image[i + 1];
		u16 b = rgb_image[i + 2];
		*out_head++ = ((r & 0b11111000) << 8) | ((g & 0b11111100) << 3) | (b >> 3);
	}

	stbi_image_free(rgb_image);
	return bgr_image;
}
