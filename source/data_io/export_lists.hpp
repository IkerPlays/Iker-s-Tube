#pragma once
#include <string>

// "Export my lists": writes what the user keeps in the app to plain files on the SD card (sdmc:/3ds/IkersTube/export/), so it can be
// opened on a computer or moved to another app: subscriptions.csv (the format of Google Takeout), favorites.txt, watch_later.txt and
// history.txt (address <TAB> title [<TAB> date]).
// Returns false when a file could not be written (SD card full or missing); the counts are the number of lines of each list.
bool export_my_lists(int &subscriptions, int &favorites, int &watch_later, int &history);
