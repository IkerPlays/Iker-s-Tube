#include "headers.hpp"
#include "network_downloader.hpp"
#include "network_io.hpp"

#include <malloc.h>
#include <dirent.h>
#include <sys/stat.h>

// The cache is much bigger on New 3DS than the original 12 MB so that the video can buffer minutes ahead, but we
// fall back to the original size whenever the heap is running low so that we never run out of memory.
static u64 get_max_cache_blocks() {
	if (!var_is_new3ds) {
		return NetworkStream::OLD3DS_MAX_CACHE_BLOCKS;
	}
	struct mallinfo mi = mallinfo();
	s64 heap_free = (s64)envGetHeapSize() - (s64)mi.uordblks;
	if (heap_free < 28 * 1000 * 1000) {
		return NetworkStream::NEW3DS_SAFE_CACHE_BLOCKS;
	}
	return NetworkStream::NEW3DS_MAX_CACHE_BLOCKS;
}
#define MAX_CACHE_BLOCKS get_max_cache_blocks()

// --------------------------------
// NetworkStream implementation
// --------------------------------

#define DISK_CACHE_DIR "sdmc:/3ds/IkersTube/cache/"

// RAM cache size (in blocks) of a stream. When the SD tier is active only the data close to the read head needs to
// live in RAM, so this stays small whatever the buffer size is.
static u64 get_ram_cache_blocks(const NetworkStream *stream) {
	if (stream->disk_active()) {
		return (var_is_new3ds ? 8 * 1000 * 1000 : 4 * 1000 * 1000) / NetworkStream::BLOCK_SIZE;
	}
	return MAX_CACHE_BLOCKS;
}

static void disk_cache_remove_stale_files() {
	DIR *dir = opendir(DISK_CACHE_DIR);
	if (!dir) {
		return;
	}
	while (struct dirent *entry = readdir(dir)) {
		std::string name = entry->d_name;
		if (name.size() > 4 && name.substr(name.size() - 4) == ".tmp") {
			remove((DISK_CACHE_DIR + name).c_str());
		}
	}
	closedir(dir);
}

// opens the temporary cache file of the stream (only once). Never called with a lock held.
static void disk_open(NetworkStream *stream) {
	static bool cleaned = false;
	static int counter = 0;
	stream->disk_tried = true;
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/IkersTube", 0777);
	mkdir("sdmc:/3ds/IkersTube/cache", 0777);
	if (!cleaned) { // leftovers of a previous run that crashed
		cleaned = true;
		disk_cache_remove_stale_files();
	}
	stream->disk_path = std::string(DISK_CACHE_DIR) + "s" + std::to_string(counter++) + ".tmp";
	stream->disk_file = fopen(stream->disk_path.c_str(), "w+b");
	if (!stream->disk_file) {
		logger.warning("net/dl", "could not open the disk cache, using RAM only");
	}
}

NetworkStream::~NetworkStream() {
	if (disk_file) {
		fclose(disk_file);
		disk_file = NULL;
		remove(disk_path.c_str());
	}
}

bool NetworkStream::has_block(u64 block) {
	downloaded_data_lock.lock();
	bool res = downloaded_data.count(block) || disk_slots.count(block);
	downloaded_data_lock.unlock();
	return res;
}

