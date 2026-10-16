#pragma once
#include <vector>
#include <map>
#include <set>
#include <cstdio>
#include <string>
#include <3ds.h>
#include "system/libctru_wrapper.hpp"
#include "network_io.hpp"

// one instance per one url (once constructed, the url is not changeable)
struct NetworkStream {
	static constexpr u64 BLOCK_SIZE = 0x40000; // 256 KiB
	static constexpr u64 NEW3DS_MAX_CACHE_BLOCKS = 32 * 1000 * 1000 / BLOCK_SIZE;       // ~2-3 min of 360p video
	static constexpr u64 NEW3DS_SAFE_CACHE_BLOCKS = 12 * 1000 * 1000 / BLOCK_SIZE; // used when RAM runs low
	static constexpr u64 OLD3DS_MAX_CACHE_BLOCKS = 4 * 1000 * 1000 / BLOCK_SIZE;
	// SD card tier : blocks are also written to a temporary file so that the buffer can reach tens of MB without
	// using RAM. It works as a sliding window : data far behind the read head is dropped, data ahead keeps loading.
	static constexpr u64 DISK_FORWARD_BLOCKS = 25 * 1000 * 1000 / BLOCK_SIZE; // ~25 MB ahead of the read head
	static constexpr u64 DISK_BEHIND_BLOCKS = 8;                              // ~2 MB kept behind (small rewinds)
	static constexpr u64 DISK_MAX_SLOTS = DISK_FORWARD_BLOCKS + DISK_BEHIND_BLOCKS + 40;
	static constexpr int RETRY_CNT_MAX = 8; // consecutive failures tolerated before the stream is declared dead
	static u64 get_block_num(u64 size) { return (size + BLOCK_SIZE - 1) / BLOCK_SIZE; }

	std::string url;
	Mutex downloaded_data_lock; // std::map needs locking when searching and inserting at the same time
	u64 len = 0;
	u64 block_num = 0;
	std::map<u64, std::vector<u8>> downloaded_data;
	bool whole_download = false;
	NetworkSessionList *session_list = NULL;

	// anything above here is not supposed to be used from outside network_downloader.cpp and network_downloader.hpp
	volatile bool ready = false;
	volatile bool suspend_request = false;
	volatile bool quit_request = false;
	volatile bool error = false;
	volatile int retry_cnt_left = RETRY_CNT_MAX;
	volatile bool use_header_range = false; // alternate between '&range=' query and 'Range:' header on retries
	volatile u64 read_head = 0;
	const char *volatile network_waiting_status = NULL;
	bool disable_interrupt = false;
	// used for livestreams
	int seq_head = -1;
	int seq_id = -1;
	bool livestream_eof = false;
	bool livestream_private = false;
	bool read_dead_tried = false;

	// if `whole_download` is true, it will not use Range request but download the whole content at once (used for
	// livestreams)
	NetworkStream(std::string url, int64_t len, bool whole_download, NetworkSessionList *session_list)
	    : url(url), len(len < 0 ? 0 : len), block_num(get_block_num(this->len)), whole_download(whole_download),
	      session_list(session_list) {}

	// disk tier state. disk_slots/free_slots/next_slot are guarded by downloaded_data_lock, file I/O by disk_lock
	// (lock order : downloaded_data_lock -> disk_lock).
	FILE *disk_file = NULL;
	bool disk_tried = false;
	volatile bool disk_write_disabled = false;
	Mutex disk_lock;
	std::map<u64, u32> disk_slots; // block -> slot index in the file
	std::vector<u32> free_slots;
	u32 next_slot = 0;
	std::string disk_path;
	~NetworkStream();
	bool disk_active() const { return disk_file != NULL && !disk_write_disabled; }
	bool has_block(u64 block); // in RAM or on the SD card
	void prune_behind(u64 read_head_block); // drop blocks far behind the read head (keeps block 0)

	double get_download_percentage();
	std::vector<double> get_buffering_progress_bar(int res_len);

	// check if the data of the current stream of range [start, start + size) is already downloaded and available
	bool is_data_available(u64 start, u64 size);

	// this function must only be called when is_data_available(start, size) returns true
	// returns the data of the stream of range [start, start + size)
	std::vector<u8> get_data(u64 start, u64 size);

	// this function is supposed to be called from NetworkStreamDownloader::*
	void set_data(u64 block, std::vector<u8> data);
};

// each instance of this class is paired with one downloader thread
// it owns NetworkStream instances, and the one with the least margin (as in proportion to the length of the entire
// stream) is the target of next downloading
class NetworkStreamDownloader {
  private:
	static constexpr u64 BLOCK_SIZE = NetworkStream::BLOCK_SIZE;
	static constexpr const char *USER_AGENT = "Mozilla/5.0 (Linux; Android 11; Pixel 3a) AppleWebKit/537.36 (KHTML, "
	                                          "like Gecko) Chrome/83.0.4103.101 Mobile Safari/537.36";

	Mutex streams_lock;
	std::vector<NetworkStream *> streams;

	bool thread_exit_requested = false;

  public:
	NetworkStreamDownloader() = default;

	// the pointer must be one that has been new-ed : it will be deleted once quit_request is made
	void add_stream(NetworkStream *stream);

	void request_thread_exit() { thread_exit_requested = true; }
	void delete_all();

	void downloader_thread();
};
// 'arg' should be a pointer to an instance of NetworkStreamDownloader
void network_downloader_thread(void *arg);
