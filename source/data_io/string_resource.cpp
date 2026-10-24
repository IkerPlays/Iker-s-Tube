#include <map>
#include "headers.hpp"
#include "string_resource.hpp"

static std::map<std::string, std::string> *string_resources = NULL;
static std::map<std::string, std::string> *string_resources_alt = NULL;

std::string get_string_resource(std::string id) {
	if (!string_resources) {
		return "[SR Null Err]";
	}
	if (!string_resources->count(id)) {
		return "[SR Not Found]";
	}
	return (*string_resources)[id];
}

Result_with_string load_string_resources(std::string lang) {
	Result_with_string result;

	static char buffer[0x4001];
	memset(buffer, 0, sizeof(buffer));
	u32 read_size;
	result = Path("romfs:/gfx/msg/string_resources_" + lang + ".txt").read_file((u8 *)buffer, 0x4000, read_size);
	if (result.code != 0) {
		result.code = -1;
		result.error_description = "Failed to open the string resource file";
		return result;
	}

	std::map<std::string, std::string> *new_resources =
	    new std::map<std::string, std::string>(parse_xml_like_text(buffer));
	if (!new_resources) {
		result.code = DEF_ERR_OUT_OF_MEMORY;
		result.string = DEF_ERR_OUT_OF_MEMORY_STR;
		return result;
	}
	for (auto &item : *new_resources) {
		std::string tmp_value;
		for (size_t i = 0; i < item.second.size();) {
			if (i + 1 < item.second.size() && item.second[i] == '\\' && item.second[i + 1] == 'n') {
				tmp_value.push_back('\n'), i += 2;
			} else {
				tmp_value.push_back(item.second[i]), i++;
			}
		}
		item.second = tmp_value;
	}
	// it's safe unless two load_string_resources() calls occur while the same call of get_string_resource() is running,
	// which is highly unlikely
	delete (string_resources_alt);
	string_resources_alt = string_resources;
	string_resources = new_resources;

	var_need_refresh = true;

	return result;
}

#include <3ds.h>
#include "variables.hpp"

std::string loading_text() {
	std::string text = get_string_resource("LOADING");
	while (!text.empty() && text.back() == '.') {
		text.pop_back();
	}
	if (text.size() >= 3 && text.compare(text.size() - 3, 3, "\xE2\x80\xA6") == 0) {
		text.resize(text.size() - 3);
	}
	int dots = (int)((osGetTime() / 300) % 3) + 1;
	var_need_refresh = true; // keep the animation running while something is loading
	return text + std::string(dots, '.');
}

struct TrRow {
	const char *fr, *de, *it, *pt, *ja;
};
const std::map<std::string, TrRow> &tr_table();

std::string tr(const char *en, const char *es) {
	if (var_lang == "es") {
		return es;
	}
	if (var_lang != "en") {
		auto &table = tr_table();
		auto it = table.find(en);
		if (it != table.end()) {
			const char *text = var_lang == "fr"   ? it->second.fr
			                   : var_lang == "de" ? it->second.de
			                   : var_lang == "it" ? it->second.it
			                   : var_lang == "br" ? it->second.pt
			                   : var_lang == "ja" ? it->second.ja
			                                      : en;
			return text;
		}
	}
	return en;
}

const char *tr_static(const char *en, const char *es) {
	static std::map<std::string, std::string> cache;
	std::string key = var_lang + "|" + en;
	auto it = cache.find(key);
	if (it == cache.end()) {
		it = cache.emplace(key, tr(en, es)).first;
	}
	return it->second.c_str();
}

std::string friendly_error(const std::string &raw) {
	if (raw.empty()) return raw;
	std::string low = raw;
	for (auto &c : low) c = tolower((unsigned char)c);
	auto has = [&](const char *word) { return low.find(word) != std::string::npos; };
	if (var_wifi_state != 2) return tr("No internet connection", "Sin conexión a internet");
	if (has("resolve")) return tr("Could not reach the server (DNS)", "No se pudo llegar al servidor (DNS)");
	if (has("timed out") || has("timeout")) return tr("The connection timed out", "La conexión tardó demasiado");
	if (has("ssl") || has("certificate")) return tr("Secure connection failed", "Falló la conexión segura");
	if (has("429") || has("too many")) return tr("YouTube is busy, try again later", "YouTube está saturado, prueba más tarde");
	if (has("server error")) return tr("YouTube had a problem, try again later", "YouTube tuvo un problema, prueba más tarde");
	if (has("network request failed") || has("http request failed") || has("empty response")) return tr("Network problem, try again", "Problema de red, inténtalo de nuevo");
	return raw;
}
