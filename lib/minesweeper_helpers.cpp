#include <cassert>
#include "wrappers.h"
#include "common.h"
#include "shm.h"
#include "log.h"
#include "futex.h"
#include "minesweeper_helpers.h"

static char* shm_start;
static int fd_from_gs, fd_to_gs;
static long _N, _K;

void minesweeper_init(long &N, long &K) {
	prog_name = "Player's Program";
	if (!Getenv("MINESWEEPER_LAUNCHED_BY_JUDGER")) {
		log("Error! This program (player's program) is not designed for starting\
manually. Please invoke the judger.\n");
		exit(1);
	}
	// Set the process to exit when its parent dies
	exit_when_parent_dies();
	// Open the shared memory (shm) region
	shm_start = open_shm();
	// Get fds
	fd_from_gs = atoi(Getenv_must_exist("MINESWEEPER_FD_PL_FROM_GS"));
	fd_to_gs = atoi(Getenv_must_exist("MINESWEEPER_FD_PL_TO_GS"));
	// Read N and K from `fd_from_gs`
	char buf[64];
	Read(fd_from_gs, buf, 64);
	int rc = sscanf(buf, "%ld %ld", &N, &K);
	if (rc != 2) {
		log("Error! Did not read enough numbers (N and K) from `fd_from_gs` in `minesweeper_init()`.\n");
		log("The game server sent \"%s\"\n", buf);
		exit(1);
	}
	_N = N; _K = K;
}

pthread_mutex_t create_channel_mutex = PTHREAD_MUTEX_INITIALIZER;
Channel create_channel(void) {
	Pthread_mutex_lock(&create_channel_mutex);
	Channel result;
	// Send 'C' to the game server, through `fd_to_gs`
	char c = 'C';
	Write(fd_to_gs, &c, 1);
	// Get the channel ID
	char buf[16];
	Read(fd_from_gs, buf, 16);
	sscanf(buf, "%d", &result.id);
	// Calculate `shm_pos`
	result.shm_pos = shm_start + result.id*CHANNEL_SHM_SIZE;
	Pthread_mutex_unlock(&create_channel_mutex);
	return result;
}

ClickResult Channel::click(long r, long c) {
	if (r < 0 || c < 0 || r >= _N || c >= _N) {
		log("Error! The player's program called `click(r, c)` with invalid arguments:\n");
		log("R = %ld, C = %ld\n", r, c);
		exit(1);
	}
	char* shm_pos = this->shm_pos;
	ClickResult result;
	// Fill in `click_r` and `click_c`
	SHM_CLICK_R(shm_pos) = (unsigned short)r;
	SHM_CLICK_C(shm_pos) = (unsigned short)c;
	
	// Wake up the corresponding thread in the game server
	SHM_PENDING_BIT(shm_pos) = 1;
 	if (SHM_SLEEPING_BIT(shm_pos)) {
		futex_wake(SHM_PENDING_BIT_PTR(shm_pos));
	}
	// Wait for the game server to complete the request (by spinning)
	while (!SHM_DONE_BIT(shm_pos)) {
		if (SHM_SLEEPING_BIT(shm_pos)) {
			int t = futex_wake(SHM_PENDING_BIT_PTR(shm_pos));
		}
	}
	// Copy the result
	int open_grid_count = SHM_OPENED_GRID_COUNT(shm_pos);
	if (open_grid_count == -1) {
		// The grid contains a mine, BOOM!
		result.is_mine = true;
	} else {
		result.is_mine = false;
		result.open_grid_count = open_grid_count;
		result.open_grid_pos = SHM_OPENED_GRID_ARR(shm_pos);
	}
	SHM_DONE_BIT(shm_pos) = 0;
	return result;
}