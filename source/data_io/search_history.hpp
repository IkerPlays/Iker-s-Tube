#pragma once
#include <string>
#include <vector>

// Recent searches (newest first, max 15), stored in sdmc:/3ds/IkersTube/search_history.txt
void search_history_add(const std::string &word);
std::vector<std::string> search_history_get();
void search_history_clear();
void search_history_remove(const std::string &word); // forgets one search
