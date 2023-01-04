/*
	judger - The judger.

		It needs: 1) Your(player's) program (in executable file), 2) The game
	server (in executable file), 3) The map file for mine-sweeping.
		Then it sets up the connection between player's program and the
	game server and then starts the game server. When time is up, it notifies
	the game server, which sends the result to the judger. The judger then calculate
	the player's score.

	Usage: ./judger <path/to/player's/program> <path/to/map> [time_limit (In seconds, default: +inf)] [path/to/game/server (Default: ./game_server)]

	Pipes:
		When judging, the three programs (player, game server,judger) are connected by pipes as follow:

		Player (pl)					Game Server (gs)	Judger (ju)
		stdin						stdin				stdin
		stdout						stdout				stdout
		stderr						stderr				stderr
		fd_to_gs  --------------->	fd_from_pl			
		fd_from_gs  <-------------	fd_to_pl
									fd_to_ju  ------->  fd_from_gs
									fd_from_ju  <-----  fd_to_gs

*/

#include <cstdio>
#include <cassert>
#include <climits>
#include <filesystem>
#include <unistd.h>
#include <fcntl.h>
#include "lib/wrappers.h"
#include "lib/log.h"
#include "lib/common.h"
#include "lib/shm.h"

void usage(char* prog_name) {
	printf("Usage: %s <path/to/player's/program> <path/to/map> [time_limit (In seconds, default: +inf)] [path/to/game/server (Default: ./game_server)]\n", prog_name);
	exit(0);
}

// Helper functions for blocking all signals / restore to default signal
sigset_t sigset_prev;
void block_all_signals() {
	sigset_t sigset_block_all;
	Sigfillset(&sigset_block_all);
	Sigprocmask(SIG_BLOCK, &sigset_block_all, &sigset_prev);
}
void restore_prev_blockset() {
	Sigprocmask(SIG_SETMASK, &sigset_prev, NULL);
}

// create_pipe - Create a pipe
// It guarantee that the result fds >= 100
void create_pipe(int &read_fd, int &write_fd) {
	static int current_fd = 102;
	// generate a pipe
	int t_read_fd, t_write_fd;
	Pipe(t_read_fd, t_write_fd);
	// create read fd
	read_fd = current_fd;
	Dup2(t_read_fd, read_fd);
	++current_fd;
	// create write fd
	write_fd = current_fd;
	Dup2(t_write_fd, write_fd);
	++current_fd;
	// close old fds
	Close(t_read_fd);
	Close(t_write_fd);
}

char* player_path;	// path to the player's program (executable file)
char* map_file_path;	// path to the map
char* game_server_path;	// path to the game server (executable file)
int time_limit;			// time limit, in seconds

int fd_pl_to_gs, fd_gs_from_pl;
int fd_pl_from_gs, fd_gs_to_pl;
int fd_gs_to_ju, fd_ju_from_gs;
int fd_gs_from_ju, fd_ju_to_gs;

pid_t game_server_pid, player_pid;

void make_sure_file_exists(const char* path, const char* file_description) {
	if (!std::filesystem::exists(path)) {
		app_error("Error: file %s (%s) does not exists.\n", path, file_description);
		exit(1);
	}
}

void make_sure_file_is_executable(const char* path, const char* file_description) {
	if (access(path, X_OK) != 0) {
		app_error("Error: file %s (%s) is not executable.\n", path, file_description);
		exit(1);
	}
}

void read_result_from_game_server_and_report();

// The SIGCHLD signal handler
// Invoked when the player's program / the game server exits.
void sigchld_handler(int _) {
	int old_errno = errno;
	block_all_signals();

	int status;
	pid_t pid;
	while ((pid=Waitpid(-1, &status, WNOHANG)) != 0) {
		if (pid == player_pid) {
			// The player's program exits
			int exit_status = WEXITSTATUS(status);
			if (exit_status != 0) {
				sio_log("Warning: The player's program exits with a non-zero exit code: ");
				sio_put(exit_status); sio_put("\n");
			}
			if (WIFSIGNALED(status)) {
				sio_log("Warning: The player's program is killed by a signal.\n");
				sio_log("The signal causing the player's program to terminate: ");
				sio_put(WTERMSIG(status));
				sio_put("\n");
			}
			read_result_from_game_server_and_report();
		} else if (pid == game_server_pid) {
			// The game server exits
			// This should never happend
			int exit_status = WEXITSTATUS(status);
			sio_log("Error: The game server exits before the judger\n");
			sio_log("In my design, this should never happen.\n");
			sio_log("This is most probably because that the game server crashes for some reason.\n");
			sio_log("Exit status of the game server: ");
			sio_put(exit_status); sio_put("\n");
			if (WIFSIGNALED(status)) {
				sio_log("The signal causing the game server to terminate: ");
				sio_put(WTERMSIG(status));
				sio_put("\n");
			}
			sio_log(this_is_a_bug_str);
			exit(1);
		} else {
			// The ... em ... well, I don't know what to say
			sio_log("Error! In `sigchld_handler` in `judger.cpp`, some child with an unexpected PID exits.\n");
			sio_log("\tInfo: game_server_pid: "); sio_put(game_server_pid);
			sio_put(" player_pid: "); sio_put(player_pid);
			sio_put(" pid of the exited child: "); sio_put(pid);
			sio_put("\n");
			sio_log(this_is_a_bug_str);
			exit(1);
		}
	}

	restore_prev_blockset();
	errno = old_errno;
}

