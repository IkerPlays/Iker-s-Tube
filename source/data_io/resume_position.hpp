#pragma once
#include <string>

// "Continue where you left off" : last position of recently watched videos, in sdmc:/3ds/IkersTube/resume.txt.
// Positions near the start or the end of a video are not stored (the video starts from the beginning).
double resume_get(const std::string &video_id); // seconds, or -1
void resume_set(const std::string &video_id, double seconds, double duration);
void resume_save();
int resume_clear_all(); // forgets every position (and the file); returns how many there were
