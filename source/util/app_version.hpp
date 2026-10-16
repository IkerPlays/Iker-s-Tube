#pragma once
#include <string>

// Version handling of the updater.
//   stable : "1.0.0", "v1.2.3"
//   devbuild : "1.0.1-dev.21" (core version of the NEXT release + "-dev.<build number>")
// Order : core (major.minor.patch) first, then a stable release is newer than any devbuild of the same core,
// then the devbuild number. So 1.0.0-dev.5 < 1.0.0 < 1.0.1-dev.1 < 1.0.1.
struct AppVersion {
	int core[3] = {0, 0, 0};
	bool is_dev = false;
	int dev = 0;
	bool valid = false;
};

// Finds the first "N[.N[.N]]" in the string (so "v1.0.0", "Iker's Tube 1.0.1-dev.21 (devbuild)" work) and an
// optional "-dev.N" right after it.
inline AppVersion app_version_parse(const std::string &s) {
	AppVersion v;
	size_t i = 0;
	while (i < s.size() && !(s[i] >= '0' && s[i] <= '9')) {
		i++;
	}
	if (i >= s.size()) {
		return v;
	}
	for (int part = 0; part < 3; part++) {
		int n = 0;
		bool any = false;
		while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
			n = n * 10 + (s[i] - '0');
			any = true;
			i++;
		}
		if (!any) {
			break;
		}
		v.core[part] = n;
		if (part < 2 && i + 1 < s.size() && s[i] == '.' && s[i + 1] >= '0' && s[i + 1] <= '9') {
			i++; // next part
		} else {
			break;
		}
	}
	v.valid = true;
	if (s.compare(i, 5, "-dev.") == 0) {
		v.is_dev = true;
		i += 5;
		while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
			v.dev = v.dev * 10 + (s[i] - '0');
			i++;
		}
	}
	return v;
}

// < 0 if a is older than b, 0 if equal, > 0 if a is newer
inline int app_version_compare(const AppVersion &a, const AppVersion &b) {
	for (int i = 0; i < 3; i++) {
		if (a.core[i] != b.core[i]) {
			return a.core[i] < b.core[i] ? -1 : 1;
		}
	}
	if (a.is_dev != b.is_dev) {
		return a.is_dev ? -1 : 1; // stable > devbuild of the same core
	}
	if (a.dev != b.dev) {
		return a.dev < b.dev ? -1 : 1;
	}
	return 0;
}
