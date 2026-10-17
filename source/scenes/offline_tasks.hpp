#pragma once
#include <string>

// Saves a video on the SD card in the background (progress is shown with notice_show()).
// Audio only : `video_url` and `both_url` are empty. With `want_video` the video is saved too :
//  - `video_url` + `audio_url` : two files (video without sound + the .m4a)
//  - only `both_url`           : one file that already has the sound (360p)
// `quality` is the height of the video that is saved (video_url / both_url).
bool offline_download_running();
int offline_download_queued();      // downloads waiting for the running one
int offline_download_percent();     // 0..100 of the running download
std::string offline_download_title(); // title of the running download
void offline_download_cancel();       // stops the running download and deletes the partial files
// Loads the page of the video (in the background) and then saves it : for the lists, where the streams are not known yet.
void offline_save_by_id_async(const std::string &id, const std::string &title, const std::string &author, bool want_video);
void offline_download_async(const std::string &id, const std::string &title, const std::string &author,
                            const std::string &audio_url, bool want_video = false, const std::string &video_url = "",
                            const std::string &both_url = "", int quality = 0);
