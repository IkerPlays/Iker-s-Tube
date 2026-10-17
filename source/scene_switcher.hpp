#pragma once

enum class SceneType {
	VIDEO_PLAYER,
	SEARCH,
	SETTINGS,
	CHANNEL,
	ABOUT,
	HISTORY,
	LIBRARY,
	HOME,
	// used for intent
	NO_CHANGE,
	BACK,
	EXIT
};
struct Intent {
	SceneType next_scene = SceneType::NO_CHANGE;
	std::string arg = "";

	bool operator==(const Intent &rhs) { return next_scene == rhs.next_scene && arg == rhs.arg; }
	bool operator!=(const Intent &rhs) { return !(*this == rhs); }
};

extern SceneType global_current_scene;
extern Intent global_intent;
// set together with global_intent by the bottom navigation bar : the destination replaces the scene stack (it is a root, not
// a step forward), and it is switched to even if the intent is the same as the one on top (the Library keeps its tab inside)
extern bool global_intent_is_nav;

void Menu_init(void);

void Menu_exit(void);

bool Menu_main(void);

void Menu_get_system_info(void);

int Menu_check_free_ram(void);
