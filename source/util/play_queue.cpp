#include "util/play_queue.hpp"
#include "system/libctru_wrapper.hpp"
#include <vector>
#include <utility>
#include <cstdlib>

static Mutex queue_lock;
static std::vector<std::pair<std::string, std::string>> queue; // {url, title}
static std::vector<std::pair<std::string, std::string>> repeat_items; // refills the queue when it runs out
static bool repeat_shuffle = false;

static void shuffle_items(std::vector<std::pair<std::string, std::string>> &items) {
	for (size_t i = items.size(); i > 1; i--) {
		std::swap(items[i - 1], items[rand() % i]);
	}
}

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
void play_queue_play_next(const std::string &url, const std::string &title) {
	queue_lock.lock();
	for (size_t i = 0; i < queue.size(); i++) {
		if (queue[i].first == url) {
			queue.erase(queue.begin() + i);
			break;
		}
	}
	queue.insert(queue.begin(), {url, title});
	queue_lock.unlock();
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
	if (!queue.size() && repeat_items.size()) { // "repeat list" : start the list again
		queue = repeat_items;
		if (repeat_shuffle) {
			shuffle_items(queue);
		}
	}
	if (queue.size()) {
		url_out = queue.front().first;
		queue.erase(queue.begin());
		res = true;
	}
	queue_lock.unlock();
	return res;
}
void play_queue_set_repeat(const std::vector<std::pair<std::string, std::string>> &items, bool shuffle) {
	queue_lock.lock();
	repeat_items = items;
	repeat_shuffle = shuffle;
	queue_lock.unlock();
}
void play_queue_clear() {
	queue_lock.lock();
	queue.clear();
	repeat_items.clear();
	queue_lock.unlock();
}

bool play_queue_get(int index, std::string &url, std::string &title) {
	bool res = false;
	queue_lock.lock();
	if (index >= 0 && index < (int)queue.size()) {
		url = queue[index].first;
		title = queue[index].second;
		res = true;
	}
	queue_lock.unlock();
	return res;
}
void play_queue_remove_at(int index) {
	queue_lock.lock();
	if (index >= 0 && index < (int)queue.size()) {
		queue.erase(queue.begin() + index);
	}
	queue_lock.unlock();
}
void play_queue_move(int index, int delta) {
	queue_lock.lock();
	int other = index + delta;
	if (index >= 0 && index < (int)queue.size() && other >= 0 && other < (int)queue.size()) {
		std::swap(queue[index], queue[other]);
	}
	queue_lock.unlock();
}
