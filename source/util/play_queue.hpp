#pragma once
#include <string>

// "Play next" queue : long-press a video in any list to add it (long-press again to remove it).
// When the current video ends, the queue is played before any autoplay suggestion.
// Thread-safe (the UI thread edits it, the player thread consumes it).
bool play_queue_toggle(const std::string &url, const std::string &title); // returns true if it was added
int play_queue_position(const std::string &url);                           // 1-based, 0 if not queued
int play_queue_size();
bool play_queue_pop(std::string &url_out); // removes and returns the first queued video
void play_queue_clear();
bool play_queue_get(int index, std::string &url, std::string &title); // 0-based
void play_queue_remove_at(int index);
void play_queue_move(int index, int delta); // swaps the item with its neighbour (delta = -1 up, +1 down)
