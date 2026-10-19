#pragma once
#include <string>
#include <utility>
#include <vector>

// "Play next" queue : long-press a video in any list to add it (long-press again to remove it).
// When the current video ends, the queue is played before any autoplay suggestion.
// Thread-safe (the UI thread edits it, the player thread consumes it).
void play_queue_play_next(const std::string &url, const std::string &title); // puts it first in the queue (moves it if queued)
bool play_queue_toggle(const std::string &url, const std::string &title); // returns true if it was added
int play_queue_position(const std::string &url);                           // 1-based, 0 if not queued
int play_queue_size();
bool play_queue_pop(std::string &url_out); // removes and returns the first queued video
void play_queue_shuffle(); // mixes the videos that are queued (the repeat list stays); one save
void play_queue_clear(); // also stops the "repeat list" mode
// "Repeat list" : when the queue runs out it is refilled with these {url, title} (shuffled again if requested)
void play_queue_set_repeat(const std::vector<std::pair<std::string, std::string>> &items, bool shuffle);
bool play_queue_get(int index, std::string &url, std::string &title); // 0-based
void play_queue_remove_at(int index);
void play_queue_move(int index, int delta); // swaps the item with its neighbour (delta = -1 up, +1 down)
void play_queue_load(); // reads the queue saved by the last run (call once at startup)
