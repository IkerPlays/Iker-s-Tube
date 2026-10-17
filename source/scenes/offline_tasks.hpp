#pragma once
#include <string>

// Saves a video on the SD card in the background (progress is shown with notice_show()).
// Audio only : `video_url` and `both_url` are empty. With `want_video` the video is saved too :
//  - `video_url` + `audio_url` : two files (video without sound + the .m4a)
//  - only `both_url`           : one file that already has the sound (360p)
// `quality` is the height of the video that is saved (video_url / both_url).
void offline_download_async(const std::string &id, const std::string &title, const std::string &author,
                            const std::string &audio_url, bool want_video = false, const std::string &video_url = "",
                            const std::string &both_url = "", int quality = 0);