// writes a block to the SD card tier (best effort : on any failure the block simply stays RAM-only)
static void disk_store(NetworkStream *stream, u64 block, const std::vector<u8> &data) {
	if (stream->whole_download || stream->disk_write_disabled || !data.size() || data.size() > NetworkStream::BLOCK_SIZE) {
		return;
	}
	if (!stream->disk_file && !stream->disk_tried) {
		disk_open(stream);
	}
	if (!stream->disk_file) {
		return;
	}
	// reserve a slot
	u32 slot;
	stream->downloaded_data_lock.lock();
	if (stream->disk_slots.count(block)) {
		stream->downloaded_data_lock.unlock();
		return;
	}
	if (stream->free_slots.size()) {
		slot = stream->free_slots.back();
		stream->free_slots.pop_back();
	} else if (stream->next_slot < NetworkStream::DISK_MAX_SLOTS) {
		slot = stream->next_slot++;
	} else {
		stream->downloaded_data_lock.unlock();
		return; // window full : keep it in RAM only
	}
	stream->downloaded_data_lock.unlock();

	// write it (nobody can read this slot yet because it is not registered in disk_slots)
	bool ok = false;
	stream->disk_lock.lock();
	if (fseek(stream->disk_file, (long)(slot * NetworkStream::BLOCK_SIZE), SEEK_SET) == 0) {
		ok = fwrite(data.data(), 1, data.size(), stream->disk_file) == data.size();
		if (ok) {
			ok = fflush(stream->disk_file) == 0;
		}
	}
	stream->disk_lock.unlock();

	stream->downloaded_data_lock.lock();
	if (ok) {
		stream->disk_slots[block] = slot;
	} else {
		stream->free_slots.push_back(slot);
		stream->disk_write_disabled = true; // SD card full or removed : stop using it
		logger.warning("net/dl", "disk cache write failed, falling back to RAM only");
	}
	stream->downloaded_data_lock.unlock();
}

void NetworkStream::prune_behind(u64 read_head_block) {
	if (read_head_block <= DISK_BEHIND_BLOCKS + 1) {
		return;
	}
	u64 limit = read_head_block - DISK_BEHIND_BLOCKS; // blocks < limit are not needed anymore (except block 0)
	downloaded_data_lock.lock();
	for (auto itr = disk_slots.begin(); itr != disk_slots.end();) {
		if (itr->first != 0 && itr->first < limit) {
			free_slots.push_back(itr->second);
			itr = disk_slots.erase(itr);
		} else if (itr->first >= limit) {
			break;
		} else {
			itr++;
		}
	}
	for (auto itr = downloaded_data.begin(); itr != downloaded_data.end();) {
		if (itr->first != 0 && itr->first < limit) {
			itr = downloaded_data.erase(itr);
		} else if (itr->first >= limit) {
			break;
		} else {
			itr++;
		}
	}
	downloaded_data_lock.unlock();
}

bool NetworkStream::is_data_available(u64 start, u64 size) {
	if (!ready) {
		return false;
	}
	if (start + size > len) {
		return false;
	}
	u64 end = start + size - 1;
	u64 start_block = start / BLOCK_SIZE;
	u64 end_block = end / BLOCK_SIZE;

	bool res = true;
	downloaded_data_lock.lock();
	for (u64 block = start_block; block <= end_block; block++) {
		if (!downloaded_data.count(block) && !disk_slots.count(block)) {
			res = false;
			break;
		}
	}
	downloaded_data_lock.unlock();
	return res;
}
std::vector<u8> NetworkStream::get_data(u64 start, u64 size) {
	if (!ready) {
		return {};
	}
	if (!size) {
		return {};
	}
	u64 end = start + size - 1;
	u64 start_block = start / BLOCK_SIZE;
	u64 end_block = end / BLOCK_SIZE;
	std::vector<u8> res;
	res.reserve(size);

	for (u64 block = start_block; block <= end_block; block++) {
		u64 cur_l = std::max(start, block * BLOCK_SIZE) - block * BLOCK_SIZE;
		u64 cur_r = std::min(end + 1, (block + 1) * BLOCK_SIZE) - block * BLOCK_SIZE;
		// the lock is held while reading from the SD card so that the slot can't be recycled under our feet
		downloaded_data_lock.lock();
		auto itr = downloaded_data.find(block);
		if (itr != downloaded_data.end()) {
			u64 ram_r = std::min<u64>(cur_r, itr->second.size()); // a short block must not be read past its end
			if (cur_l < ram_r) res.insert(res.end(), itr->second.begin() + cur_l, itr->second.begin() + ram_r);
			downloaded_data_lock.unlock();
			continue;
		}
		auto slot_itr = disk_slots.find(block);
		bool ok = false;
		if (slot_itr != disk_slots.end() && disk_file) {
			size_t old_size = res.size();
			res.resize(old_size + (cur_r - cur_l));
			disk_lock.lock();
			if (fseek(disk_file, (long)(slot_itr->second * BLOCK_SIZE + cur_l), SEEK_SET) == 0) {
				ok = fread(&res[old_size], 1, cur_r - cur_l, disk_file) == cur_r - cur_l;
			}
			disk_lock.unlock();
			if (!ok) {
				res.resize(old_size);
			}
		}
		downloaded_data_lock.unlock();
		if (!ok) {
			break; // data vanished (should not happen) : return what we have, the caller treats it as a short read
		}
	}
	return res;
}

