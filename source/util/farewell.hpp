#pragma once
#include <string>
#include <vector>
#include "headers.hpp"

// The farewell of the final version (2.0): used by the About screen and by the dialog shown once after the update.

// "12 h 5 min" / "35 min" / "" (when the app was hardly used): the time spent watching with the app
inline std::string farewell_play_time_text() {
	long long minutes = (long long)(var_total_play_seconds / 60);
	if (minutes < 1) return "";
	if (minutes < 60) return std::to_string(minutes) + " min";
	return std::to_string(minutes / 60) + " h " + std::to_string(minutes % 60) + " min";
}

// the long message of the About screen
inline std::vector<std::string> farewell_about_lines() {
	std::vector<std::string> lines = {
	    tr("Thank you for every video you watched, every", "Gracias por cada vídeo que viste, cada"),
	    tr("bug you reported and every idea you sent.", "fallo que avisaste y cada idea que mandaste."),
	    "",
	    tr("Version 2.0 is the final version: the app", "La versión 2.0 es la final: la app"),
	    tr("is complete and will keep working as long as", "está completa y seguirá funcionando mientras"),
	    tr("YouTube allows it. There will be no more updates.", "YouTube lo permita. No habrá más actualizaciones."),
	    "",
	    tr("The source stays open (GPLv3): anyone can fork", "El código sigue abierto (GPLv3): cualquiera puede"),
	    tr("it, fix it and keep it alive.", "bifurcarlo, arreglarlo y mantenerlo vivo."),
	};
	std::string time = farewell_play_time_text();
	if (time != "") {
		lines.push_back("");
		lines.push_back(tr("Time you spent watching with the app: ", "Tiempo que pasaste viendo con la app: ") + time);
	}
	lines.push_back("");
	lines.push_back(tr("Goodbye, and happy watching!  - IkerPlays", "¡Adiós y a disfrutar!  - IkerPlays"));
	return lines;
}

// the short message of the dialog (7 lines at most; each line must fit the 240 px of a dialog, in every language)
inline std::vector<std::string> farewell_dialog_lines(const std::string &version) {
	std::string time = farewell_play_time_text();
	return {
	    "Iker's Tube " + version + tr(" - final version", " - versión final"),
	    tr("Thank you for using the app!", "¡Gracias por usar la app!"),
	    tr("This is the last update:", "Es la última actualización:"),
	    tr("the project is finished.", "el proyecto está terminado."),
	    time != "" ? tr("You watched: ", "Has visto: ") + time : tr("Enjoy it, and share it!", "¡Disfrútala y compártela!"),
	    tr("New: farewell and list export.", "Nuevo: despedida y exportar listas."),
	    tr("Better sleep timer, exit and saves.", "Mejor temporizador, salida y guardado."),
	};
}
