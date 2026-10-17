#pragma once
#include "types.hpp"

#define LOCALIZED(id) get_string_resource(#id)
#define LOCALIZED_ENABLED_STATUS(cond) ((cond) ? LOCALIZED(ENABLED) : LOCALIZED(DISABLED))

std::string get_string_resource(std::string id);
Result_with_string load_string_resources(std::string lang);

// "Loading" with an animated trailing ellipsis (1 to 3 dots, changes every 300 ms).
std::string loading_text();
