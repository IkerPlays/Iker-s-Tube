#pragma once
#include <string>
#include "types.hpp"

// Full bottom-screen popup that shows a QR code for a text (e.g. a video URL).
void qr_popup_show(const std::string &text);
bool qr_popup_visible();
void qr_popup_draw();
// returns true when the popup consumed the input (it is visible)
bool qr_popup_update(Hid_info key);
