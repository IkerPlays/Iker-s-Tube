#include "headers.hpp"
#include "system/crash_handler.hpp"

int main() {
	Crash_init();
	Menu_init();

	// Main loop
	while (aptMainLoop()) {
		if (!Menu_main()) {
			break;
		}
	}

	Menu_exit();
	return 0;
}
