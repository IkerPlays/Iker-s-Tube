#pragma once
#include <string>

// Downloads the audio of a video to the SD card in the background (progress is shown with notice_show()).
void offline_download_async(const std::string &id, const std::string &title, const std::string &author,
                            const std::string &audio_url);
