#pragma once

// Installs a std::terminate handler. The app is built without exceptions, so any `throw` (out of range substr/at,
// bad_alloc, bad_function_call, ...) ends in std::terminate. The handler writes sdmc:/3ds/IkersTube/crash.txt with the
// exception type, the version, the thread and a backtrace (addresses; resolve them with addr2line on the same build).
void Crash_init();