// The SIGPIPE signal handler
// Invoked when we try to write something to a closed pipe.
// In ideal situations, we receive SIGCHLD before any SIGPIPE, and in our logic,
// when we receive SIGCHLD, the whole judging prodecure is going to finish,
// which means that we won't write anything to any pipes.
// So receiving SIGPIPE means there is some bug inside the judger.
void sigpipe_handler(int _) {
	block_all_signals();
	sio_log("Warning: Broken pipe.\n");
	sio_log("This means that there are some bugs in the judger.\n");
	sio_log(this_is_a_bug_str);
	exit(1);
}

// The SIGALRM signal handler
// Invoked when time is up.
void sigalrm_handler(int _) {
	block_all_signals();
	sio_log("Time is up. Killing player's program and reading result from the game server.\n");
	kill(player_pid, SIGKILL);
	read_result_from_game_server_and_report();
}

// Create a shared memory region, consisting MAX_CHANNEL*CHANNEL_MEMORY_REGION_SIZE = SHM_SIZE bytes
void create_shared_memory_region() {
	// Create the fd pointing to the shared memory region
	int mem_fd = Shm_open(SHM_NAME, O_CREAT | O_TRUNC | O_RDWR, S_IRWXU);
	// Truncate the memory region
	Ftruncate(mem_fd, TOTAL_SHM_SIZE);
}

void create_game_server() {
	if ((game_server_pid = Fork()) == 0) {
		// I am the child
		// Close unnecessary file descriptors
		Close(fd_pl_from_gs);
		Close(fd_pl_to_gs);
		Close(fd_ju_from_gs);
		Close(fd_ju_to_gs);

		// Set env variables
		char buf[16];
		sprintf(buf, "%d", fd_gs_to_pl);
		Setenv("MINESWEEPER_FD_GS_TO_PL", buf, true);
		sprintf(buf, "%d", fd_gs_from_pl);
		Setenv("MINESWEEPER_FD_GS_FROM_PL", buf, true);
		sprintf(buf, "%d", fd_gs_to_ju);
		Setenv("MINESWEEPER_FD_GS_TO_JU", buf, true);
		sprintf(buf, "%d", fd_gs_from_ju);
		Setenv("MINESWEEPER_FD_GS_FROM_JU", buf, true);
		Setenv("MINESWEEPER_MAP_FILE_PATH", map_file_path, true);
		Setenv("MINESWEEPER_LAUNCHED_BY_JUDGER", "1", true);

		// Reset signal handlers
		Signal(SIGCHLD, SIG_DFL);
		Signal(SIGPIPE, SIG_DFL);
		Signal(SIGALRM, SIG_DFL);

		log("Starting game server...\n");

		// Exec
		Execl(game_server_path, game_server_path, NULL);
	} else {
		// I am the parent (the judger)
		Close(fd_gs_from_ju);
		Close(fd_gs_from_pl);
		Close(fd_gs_to_ju);
		Close(fd_gs_to_pl);
	}
}

void create_player() {
	if ((player_pid = Fork()) == 0) {
		// I am the child
		// Close unnecessary file descriptors
		Close(fd_ju_from_gs);
		Close(fd_ju_to_gs);

		// Set env variables
		char buf[16];
		sprintf(buf, "%d", fd_pl_to_gs);
		Setenv("MINESWEEPER_FD_PL_TO_GS", buf, true);
		sprintf(buf, "%d", fd_pl_from_gs);
		Setenv("MINESWEEPER_FD_PL_FROM_GS", buf, true);
		Setenv("MINESWEEPER_LAUNCHED_BY_JUDGER", "1", true);

		// Reset signal handlers
		Signal(SIGCHLD, SIG_DFL);
		Signal(SIGPIPE, SIG_DFL);
		Signal(SIGALRM, SIG_DFL);

		log("Starting player's program...\n");

		// Exec
		Execl(player_path, player_path, NULL);
	} else {
		// I am the parent (the judger)
		Close(fd_pl_from_gs);
		Close(fd_pl_to_gs);
	}
}

