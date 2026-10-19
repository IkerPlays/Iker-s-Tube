#pragma once

void load_settings();
void settings_check_stable(); // call it often: marks the start as good after 30 seconds
int local_day_number();
void settings_mark_stable(); // the app has been running fine: removes the starting flag (see load_settings)
void save_settings();
