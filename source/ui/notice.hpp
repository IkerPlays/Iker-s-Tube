#pragma once
#include <string>

// A short message shown for a few seconds at the bottom of the bottom screen (thread-safe).
void notice_show(const std::string &text);
void notice_draw(float bottom_y); // call from the draw function of a scene