// read_result_from_game_server - Send character 'F' to the game server and
// read the result from game server (via fd_ju_from_gs), print it out, report it
// to the grader and exit
void read_result_from_game_server_and_report() {
	// Send "F" to the game server
	char c = 'F';
	Write(fd_ju_to_gs, &c, 1);
	// Read the response
	char buf[128];
	Read(fd_ju_from_gs, buf, 128);
	// Parse the response
	int status;
	long N, K;
	long cnt_non_mine, cnt_is_mine;
	assert(sscanf(buf, "%d %ld %ld %ld %ld",
		&status, &N, &K, &cnt_non_mine, &cnt_is_mine) == 5);
	// Print it out
	log("Result:\n");
	log("点开的非雷格子: %ld/%ld (%.4f%%)\n",
		cnt_non_mine, N*N-K, (double)cnt_non_mine/(N*N-K)*100);
	log("点开的雷: %ld/%ld (%.4f%%)\n",
		cnt_is_mine, K, (double)cnt_is_mine/K*100);
	// Exit
	exit(0);
}

int main(int argc, char* argv[]) {
	prog_name = "Judger";
	if (argc != 3 && argc != 4 && argc != 5) {
		usage(argv[0]);
	}

	// Parse the input
	player_path = argv[1];
	map_file_path = argv[2];
	if (argc >= 4) {
		time_limit = atoi(argv[3]);
		if (time_limit <= 0) app_error("Bad value for `time_limit`");
	} else {
		time_limit = INT_MAX;
	}
	if (argc >= 5) {
		game_server_path = argv[4];
	} else {
		game_server_path = (char*)"./game_server";
	}

	// Make sure all the files exist, and is executable
	make_sure_file_exists(player_path, "player's program");
	make_sure_file_exists(map_file_path, "the map");
	make_sure_file_exists(game_server_path, "the game server");
	make_sure_file_is_executable(player_path, "player's program");
	make_sure_file_is_executable(game_server_path, "the game server");

	// Create pipes
	// 命名规则：fd_A_to_B 代表这个 fd 归 A 所有，这个 fd 所对应的 PIPE 的另一端归程序 B 所有
	// 连接方式详见本程序 (judger.cpp) 开头的注释
	// We use `create_pipe` instead of `Pipe` to make sure that
	// the result fd is greater or equal to 100, since that
	// the player's program may want to use fds below 100. 
	create_pipe(fd_gs_from_pl, fd_pl_to_gs);
	create_pipe(fd_pl_from_gs, fd_gs_to_pl);
	create_pipe(fd_gs_from_ju, fd_ju_to_gs);
	create_pipe(fd_ju_from_gs, fd_gs_to_ju);

	// Create the shared memory region (used for communication between
	// the user's program and the game server)
	create_shared_memory_region();

	// Set up the signal handlers
	Signal(SIGCHLD, sigchld_handler);
	Signal(SIGPIPE, sigpipe_handler);
	Signal(SIGALRM, sigalrm_handler);

	// Launch the game server and the player's program
	// We block all signals here, in order to prevent SIGCHLD from disturbing us.
	block_all_signals();
	create_game_server();
	create_player();
	Usleep(10000);	// Sleep for a short time, increase stability
	Alarm(time_limit);	// Set up the alarm. When time is up, we should receive a SIGALRM signal
	restore_prev_blockset();

	// // Go to bed to sleep, and use sigsuspend() to wait for any signals
	// sigset_t cur_set;
	// Sigprocmask(SIG_SETMASK, NULL, &cur_set);	// grab the sigset mask
	// while (true) {
	// 	Sigsuspend(&cur_set);
	// }

	// Go to bed to sleep. Wake up when signal arrives, or the game server
	// sends something to the judger (through fd_ju_from_gs)
	fd_set monitor_fd_set;
	while (true) {
		FD_ZERO(&monitor_fd_set);
		FD_SET(fd_ju_from_gs, &monitor_fd_set);
		Select(fd_ju_from_gs+1, &monitor_fd_set, NULL, NULL, NULL);
		// We received something from the game server
		// This happens when the player's program does something bad (e.g. requesting
		// too much channels; sends an invalid `click` response...)
		char buf[1024];
		Read(fd_ju_from_gs, buf, 1024);
		log("The game server sends this to judger: ");
		fprintf(stderr, "\"%s\"\n", buf);
		log("So the judger will count the score and exit immediately.\n");
		read_result_from_game_server_and_report();
	}

	// The control flow should not reach here
	log("Error! The control flow reaches to the end of `main` in the judger.\n");
	log(this_is_a_bug_str);
	return 0;
}