void NetworkStream::set_data(u64 block, std::vector<u8> data) {
	// the SD tier is written first (outside of any lock held by readers for long)
	disk_store(this, block, data);

	downloaded_data_lock.lock();

	// if you move it, it doesn't perform a copy which means you will never get a std::bad_alloc which used to happen before
	downloaded_data[block] = std::move(data);

	// ensure it doesn't cache too much and run out of memory. Evicted blocks are still on the SD card when it is active.
	u64 ram_limit = get_ram_cache_blocks(this);
	while (downloaded_data.size() > ram_limit && downloaded_data.size() > 1) {
		u64 read_head_block = read_head / BLOCK_SIZE;
		if (std::next(downloaded_data.begin())->first < read_head_block) {
			downloaded_data.erase(std::next(downloaded_data.begin()));
		} else {
			downloaded_data.erase(std::prev(downloaded_data.end()));
		}
	}
	downloaded_data_lock.unlock();
}
double NetworkStream::get_download_percentage() {
	downloaded_data_lock.lock();
	size_t count = disk_slots.size();
	for (auto &kv : downloaded_data) {
		if (!disk_slots.count(kv.first)) {
			count++;
		}
	}
	double res = len ? (double)count * BLOCK_SIZE / len * 100 : 0;
	downloaded_data_lock.unlock();
	return res;
}
std::vector<double> NetworkStream::get_buffering_progress_bar(int res_len) {
	downloaded_data_lock.lock();
	std::vector<double> res(res_len);
	if (!len) {
		downloaded_data_lock.unlock();
		return res;
	}
	for (int i = 0; i < res_len; i++) {
		u64 l = (u64)len * i / res_len;
		u64 r = std::min<u64>(len, len * (i + 1) / res_len);
		if (r <= l) {
			continue;
		}
		for (u64 block = l / BLOCK_SIZE; block <= (r - 1) / BLOCK_SIZE; block++) {
			if (!downloaded_data.count(block) && !disk_slots.count(block)) {
				continue;
			}
			u64 il = block * BLOCK_SIZE;
			u64 ir = std::min((block + 1) * BLOCK_SIZE, len);
			res[i] += std::min(ir, r) - std::max(il, l);
		}
		res[i] /= r - l;
		res[i] *= 100;
	}
	downloaded_data_lock.unlock();
	return res;
}

// --------------------------------
// NetworkStreamDownloader implementation
// --------------------------------

void NetworkStreamDownloader::add_stream(NetworkStream *stream) {
	streams_lock.lock();
	size_t index = (size_t)-1;
	for (size_t i = 0; i < streams.size(); i++) {
		if (!streams[i]) {
			streams[i] = stream;
			index = i;
			break;
		}
	}
	if (index == (size_t)-1) {
		index = streams.size();
		streams.push_back(stream);
	}
	streams_lock.unlock();
}

static bool thread_network_session_list_inited = false;
static NetworkSessionList thread_network_session_list;
static void confirm_thread_network_session_list_inited() {
	if (!thread_network_session_list_inited) {
		thread_network_session_list_inited = true;
		thread_network_session_list.init();
	}
}

