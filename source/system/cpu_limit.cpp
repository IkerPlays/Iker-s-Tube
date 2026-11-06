#include "headers.hpp"
#include <set>

static Mutex cpu_limits_lock;
static std::multiset<int> cpu_limits;

static const int BASE_CPU_LIMIT = 30; // the limit Menu_init sets at start: what stays when no task asks for a lower one

static void apply_cpu_limit_locked() {
	APT_SetAppCpuTimeLimit(cpu_limits.empty() ? BASE_CPU_LIMIT : *cpu_limits.begin()); // (begin() of an empty set must not be read)
}
void add_cpu_limit(int limit) {
	cpu_limits_lock.lock();
	cpu_limits.insert(limit);
	apply_cpu_limit_locked();
	cpu_limits_lock.unlock();
}
void remove_cpu_limit(int limit) {
	cpu_limits_lock.lock();
	auto it = cpu_limits.find(limit);
	if (it != cpu_limits.end()) cpu_limits.erase(it); // (a restore without its suspend must not erase end())
	apply_cpu_limit_locked();
	cpu_limits_lock.unlock();
}
int get_cpu_limit() {
	u32 res;
	APT_GetAppCpuTimeLimit(&res);
	return res;
}
