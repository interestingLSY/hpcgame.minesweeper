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

// The SIGCHLD signal handler
// Invoked when the player's program / the game server exits.
void sigchld_handler(int _) {

}

// The SIGPIPE signal handler
// Invoked when we try to write something to a closed pipe.
// In ideal situations, we receive SIGCHLD before any SIGPIPE, and in our logic,
// when we receive SIGCHLD, the whole judging prodecure is going to finish,
// which means that we won't write anything to any pipes.
// So receiving SIGPIPE means there is some bug inside the judger.
void sigpipe_handler(int _) {
	log("Warning: Broken pipe.\n");
	log("This means that there is some bugs in the judger.\n");
	log("Please contact interestingLSY (interestingLSY@gmail.com) and report the bug.\n");
	log("Thank you!\n");
	exit(1);
}

// The SIGALRM signal handler
// Invoked when time is up.
void sigalrm_handler(int _) {

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
	}
}

void create_player() {
	if ((player_pid = Fork()) == 0) {
		// I am the child
		// Close unnecessary file descriptors
		Close(fd_gs_from_pl);
		Close(fd_gs_to_pl);
		close(fd_gs_from_ju);
		Close(fd_gs_to_ju);
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
	}
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

	// Create those pipes
	// 命名规则：fd_A_to_B 代表这个 fd 归 A 所有，这个 fd 所对应的 PIPE 的另一端归程序 B 所有
	// 连接方式详见本程序 (judger.cpp) 开头的注释
	Pipe(fd_gs_from_pl, fd_pl_to_gs);
	Pipe(fd_pl_from_gs, fd_gs_to_pl);
	Pipe(fd_ju_from_gs, fd_gs_to_ju);
	Pipe(fd_gs_from_ju, fd_ju_to_gs);

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


	return 0;
}