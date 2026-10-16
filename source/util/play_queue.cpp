#include "util/play_queue.hpp"
#include "system/libctru_wrapper.hpp"
#include <vector>
#include <utility>

static Mutex queue_lock;
static std::vector<std::pair<std::string, std::string>> queue; // {url, title}

bool play_queue_toggle(const std::string &url, const std::string &title) {
	bool added = true;
	queue_lock.lock();
	for (size_t i = 0; i < queue.size(); i++) {
		if (queue[i].first == url) {
			queue.erase(queue.begin() + i);
			added = false;
			break;
		}
	}
	if (added) {
		queue.push_back({url, title});
	}
	queue_lock.unlock();
	return added;
}
int play_queue_position(const std::string &url) {
	int res = 0;
	queue_lock.lock();
	for (size_t i = 0; i < queue.size(); i++) {
		if (queue[i].first == url) {
			res = i + 1;
			break;
		}
	}
	queue_lock.unlock();
	return res;
}
int play_queue_size() {
	queue_lock.lock();
	int res = queue.size();
	queue_lock.unlock();
	return res;
}
bool play_queue_pop(std::string &url_out) {
	bool res = false;
	queue_lock.lock();
	if (queue.size()) {
		url_out = queue.front().first;
		queue.erase(queue.begin());
		res = true;
	}
	queue_lock.unlock();
	return res;
}
void play_queue_clear() {
	queue_lock.lock();
	queue.clear();
	queue_lock.unlock();
}
