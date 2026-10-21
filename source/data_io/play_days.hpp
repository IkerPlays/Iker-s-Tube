#pragma once
#include <utility>
#include <vector>

// Seconds played on each of the last days (sdmc:/3ds/IkersTube/play_days.txt, "day<TAB>seconds"), for the chart of My Wrapped.
// `day` is local_day_number().
void play_days_record(int day, int seconds);                     // keeps the biggest value seen for that day
std::vector<std::pair<int, int>> play_days_last(int count);      // the last `count` days up to today, oldest first
void play_days_clear();                                           // forgets every recorded day
