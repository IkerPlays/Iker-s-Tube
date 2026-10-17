#pragma once

// Writes sdmc:/3ds/IkersTube/report.txt : version, console, main settings, state of the account and Studio, memory, and the end of
// the log and of the last crash report. One file to send when something fails. Returns false if the SD card cannot be written.
bool report_create();
