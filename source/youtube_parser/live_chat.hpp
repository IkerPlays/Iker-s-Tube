#pragma once
#include <string>
#include <vector>

// Live chat of a live stream : a background thread reads it (InnerTube get_live_chat, no login needed) while it is open.
struct ChatMessage {
	std::string author, text;
};
void live_chat_open(const std::string &video_id); // starts reading (restarts if it was another video)
void live_chat_close();
bool live_chat_is_open();
std::string live_chat_video(); // the video whose chat is (or was last) open
unsigned live_chat_version();                             // changes whenever a message arrived or the status changed
std::vector<ChatMessage> live_chat_latest(size_t count);  // the newest `count` messages, newest first
std::string live_chat_status();                           // "" when all is fine, otherwise why there is no chat
// Sends a message with the Studio login (YouTube Data API). Blocks : worker thread only.
bool live_chat_send(const std::string &video_id, const std::string &text, std::string &error);
void live_chat_toggle_mode(); // Top chat <-> Live chat (all the messages), when the stream offers both
int live_chat_mode();           // 0 : top chat, 1 : all messages
