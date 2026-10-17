#include "system/crash_handler.hpp"
#include <3ds.h>
#include <cstdio>
#include <cstdlib>
#include <cxxabi.h>
#include <exception>
#include <sys/stat.h>
#include <typeinfo>
#include <unwind.h>

#ifndef IKT_VERSION
#define IKT_VERSION "?"
#endif

struct BacktraceState {
	FILE *file;
	int count;
};

static _Unwind_Reason_Code backtrace_step(struct _Unwind_Context *context, void *arg) {
	BacktraceState *state = (BacktraceState *)arg;
	uintptr_t ip = _Unwind_GetIP(context);
	if (ip) {
		fprintf(state->file, "  #%02d %08lx\n", state->count, (unsigned long)ip);
		state->count++;
	}
	return state->count >= 40 ? _URC_END_OF_STACK : _URC_NO_REASON;
}

static void terminate_handler() {
	static bool already_handling = false; // a second failure inside the handler must not loop
	if (!already_handling) {
		already_handling = true;
		mkdir("sdmc:/3ds", 0777);
		mkdir("sdmc:/3ds/IkersTube", 0777);
		FILE *f = fopen("sdmc:/3ds/IkersTube/crash.txt", "a");
		if (f) {
			Handle thread = 0;
			u32 thread_id = 0;
			svcGetThreadId(&thread_id, CUR_THREAD_HANDLE);
			fprintf(f, "=== crash : std::terminate ===\nversion %s (%s)\nthread id %lu\n", IKT_VERSION, __DATE__ " " __TIME__,
			        (unsigned long)thread_id);
			std::type_info *type = abi::__cxa_current_exception_type();
			fprintf(f, "exception type : %s\n", type ? type->name() : "none (abort or terminate called directly)");
			fprintf(f, "backtrace :\n");
			BacktraceState state = {f, 0};
			_Unwind_Backtrace(backtrace_step, &state);
			fprintf(f, "\n");
			fclose(f);
			(void)thread;
		}
	}
	abort();
}

void Crash_init() {
	std::set_terminate(terminate_handler);
}
