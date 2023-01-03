/*
	game_server - The minesweeper game server

	    Notice: This program is designed to be launched by the judger(judger.cpp).
	Please do not launch it directly.

		It accepts the following envariables: MINESWEEPER_LAUNCHED_BY_JUDGER (must present),
	MINESWEEPER_MAP_FILE_PATH, MINESWEEPER_FD_GS_TO_PL, MINESWEEPER_FD_GS_FROM_PL,
	MINESWEEPER_FD_GS_TO_JU, MINESWEEPER_FD_GS_FROM_JU.
		It first parses those envariables and reads the map from the file
	indicated by MINESWEEPER_MAP_FILE_PATH. Then it begins to interact with
	the player's program.
		When the player's program sents an 'C' (stands for "Create Channel"),
	the game server creates a new channel and responses with the channel ID.
		When the player's program exits or the time is up, the judger sents an 'F'
	(stands for "Finished") character to the game server (received through
	`fd_from_ju`), and then the game server will send the number of opened
	non-mine grids and opened is-mine grids to the judger (sent through
	`fd_to_ju`)

	Overall Design:
		The main thread is responsible for listening to `fd_from_ju` and `fd_from_pl`
	at the same time (by I/O multiplexing). When the judger sents an 'F', the
	game server responses with the result (see above). When the player's program
	sents an 'C', it creates a new thread and let that thread handle that Channel.
		Each channel has a unique shared memory region. The player's program
	uses that region to communicate with the game server. (Note. we use shared
	memory instead of pipe/FIFO/message-queue to avoid overhead introduced by
	system calls).
		Each channel is handled by a particular thread, which we called "worker
	thread". The worker thread monitors the channel (namely, monitors the shared
	memory region). The worker thread acts like a "two-phase lock". After creation
	or completing a `click` request, the worker thread first spins for a certain
	times. If it did not detect further requests, it calls `futex_wait` and
	begins to sleep, until a new request arrives. We use this mechanism to ensure:
		- If the channel is busy (i.e. the player's program uses the channel
		frequently), then the worker thread just spins in the interval between
		two requests, achieving a better latency.
		- If the channel is not busy (i.e. the player's program only uses the
		channel from time to time), then if we still use a spin lock, we would
		waste a lot of CPU cycles (wasteful!). So we just use `futex_wait`, and
		let the kernel wake the worker thread up when the channel is active
		again, which doesn't lose much performance.

	Memory layout of a channel:
		Each channel has a shared memory (shm) region of `CHANNEL_SHM_SIZE` bytes,
	where `CHANNEL_SHM_SIZE` is defined in `common.h`. 
		Memory layout:
		- 1 byte: `pending bit`. When the player's program wants to click, it sets this
			bit to 1 first.
		- 1 byte: `sleeping bit`. When the game server is about to enter the second phase
			(futex_wait), it sets this bit to 1. When the player's program wants to click,
			it tests whether this bit is 1. If it is 1 then the player's program will
			call `futex_wake()`.
		- 1 byte: `done bit`. When the game server completes the request, it
			sets this bit to 1.
		- 1 bytes `skip_when_reopen bit`. If it is 1 and the target grid of the
			current request has been opened before, then the game server will
			put -2 （or -3, if the grid contains a mine） in "bytes indicating
			how many grids are opened" and returns immediately.
		- 2 bytes for click_r
		- 2 bytes for click_c
		- 4 bytes indicating how many grids are opened (-1 if the grid contains a mine,
			-2 if `re_report bit` is 0 and the target grid of the current request
			has been opened before)
		- 2 bytes r1, 2 bytes c1, 2 bytes number in grid (r1, c1)
		- 2 bytes r2, 2 bytes c2, 2 bytes number in grid (r2, c2)
		- ...
		- 2 bytes rK, 2 bytes cK, 2 bytes number in grid (rK, cK)
*/
#include <atomic>
#include <utility>
#include <vector>
#include <climits>
#include <stdatomic.h>
#include <functional>
#include "lib/wrappers.h"
#include "lib/log.h"
#include "lib/common.h"
#include "lib/shm.h"
#include "lib/futex.h"
#include "lib/queue.h"
using std::atomic_flag, std::atomic, std::atomic_compare_exchange_strong;
using std::pair, std::vector;
using std::max, std::min;

void kill_worker_threads();
void report_error_to_judger(const char* error_s);

