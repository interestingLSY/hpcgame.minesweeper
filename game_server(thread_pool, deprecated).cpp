/*
	game_server - The minesweeper game server

		Notice: This program is designed to be launched by the judger (judger.cpp). Please do
	not launch it directly.

		It accepts the following envariables: MINESWEEPER_LAUNCHED_BY_JUDGER (must present),
	MINESWEEPER_MAP_FILE_PATH, MINESWEEPER_FD_GS_TO_PL, MINESWEEPER_FD_GS_FROM_PL,
	MINESWEEPER_FD_GS_TO_JU, MINESWEEPER_FD_GS_FROM_JU.
		It first parses those envariables and reads the map from the file
	indicated by MINESWEEPER_MAP_FILE_PATH. Then it begins to interact with
	the player's program.
		When the player's program exits or the time is up, the judger sents an 'F' (stands for "Finished")
	character to the game server (received through `fd_from_ju`), and then the
	game server will send the number of opened non-mine grids and opened is-mine
	grids to the judger (sent through `fd_to_ju`)

	Overall Design:
		TL; DR: The game server uses a producer-comsumer model.
		A thread (named "co-thread") monitors the entire shared memory region,
	and when there is a pending request, it puts the ID of that channel into
	a queue (named pending_queue). Threads in the thread pool (consisting
	`NUM_THREADPOOL_THREAD` threads) is responsible for taking those IDs from
	the queue and working on them.

	Detailed Design:
		The main thread is responsible for accepting incoming "new-channel"
	requests as well as receiving commands from the judger (by I/O multiplexing).
		Each `Channel` has exactly one shared memory region. Those regions are
	carefully laid out to gain a better cache performace. When a channel is
	activated (the player's program calls `click()`), the player's program
	marks the "pending bit" in the corresponding shared memory region to 1.
		Besides the main thread, there is a "co-thread". It is responsible for
	monitoring the entire shared memory region, and when it finds out that the
	"pending bit" of a particular region is 1, it sets that bit to 0 and puts the
	ID of the channel into `pending_queue`.
		A thread pool is maintained, consisting `NUM_THREADPOOL_THREAD` threads.
	Each thread in the thread pool monitors the `pending_queue`, and when it is
	non-empty, the thread takes the channel ID from the front of the queue and
	works on that request.

	// [Outdated]
	// 	When it accepts a new-channel request, a new thread (namely "worker")
	// is created to monitor the shared memory region. We guarantee that the index of the
	// shared memory equals to the channel ID.
	// 	Each work thread is responsible for monitoring exactly one channel. In
	// detail, it monitors a chunk of shared memory by spinning (`while` loop),
	// and when a `click` request arrives

*/

#include <utility>
#include "lib/wrappers.h"
#include "lib/log.h"
using std::pair;

constexpr int NUM_THREADPOOL_THREAD = 8; // The number of threads in the thread pool
constexpr int NUM_SUMMARIZE_THREAD = 8;	// The number of thread for `summarize()`

long N, K, logN;	// The size of the map, the number of mines
char* map_file_path;	// path to the map file
int fd_to_pl, fd_from_pl;	// fds (used to communicate with player's program)
int fd_to_ju, fd_from_ju;	// fds (used to communicate with the judger)

char* is_mine;	// A large bit array, representing the map.
char test_is_mine(long r, long c) {
	if (r < 0 || c < 0 || r >= N || c >= N) return 0;
	long index = (r<<logN) + c;
	long number = index/8, offset = index%8;
	return is_mine[number]>>offset&0x1;
}

char* is_open;	// A large bit array, representing whether the grid is opened by the player
char test_is_open(long r, long c) {
	if (r < 0 || c < 0 || r >= N || c >= N) return 0;
	long index = (r<<logN) + c;
	long number = index/8, offset = index%8;
	return is_open[number]>>offset&0x1;
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

	logN = (long)(log2((double)N)+0.01);

	is_mine = (char*)Malloc(N*N/8);
	size_t byte_read = fread(is_mine, 1, N*N/8, map_file);
	if (byte_read != N*N/8) {
		app_error("Failed to read the map. Maybe the map file is broken?");
	}

	Fclose(map_file);
	log("Map info: N = %ld, K = %ld\n", N, K);
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
 * Functions for maintaining the `pending_queue`
 * The queue is a message queue. There is only one producer (the co-thread)
 * and multiple consumers (threads in the thread pool)
 * The goal is to minimize the overhead brought by synchronization.
 */


/*
 * Functions and variables for threads in the thread pool
 */
char* vis[NUM_THREADPOOL_THREAD];	// vis array for BFS


// thread_pool_thread_routine - Thread routine for threads in the thread pool
void* thread_pool_thread_routine(void* arg) {
	long thread_id = (long)arg;

	return NULL;
}

int main(int argc, char* argv[]) {
	prog_name = "Game Server";

	read_env_vars();

	read_map();

	// Alloc space for `is_open`
	is_open = (char*)Calloc(N*N/8, 1);

	summarize();
}