static std::string remove_url_parameter(const std::string &url, const std::string &param) {
	std::string res;
	for (size_t i = 0; i < url.size();) {
		if (url[i] == '&' || url[i] == '?') {
			if (url.substr(i + 1, param.size() + 1) == param + "=") {
				i = std::find(url.begin() + i + 1, url.end(), '&') - url.begin();
			} else {
				res.push_back(url[i++]);
			}
		} else {
			res.push_back(url[i++]);
		}
	}
	return res;
}

#define LOG_THREAD_STR "net/dl"
void NetworkStreamDownloader::downloader_thread() {
	// Called when a block download failed. Retries with exponential backoff and alternates the range method
	// (query parameter <-> Range header) because some failures only affect one of them. The stream is declared dead
	// only after NetworkStream::RETRY_CNT_MAX consecutive failures.
	auto register_failure = [&](NetworkStream *stream, const std::string &reason) {
		logger.error(LOG_THREAD_STR, "block download failed (" + reason + ")");
		if (stream->retry_cnt_left > 0) {
			stream->retry_cnt_left--;
			stream->use_header_range = !stream->use_header_range;
			int attempt = NetworkStream::RETRY_CNT_MAX - stream->retry_cnt_left; // 1, 2, 3, ...
			int delay_ms = std::min(250 << std::min(attempt - 1, 4), 3000);
			for (int waited = 0; waited < delay_ms && !thread_exit_requested && !stream->quit_request; waited += 50) {
				usleep(50000);
			}
		} else {
			stream->error = true;
		}
	};
	while (!thread_exit_requested) {
		size_t cur_stream_index =
		    (size_t)-1; // the index of the stream on which we will perform a download in this loop
		streams_lock.lock();
		// back up 'read_head's as those can be changed from another thread
		std::vector<u64> read_heads(streams.size());
		for (size_t i = 0; i < streams.size(); i++) {
			if (streams[i]) {
				read_heads[i] = streams[i]->read_head;
			}
		}

		// find the stream to download next
		double margin_percentage_min = 1000;
		for (size_t i = 0; i < streams.size(); i++) {
			if (!streams[i]) {
				continue;
			}
			if (streams[i]->quit_request) {
				delete streams[i];
				streams[i] = NULL;
				continue;
			}
			if (streams[i]->error) {
				continue;
			}
			if (streams[i]->suspend_request) {
				continue;
			}
			if (!streams[i]->ready) {
				cur_stream_index = i;
				break;
			}
			if (streams[i]->whole_download) {
				continue; // its entire content should already be downloaded
			}

			// sliding window : with the SD tier we keep ~25 MB ahead of the read head and drop what is far behind;
			// without it we are limited by the RAM cache
			int forward_buffer_block_num =
			    streams[i]->disk_active()
			        ? std::max<int>(2, NetworkStream::DISK_FORWARD_BLOCKS * var_forward_buffer_ratio)
			        : std::max<int>(2, (MAX_CACHE_BLOCKS - 1) * var_forward_buffer_ratio); // block #0 is always kept
			u64 read_head_block = read_heads[i] / BLOCK_SIZE;
			if (streams[i]->ready) {
				streams[i]->prune_behind(read_head_block);
			}
			u64 first_not_downloaded_block = read_head_block;
			while (first_not_downloaded_block < streams[i]->block_num &&
			       streams[i]->has_block(first_not_downloaded_block)) {
				first_not_downloaded_block++;
				if (first_not_downloaded_block == read_head_block + forward_buffer_block_num) {
					break;
				}
			}
			if (first_not_downloaded_block == streams[i]->block_num) {
				continue;
			}
			if (first_not_downloaded_block == read_head_block + forward_buffer_block_num) {
				continue; // no need to download this stream for now
			}

			double margin_percentage;
			if (first_not_downloaded_block == read_head_block) {
				margin_percentage = 0;
			} else {
				margin_percentage =
				    (double)(first_not_downloaded_block * BLOCK_SIZE - read_heads[i]) / streams[i]->len * 100;
			}
			if (margin_percentage_min > margin_percentage) {
				margin_percentage_min = margin_percentage;
				cur_stream_index = i;
			}
		}

		if (cur_stream_index == (size_t)-1) {
			streams_lock.unlock();
			usleep(20000);
			continue;
		}
		NetworkStream *cur_stream = streams[cur_stream_index];
		streams_lock.unlock();

		confirm_thread_network_session_list_inited();
		// whole download
		if (cur_stream->whole_download) {
			auto &session_list = cur_stream->session_list ? *cur_stream->session_list : thread_network_session_list;
			auto result = session_list.perform(HttpRequest::GET(cur_stream->url, {}));
			if (result.redirected_url != "") {
				cur_stream->url = result.redirected_url;
			}

			if (!result.fail && result.status_code_is_success() && result.data.size()) {
				{ // acquire necessary headers
					char *end;
					auto value = result.get_header("x-head-seqnum");
					cur_stream->seq_head = strtoll(value.c_str(), &end, 10);
					if (*end || !value.size()) {
						logger.error("net/dl", "failed to acquire x-head-seqnum");
						cur_stream->seq_head = -1;
						cur_stream->error = true;
					}
					value = result.get_header("x-sequence-num");
					cur_stream->seq_id = strtoll(value.c_str(), &end, 10);
					if (*end || !value.size()) {
						logger.error("net/dl", "failed to acquire x-sequence-num");
						cur_stream->seq_id = -1;
						cur_stream->error = true;
					}
				}
				if (!cur_stream->error) {
					cur_stream->len = result.data.size();
					cur_stream->block_num = (cur_stream->len + BLOCK_SIZE - 1) / BLOCK_SIZE;
					for (size_t i = 0; i < result.data.size(); i += BLOCK_SIZE) {
						size_t left = i;
						size_t right = std::min<size_t>(i + BLOCK_SIZE, result.data.size());
						cur_stream->set_data(i / BLOCK_SIZE,
						                     std::vector<u8>(result.data.begin() + left, result.data.begin() + right));
					}
					cur_stream->ready = true;
				}
			} else {
				logger.error("net/dl", "failed accessing : " + result.error);
				cur_stream->error = true;
				switch (result.status_code) {
				// these codes are returned when trying to read beyond the end of the livestream
				case HTTP_STATUS_CODE_NO_CONTENT:
				case HTTP_STATUS_CODE_NOT_FOUND:
					cur_stream->livestream_eof = true;
					break;
				// this code is returned when trying to read an ended livestream without archive
				case HTTP_STATUS_CODE_FORBIDDEN:
					cur_stream->livestream_private = true;
					break;
				}
			}
		} else {
			u64 block_reading = read_heads[cur_stream_index] / BLOCK_SIZE;
			if (cur_stream->ready) {
				while (block_reading < cur_stream->block_num && cur_stream->has_block(block_reading)) {
					block_reading++;
				}
				if (block_reading == cur_stream->block_num) { // something unexpected happened
					logger.error(LOG_THREAD_STR, "unexpected error (trying to read beyond the end of the stream)");
					cur_stream->error = true;
					continue;
				}
			}
			// Util_log_save("net/dl", "dl next : " + std::to_string(cur_stream_index) + " " +
			// std::to_string(block_reading));

			if (cur_stream->url.compare(0, 6, "local:") == 0) {
				// a file of the SD card (offline playback) : blocks are read from the file instead of the network
				std::string path = cur_stream->url.substr(6);
				FILE *local_file = fopen(path.c_str(), "rb");
				if (!local_file) {
					logger.error("net/dl", "cannot open the local file : " + path);
					cur_stream->error = true;
					continue;
				}
				fseek(local_file, 0, SEEK_END);
				long local_size = ftell(local_file);
				if (local_size < 0) {
					fclose(local_file);
					cur_stream->error = true;
					continue;
				}
				u64 total = (u64)local_size;
				if (cur_stream->len == 0) {
					cur_stream->len = total;
					cur_stream->block_num = NetworkStream::get_block_num(total);
				}
				u64 local_start = block_reading * BLOCK_SIZE;
				if (local_start >= total) {
					fclose(local_file);
					cur_stream->error = true;
					continue;
				}
				u64 local_end = std::min((block_reading + 1) * BLOCK_SIZE, total);
				std::vector<u8> local_data(local_end - local_start);
				fseek(local_file, (long)local_start, SEEK_SET);
				size_t got = fread(local_data.data(), 1, local_data.size(), local_file);
				fclose(local_file);
				if (got != local_data.size()) {
					register_failure(cur_stream, "local file read failed");
					continue;
				}
				cur_stream->retry_cnt_left = NetworkStream::RETRY_CNT_MAX;
				cur_stream->set_data(block_reading, std::move(local_data));
				cur_stream->ready = true;
				continue;
			}

			u64 start = block_reading * BLOCK_SIZE;
			u64 end = cur_stream->ready ? std::min((block_reading + 1) * BLOCK_SIZE, cur_stream->len)
			                            : (block_reading + 1) * BLOCK_SIZE;
			u64 expected_len = end - start;

			auto &session_list = cur_stream->session_list ? *cur_stream->session_list : thread_network_session_list;
			// length not sure -> use Range header to get the size (slower)
			const std::string range_value = std::to_string(start) + "-" + std::to_string(end - 1);
			auto result = (cur_stream->len == 0 || cur_stream->use_header_range)
			                  ? session_list.perform(HttpRequest::GET(cur_stream->url, {{"Range", "bytes=" + range_value}}))
			                  : session_list.perform(HttpRequest::GET(cur_stream->url + "&range=" + range_value, {}));
			if (result.redirected_url != "") {
				cur_stream->url = remove_url_parameter(result.redirected_url, "range");
			}

			if (!result.fail && result.status_code_is_success()) {
				if (cur_stream->len == 0) {
					auto content_range_str = result.get_header("Content-Range");
					char *slash = strchr(content_range_str.c_str(), '/');
					bool ok = false;
					if (slash) {
						char *end;
						cur_stream->len = strtoll(slash + 1, &end, 10);
						if (!*end) {
							ok = true;
							cur_stream->block_num = NetworkStream::get_block_num(cur_stream->len);
						} else {
							logger.error(LOG_THREAD_STR, "failed to parse Content-Range : " + std::string(slash + 1));
						}
					} else {
						logger.error(LOG_THREAD_STR,
						             "no slash in Content-Range response header : " + content_range_str);
					}
					if (!ok) {
						cur_stream->error = true;
					}
				}
				if (cur_stream->ready && result.data.size() != expected_len) {
					register_failure(cur_stream, "size discrepancy : " + std::to_string(expected_len) + " -> " +
					                                 std::to_string(result.data.size()));
					continue;
				}
				cur_stream->retry_cnt_left = NetworkStream::RETRY_CNT_MAX;
				cur_stream->set_data(block_reading, std::move(result.data)); // data isn't accessed further down so we can move it
				cur_stream->ready = true;
			} else if (!result.fail) {
				int code = result.status_code;
				// 403/408/429/5xx are frequently transient with googlevideo -> retry; anything else is fatal
				if (code == 403 || code == 408 || code == 429 || code >= 500) {
					register_failure(cur_stream, "http " + std::to_string(code));
				} else {
					logger.error("net/dl", "stream returned: " + std::to_string(code));
					cur_stream->error = true;
				}
			} else {
				register_failure(cur_stream, "access failed : " + result.error);
			}
		}
	}
	logger.info(LOG_THREAD_STR, "Exit, deiniting...");
	for (auto stream : streams) {
		if (stream) {
			stream->quit_request = true;
		}
	}
}
void NetworkStreamDownloader::delete_all() {
	for (auto &stream : streams) {
		delete stream;
		stream = NULL;
	}
}

// --------------------------------
// other functions implementation
// --------------------------------

void network_downloader_thread(void *downloader_) {
	NetworkStreamDownloader *downloader = (NetworkStreamDownloader *)downloader_;

	downloader->downloader_thread();

	threadExit(0);
}
