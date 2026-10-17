#pragma once
#include "ui/views/view.hpp"

// The Studio tab of the home scene : channel statistics and video / Short uploads from the SD card.
View *Studio_create_view();
void Studio_set_height(double height);
void Studio_exit();