// The upper limit for active (doing BFS) worker threads
// We need this because that every worker thread which is doing BFS needs a
// vis[] array. If we allocate a unique vis[] array for each thread, then
// the memory usage would be unacceptable. Instead, we just allocate a vis[]
// array with NUM_ACTIVE_WORKER_THREAD*N*N bits, and when a thread wants
// to BFS, it just use N*N bits from the array.
constexpr int NUM_ACTIVE_WORKER_THREAD = 8;

// The number of thread for `summarize()`
constexpr int NUM_SUMMARIZE_THREAD = 8;	

// The spin amount in the two phase lock.
constexpr int TWO_PHASE_LOCK_SPIN_AMUONT = 2048;

long N, K, logN;	// The size of the map, the number of mines
char* map_file_path;	// path to the map file
int fd_to_pl, fd_from_pl;	// fds (used to communicate with player's program)
int fd_to_ju, fd_from_ju;	// fds (used to communicate with the judger)

char* shm_start;	// Point to the head of the shared memory region

char* is_mine;	// A large bit array, representing the map.
inline char test_is_mine(long r, long c) {
	if (r < 0 || c < 0 || r >= N || c >= N) return 0;
	long index = (r<<logN) + c;
	long number = index/8, offset = index%8;
	return is_mine[number]>>offset&0x1;
}
inline char get_adj_mine(long r, long c) {
	return test_is_mine(r-1, c-1) + test_is_mine(r-1, c) + test_is_mine(r-1, c+1)
		+  test_is_mine(r, c-1) + test_is_mine(r, c+1)
		+  test_is_mine(r+1, c-1) + test_is_mine(r+1, c) + test_is_mine(r+1, c+1);
}

char* is_open;	// A large bit array, representing whether the grid is opened by the player
inline char test_is_open(long r, long c) {
	if (r < 0 || c < 0 || r >= N || c >= N) return 0;
	long index = (r<<logN) + c;
	long number = index/8, offset = index%8;
	return is_open[number]>>offset&0x1;
}
inline void set_is_open(long r, long c) {
	long index = (r<<logN) + c;
	long number = index/8, offset = index%8;
	__atomic_or_fetch(is_open+number, 0x1<<offset, __ATOMIC_RELAXED);
	// is_open[number] |= 0x1<<offset;	// Data race
}

/*
 * Functions for initialization
 */

// read and parse necessary env variables
void read_env_vars() {
	if (!Getenv("MINESWEEPER_LAUNCHED_BY_JUDGER")) {
		app_error("This program (game server) is designed to be launched by \
the judger. Please do not launch it directly.");
	}
	map_file_path = Getenv_must_exist("MINESWEEPER_MAP_FILE_PATH");
	fd_to_pl = atoi(Getenv_must_exist("MINESWEEPER_FD_GS_TO_PL"));
	fd_from_pl = atoi(Getenv_must_exist("MINESWEEPER_FD_GS_FROM_PL"));
	fd_to_ju = atoi(Getenv_must_exist("MINESWEEPER_FD_GS_TO_JU"));
	fd_from_ju = atoi(Getenv_must_exist("MINESWEEPER_FD_GS_FROM_JU"));
}

// read and parse the map
void read_map() {
	FILE* map_file = fopen(map_file_path, "r");
	if (!map_file) {
		unix_error("Failed to open the map file");
	}

	if (fscanf(map_file, "%ld %ld\n", &N, &K) != 2) {
		app_error("Failed to read N and K. Maybe the map file is broken?");
	}

	log("Map info: N = %ld, K = %ld\n", N, K);
	logN = (long)(log2((double)N)+0.01);

	Fseek(map_file, 0, SEEK_SET);
	char c;
	while ((c = fgetc(map_file)) != '\n');

	is_mine = (char*)Malloc(N*N/8);
	size_t byte_read = fread(is_mine, 1, N*N/8, map_file);
	if (byte_read != N*N/8) {
		log("%u bytes read\n", byte_read);
		app_error("Failed to read the map. Maybe the map file is broken?");
	}

	Fclose(map_file);
}


/*
 * Functions for summarization
 */

