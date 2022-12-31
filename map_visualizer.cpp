/*
	map_visualizer - Visualize a map for the game minesweeper
	It reads the map from a file, and prints the map (in a human-readable format)
	onto the screen.

	Usage: ./map_visualizer <map_file>
*/

#include <cstdio>
#include <random>
#include <chrono>
#include <cassert>
#include "lib/wrappers.h"

#define DISPLAY_THRESHOLD_N 2048

void usage(char* prog_name) {
	printf("Usage: %s <path/to/map_file>\n", prog_name);
	exit(0);
}

long N, K, logN;
char* is_mine;

char test_is_mine(long r, long c) {
	if (r < 0 || c < 0 || r >= N || c >= N) return 0;
	long index = (r<<logN) + c;
	long number = index/8, offset = index%8;
	return is_mine[number]>>offset&0x1;
}

int main(int argc, char* argv[]) {
	if (argc != 2) {
		usage(argv[0]);
	}

	FILE* map_file = fopen(argv[1], "r");
	if (!map_file) {
		unix_error("Failed to open the map file");
	}
	printf("File: %s\n", argv[1]);

	if (fscanf(map_file, "%ld %ld\n", &N, &K) != 2) {
		app_error("Failed to read N and K. Maybe the map file is broken?");
	}

	printf("Size(N): %ld\nNumber of mines(K): %ld\n", N, K);
	logN = (long)(log2((double)N)+0.01);

	if (N > DISPLAY_THRESHOLD_N) {
		app_error("This map is too large.\n\
Displaying it may stuck or crash your terminal (especially when you are using a code server).\n\
If you really want to display it, please change DISPLAY_THRESHOLD_N in \
map_visualizer to a larger number, and recompile it.\n\
P.S. the default value of DISPLAY_THRESHOLD_N is 2048.");
	}

	is_mine = (char*)Malloc(N*N/8);
	size_t byte_read = fread(is_mine, 1, N*N/8, map_file);
	if (byte_read != N*N/8) {
		app_error("Failed to read the map. Maybe the map file is broken?");
	}

	long mine_cnt = 0;
	for (long r = 0; r < N; ++r) {
		for (long c = 0; c < N; ++c) {
			if (test_is_mine(r, c)) {
				putchar('*');
				mine_cnt += 1;
			} else {
				long cnt = test_is_mine(r-1, c-1) + test_is_mine(r-1, c) + test_is_mine(r-1, c+1)
						+ test_is_mine(r, c-1) + test_is_mine(r, c) + test_is_mine(r, c+1)
						+ test_is_mine(r+1, c-1) + test_is_mine(r+1, c) + test_is_mine(r+1, c+1);
				putchar(cnt+'0');
			}
		}
		putchar('\n');
	}
	assert(mine_cnt == K);

	return 0;
}