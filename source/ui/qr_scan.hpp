#pragma once
#include <string>

// Opens the 3DS camera and shows a live preview until a QR code is read or B is pressed.
// Blocks (runs its own draw loop). Returns true and fills `out` when a code was read.
bool qr_scan_run(std::string &out);