// summarize_thread_routine: thread routine used in `summarize()`
// The i-th thread is responsible for rows in
// [thread_id*(N/NUM_SUMMARIZE_THREAD), (thread_id+1)*(N/NUM_SUMMARIZE_THREAD))
void* summarize_thread_routine(void* arg) {
	long thread_id = (long)arg;
	long row_start = thread_id*(N/NUM_SUMMARIZE_THREAD);
	long row_end = (thread_id+1)*(N/NUM_SUMMARIZE_THREAD);
	long index_start = row_start*N/8;
	long index_end = row_end*N/8;
	// we only need to examine elements with index within [index_start, index_end)
	long cnt_non_mine = 0;
	long cnt_is_mine = 0;
	for (long i = index_start; i < index_end; ++i) {
		cnt_non_mine += __builtin_popcount((uint8_t)(~is_mine[i]&is_open[i]));
		cnt_is_mine += __builtin_popcount((uint8_t)(is_mine[i]&is_open[i]));
	}
	pair<long, long>* result = (pair<long, long>*)Malloc(sizeof(pair<long, long>));
	result->first = cnt_non_mine;
	result->second = cnt_is_mine;
	return result;
}

// summarize - Send the number of opened non-mine grids and opened is-mine
// grids to the judger, through fd_to_ju
void summarize() {
	kill_worker_threads();
	if (N%NUM_SUMMARIZE_THREAD != 0) {
		app_error("NUM_SUMMARIZE must be a factor of N.");
	}
	// Create the threads for counting
	pthread_t tids[NUM_SUMMARIZE_THREAD];
	for (int i = 0; i < NUM_SUMMARIZE_THREAD; ++i) {
		Pthread_create(tids+i, NULL, summarize_thread_routine, (void*)(long)i);
	}
	// Join those threads
	long cnt_non_mine = 0;
	long cnt_is_mine = 0;
	for (int i = 0; i < NUM_SUMMARIZE_THREAD; ++i) {
		pair<long, long>* result;
		Pthread_join(tids[i], (void**)&result);
		cnt_non_mine += result->first;
		cnt_is_mine += result->second;
	}
	// Send it to the judger, via fd_to_ju
	// Format: "Status N K cnt_non_mine cnt_is_mine"
	char buf[128];
	sprintf(buf, "%d %ld %ld %ld %ld",
		0, N, K, cnt_non_mine, cnt_is_mine);
	Write(fd_to_ju, buf, strlen(buf));
	// Clean up and exit
	Free(is_mine);
	Free(is_open);
	exit(0);
}

/*
 * Functions and variables for worker threads
 */

// A vector for maintaining all the tids of worker threads.
pthread_mutex_t worker_thread_tids_mutex = PTHREAD_MUTEX_INITIALIZER;
vector<pthread_t> worker_thread_tids;
void add_worker_thread_tid(pthread_t tid) {
	Pthread_mutex_lock(&worker_thread_tids_mutex);
	worker_thread_tids.push_back(tid);
	Pthread_mutex_unlock(&worker_thread_tids_mutex);
}

// The channel_id of the next channel, starting from 0
atomic<int> next_channel_id = 0;

// For BFS. Mark whether a grid is visited
// We call `vis[i]` as "The i-th level"
char* vis[NUM_ACTIVE_WORKER_THREAD];
char test_is_vis(int level, long r, long c) {
	if (r < 0 || c < 0 || r >= N || c >= N) return 1;
	long index = (r<<logN) + c;
	long number = index/8, offset = index%8;
	return vis[level][number]>>offset&0x1;
}
void set_is_vis(int level, long r, long c) {
	long index = (r<<logN) + c;
	long number = index/8, offset = index%8;
	vis[level][number] |= 0x1<<offset;
}
void unset_is_vis_area(int level, long r, long c) {
	long index = (r<<logN) + c;
	long number = index/8;
	vis[level][number] = 0;
}

// Queues for BFS
Queue queues[NUM_ACTIVE_WORKER_THREAD];

// For BFS. `vis_occupied_flags[i]` marks whether `vis[i]` is in use by some thread
atomic_flag vis_occupied_flags[NUM_ACTIVE_WORKER_THREAD];

// find_available_level - find a available in `vis[]`, by spinning around `vis_occupied_flags`
int find_available_level() {
	while (true) {
		for (int i = 0; i < NUM_ACTIVE_WORKER_THREAD; ++i) {
			if (vis_occupied_flags[i].test_and_set() == 0)
				return i;
		}
	}
}
// release_level - release a level
void release_level(int level) {
	vis_occupied_flags[level].clear();
}

