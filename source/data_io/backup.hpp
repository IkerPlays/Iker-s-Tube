#pragma once

// Copies the user data (favorites, watch later, playlists, bookmarks, history, subscriptions, resume points, settings)
// to sdmc:/3ds/IkersTube/backup/ and back. The OAuth login is NOT part of the backup.
// Both return the number of files copied (-1 if the SD card could not be used).
int backup_export();
int backup_import(); // the app has to be restarted afterwards: the files are restored at the next start
void backup_apply_pending(); // restores a pending import (call at the very start, before anything is loaded)
long long backup_time(); // when the backup in sdmc:/3ds/IkersTube/backup was made (seconds since 1970, 0 : there is none)
