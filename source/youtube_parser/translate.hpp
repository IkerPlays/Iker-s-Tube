#pragma once
#include <string>

// Translates a text into the language of the app with the free Google Translate web endpoint (no key, unofficial).
// Blocks on the network : call it from a worker thread. Returns false if the request failed.
bool translate_text(const std::string &text, std::string &out);