void worker_thread_bfs(
	int click_r, int click_c,
	const int level,
	long &result_open_count,
	unsigned short result_arr[MAX_OPEN_GRID][3]
) {
	static constexpr int delta_xy[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};
	Queue &q = queues[level];
	std::function<void(long, long)> append_to_result = [&](long r, long c) {
		result_arr[result_open_count][0] = r;
		result_arr[result_open_count][1] = c;
		result_arr[result_open_count][2] = get_adj_mine(r, c);
		result_open_count += 1;
	};
	result_open_count = 0;
	q.clear();
	q.push(click_r, click_c);
	set_is_vis(level, click_r, click_c);
	append_to_result(click_r, click_c);
	while (!q.empty()) {
		int r, c;
		q.pop(r, c);
		for (int k = 0; k < 8; ++k) {
			int new_r = r + delta_xy[k][0];
			int new_c = c + delta_xy[k][1];
			if (!test_is_vis(level, new_r, new_c) && !test_is_mine(new_r, new_c)) {
				append_to_result(new_r, new_c);
				set_is_vis(level, new_r, new_c);
				if (!get_adj_mine(new_r, new_c)) {
					q.push(new_r, new_c);
				}
			}
		}
	}
	// Clean up vis[level]
	for (int i = 0; i < result_open_count; ++i) {
		unset_is_vis_area(level, result_arr[i][0], result_arr[i][1]);
	}
	// Open those grids
	for (int i = 0; i < result_open_count; ++i) {
		set_is_open(result_arr[i][0], result_arr[i][1]);
	}
}

// worker_thread_routine - Thread routine for a worker thread
void* worker_thread_routine(void* arg) {
	add_worker_thread_tid(Pthread_self());

	// Create a new channel
	int channel_id = next_channel_id.fetch_add(1);
	if (channel_id >= MAX_CHANNEL) {
		char buf[128];
		sprintf(buf, "Error! The player's program has opened too many channels. Limit: %d", MAX_CHANNEL);
		report_error_to_judger(buf);
		kill_worker_threads();
		return NULL;
	}
	char* shm_pos = shm_start + CHANNEL_SHM_SIZE*channel_id;
	init_shm_region(shm_pos);

	// Response to player's program with the channel ID (through fd_to_pl)
	char buf[16];
	sprintf(buf, "%d", channel_id);
	Write(fd_to_pl, buf, strlen(buf));

	// printf("is_mine %d\n", test_is_mine(1, 1));
	// Go to 996!
	while (true) {
		// The two phase lock
		// First we spin for a while
		bool flag = false;
		for (int i = 0; i < TWO_PHASE_LOCK_SPIN_AMUONT; ++i) {
			if (SHM_PENDING_BIT(shm_pos)) {
				flag = true;
				break;
			}
		}
		// If we still cannot grab the lock, we use `futex`
		if (!flag) {
			SHM_SLEEPING_BIT(shm_pos) = 1;
			while (true) {
				if (SHM_PENDING_BIT(shm_pos) == 1) {
					break;
				}
				futex_wait(SHM_PENDING_BIT_PTR(shm_pos), 0);
			}
		}
		// I'm wake up
		// Cleanup
		SHM_PENDING_BIT(shm_pos) = 0;
		SHM_SLEEPING_BIT(shm_pos) = 0;
		// There is a new `click()` request
		long click_r = SHM_CLICK_R(shm_pos);
		long click_c = SHM_CLICK_C(shm_pos);
		bool skip_when_reopen = SHM_SKIP_WHEN_REOPEN_BIT(shm_pos);
		
		if (skip_when_reopen && test_is_open(click_r, click_c)) {
			// This grid has been opened before, and the player's program says
			// that "If the grid has been opened before, plz skip it"
			// So we just put -2 (non-mine) or -3 (is-mine) into SHM_OPEN_GRID_COUNT and return
			SHM_OPENED_GRID_COUNT(shm_pos) = test_is_mine(click_r, click_c) ? -3 : -2;
		} else {
			if (test_is_mine(click_r, click_c)) {
				// This grid contains a mine, BOOM SHAKALAKA!
				set_is_open(click_r, click_c);
				SHM_OPENED_GRID_COUNT(shm_pos) = -1;
			} else if (get_adj_mine(click_r, click_c)) {
				// This grid contains a non-zero number
				set_is_open(click_r, click_c);
				SHM_OPENED_GRID_COUNT(shm_pos) = 1;
				(*SHM_OPENED_GRID_ARR(shm_pos))[0][0] = click_r;
				(*SHM_OPENED_GRID_ARR(shm_pos))[0][1] = click_c;
				(*SHM_OPENED_GRID_ARR(shm_pos))[0][2] = get_adj_mine(click_r, click_c);
			} else {
				// This grid contains zero
				// BFS is needed
				int level = find_available_level();
				long result_open_count = 0;
				worker_thread_bfs(
					click_r, click_c, level, result_open_count,
					*SHM_OPENED_GRID_ARR(shm_pos));
				SHM_OPENED_GRID_COUNT(shm_pos) = result_open_count;
				release_level(level);
			}
		}

		// Done
		SHM_DONE_BIT(shm_pos) = 1;
	}

	return NULL;
}


