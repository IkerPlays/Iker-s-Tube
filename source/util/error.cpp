#include "headers.hpp"
#include "ui/colors.hpp"

bool err_error_display = false;
std::string err_error_summary = "N/A";
std::string err_error_description = "N/A";
std::string err_error_place = "N/A";
std::string err_error_code = "N/A";

bool Util_err_query_error_show_flag(void) { return err_error_display; }

void Util_err_set_error_message(std::string summary, std::string description, std::string place) {
	Util_err_set_error_message(summary, description, place, 1234567890);
}

void Util_err_set_error_message(std::string summary, std::string description, std::string place, int error_code) {
	char cache[128];
	Util_err_clear_error_message();
	err_error_summary = summary;
	err_error_description = description;
	err_error_place = place;
	if (error_code == 1234567890) {
		err_error_code = "N/A";
	} else {
		sprintf(cache, "0x%x", error_code);
		err_error_code = cache;
	}
}

void Util_err_set_error_show_flag(bool flag) { err_error_display = flag; }

void Util_err_clear_error_message(void) {
	err_error_summary = "N/A";
	err_error_description = "N/A";
	err_error_place = "N/A";
	err_error_code = "N/A";
}

void Util_err_save_error(void) {
	char stamp[40]; // padded : "2026-1-12" and "2026-11-2" no longer give the same name, and the hour is part of it
	snprintf(stamp, sizeof stamp, "%04d%02d%02d_%02d%02d%02d.txt", var_years, var_months, var_days, var_hours, var_minutes, var_seconds);
	Path(DEF_MAIN_DIR + "error/" + stamp)
	    .write_file(
	        (u8 *)(err_error_summary + "\n" + err_error_description + "\n" + err_error_place + "\n" + err_error_code)
	            .c_str(),
	        (err_error_summary + "\n" + err_error_description + "\n" + err_error_place + "\n" + err_error_code)
	            .length());
	Util_err_set_error_show_flag(false);
}

void Util_err_main(Hid_info key) {
	if (key.p_a ||
	    (key.p_touch && key.touch_x >= 150 && key.touch_x <= 204 && key.touch_y >= 150 && key.touch_y <= 170)) {
		err_error_display = false;
		var_need_refresh = true;
	} else if (key.p_x ||
	           (key.p_touch && key.touch_x >= 209 && key.touch_x <= 259 && key.touch_y >= 150 && key.touch_y <= 170)) {
		Util_err_save_error();
		var_need_refresh = true;
	}
}

void Util_err_draw(void) {
	Draw_texture(var_square_image[0], DEF_DRAW_LIGHT_GRAY, 20.0, 30.0, 280.0, 150.0);
	Draw_texture(var_square_image[0], DEF_DRAW_YELLOW, 150.0, 150.0, 55.0, 20.0);
	Draw_texture(var_square_image[0], DEF_DRAW_YELLOW, 209.0, 150.0, 50.0, 20.0);

	Draw("Summary : ", 22.5, 40.0, 0.45, 0.45, DEF_DRAW_RED);
	Draw(err_error_summary, 22.5, 50.0, 0.45, 0.45, DEF_DRAW_BLACK);
	Draw("Description : ", 22.5, 60.0, 0.45, 0.45, DEF_DRAW_RED);
	{ // a long description goes on a second line instead of running off the panel
		auto lines = truncate_str(err_error_description, 270, 2, 0.4, 0.4);
		for (size_t i = 0; i < lines.size(); i++) Draw(lines[i], 22.5, 70.0 + 9.0 * i, 0.4, 0.4, DEF_DRAW_BLACK);
	}
	Draw("Place : ", 22.5, 90.0, 0.45, 0.45, DEF_DRAW_RED);
	Draw(err_error_place, 22.5, 100.0, 0.45, 0.45, DEF_DRAW_BLACK);
	Draw("Error code : ", 22.5, 110.0, 0.45, 0.45, DEF_DRAW_RED);
	Draw(err_error_code, 22.5, 120.0, 0.45, 0.45, DEF_DRAW_BLACK);
	Draw("Close (A)", 152.5, 152.5, 0.375, 0.375, DEF_DRAW_BLACK);
	Draw("Save (X)", 211.5, 152.5, 0.375, 0.375, DEF_DRAW_BLACK);
}