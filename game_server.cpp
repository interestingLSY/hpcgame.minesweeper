/*
	game_server - The minesweeper game server

		This program is designed to be launched by the judger (judger.cpp). Please do
	not launch it directly.
*/
#include "lib/wrappers.h"
#include "lib/log.h"

long N, K, logN;	// The size of the map, the number of mines
char* map_file_path;	// path to the map file
int fd_to_pl, fd_from_pl;	// fds (used to communicate with player's program)
int fd_to_ju, fd_from_ju;	// fds (used to communicate with the judger)

char* is_mine;	// the map
char test_is_mine(long r, long c) {
	if (r < 0 || c < 0 || r >= N || c >= N) return 0;
	long index = (r<<logN) + c;
	long number = index/8, offset = index%8;
	return is_mine[number]>>offset&0x1;
}

// read and parse necessary env variables
void read_env_vars() {
	if (!Getenv("MINESWEEPER_LAUNCHED_BY_JUDGER")) {
		app_error("This program (game server) is designed to be launched by \
the judger. Please do not launch it directly");
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

int main(int argc, char* argv[]) {
	prog_name = "Game Server";

	read_env_vars();

	read_map();
}