/*
 * Functions and variables for the main thread
 */

// Kill all worker threads and report some error to the judger
void kill_worker_threads() {
	Pthread_mutex_lock(&worker_thread_tids_mutex);
	for (pthread_t tid : worker_thread_tids) {
		// Pthread_kill(tid, SIGKILL); TODO
	}
	Pthread_mutex_unlock(&worker_thread_tids_mutex);
}

// Send something to the judger
void report_error_to_judger(const char* error_s) {
	Pthread_mutex_lock(&worker_thread_tids_mutex);
	Write(fd_to_ju, error_s, strlen(error_s));
	Pthread_mutex_unlock(&worker_thread_tids_mutex);
}

// main_thread_routine - Thread routine for the main thread.
// (Actually this function is not a "thread routine" because it is not used
// as an argument for `pthread_create`)
void main_thread_routine() {
	static constexpr int BUF_LEN = 16;
	char buf[BUF_LEN];
	fd_set monitor_fd_set;	// fd set for I/O multiplexing, including fd_from_pl and fd_from_ju
	bool fd_from_pl_eofed = false;	// Whether the player has closed the connection
	while (true) {
		FD_ZERO(&monitor_fd_set);
		if (!fd_from_pl_eofed)
			FD_SET(fd_from_pl, &monitor_fd_set);
		FD_SET(fd_from_ju, &monitor_fd_set);
		Select(max(fd_from_ju, fd_from_pl)+1, &monitor_fd_set, NULL, NULL, NULL);
		if (FD_ISSET(fd_from_pl, &monitor_fd_set)) {
			// Received something from the player's program
			int t = Read(fd_from_pl, buf, BUF_LEN);
			if (t == 0) {
				fd_from_pl_eofed = true;
				continue;
			}
			switch (buf[0]) {
				case 'C':
					// "I want to create a new channel"
					pthread_t tid;
					Pthread_create(&tid, NULL, worker_thread_routine, NULL);
					break;
				default:
					log("Error! Received something unknown from the player's program: %c (ASCII=%d)\n", buf[0], int(buf[0]));
					log(this_is_a_bug_str);
					exit(1);
			}
		} else if (FD_ISSET(fd_from_ju, &monitor_fd_set)) {
			// Received something from the judger
			Read(fd_from_ju, buf, BUF_LEN);
			switch (buf[0]) {
				case 'F':
					// "Finish judging, please report the result to the judger"
					summarize();
					break;
				default:
					log("Error! Received something unknown from the judger: %c (ASCII=%d)\n", buf[0], int(buf[0]));
					log(this_is_a_bug_str);
					exit(1);
			}
		} else {
			// Received something from ... em ... ok maybe it is caused by the solar storm?
			log("Error! At line %d in `game_server`, received something from a unknown location\n", __LINE__);
			log(this_is_a_bug_str);
			exit(1);
		}
	}
}

int main(int argc, char* argv[]) {
	prog_name = "Game Server";
	exit_when_parent_dies();

	read_env_vars();

	read_map();

	// Alloc space for `is_open`
	is_open = (char*)Calloc(N*N/8, 1);
	// Alloc space for `vis`
	for (int i = 0; i < NUM_ACTIVE_WORKER_THREAD; ++i) {
		vis[i] = (char*)Calloc(N*N/8, 1);
	}
	// Initialize vis_occupied_flags
	for (int i = 0; i < NUM_ACTIVE_WORKER_THREAD; ++i) {
		vis_occupied_flags[i].clear();
	}

	shm_start = open_shm();
	
	// Send N and K to the players program, via `fd_to_pl`
	char buf[64];
	sprintf(buf, "%ld %ld", N, K);
	Write(fd_to_pl, buf, strlen(buf));

	main_thread_routine();

	// The control flow should not reach here
	log("Error! The control flow reaches to the end of `main` in the game server.\n");
	log(this_is_a_bug_str);
	return 1;
}