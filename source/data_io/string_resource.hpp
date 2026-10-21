#pragma once
#include "types.hpp"

#define LOCALIZED(id) get_string_resource(#id)
#define LOCALIZED_ENABLED_STATUS(cond) ((cond) ? LOCALIZED(ENABLED) : LOCALIZED(DISABLED))

std::string get_string_resource(std::string id);
Result_with_string load_string_resources(std::string lang);

// "Loading" with an animated trailing ellipsis (1 to 3 dots, changes every 300 ms).
std::string loading_text();

// A readable version of a network error (no Wi-Fi, no server, timed out...). Unknown errors come back unchanged.
std::string friendly_error(const std::string &raw);

// Text for the features added by Iker's Tube : Spanish when the app language is Spanish, English otherwise.
std::string tr(const char *en, const char *es);

// Same as tr() but the returned pointer stays valid forever (for `const char *` variables).
const char *tr_static(const char *en, const char *es);
