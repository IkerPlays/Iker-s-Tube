#pragma once

void load_settings();
void settings_mark_stable(); // the app has been running fine: removes the starting flag (see load_settings)
void save_settings();
