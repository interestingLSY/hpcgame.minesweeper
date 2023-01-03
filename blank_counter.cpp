/*
*/

#include <cstdio>
#include <random>
#include <chrono>
#include <cassert>
#include <queue>
#include "lib/wrappers.h"
using std::max, std::queue;

void usage(char* prog_name) {
	printf("Usage: %s <path/to/map_file>\n", prog_name);
	exit(0);
}

long N, K, logN;
char* is_mine;

char test_is_mine(long r, long c) {
	if (r < 0 || c < 0 || r >= N || c >= N) return 1;
	long index = (r<<logN) + c;
	long number = index/8, offset = index%8;
	return is_mine[number]>>offset&0x1;
}

char* vis;
char test_is_vis(long r, long c) {
	if (r < 0 || c < 0 || r >= N || c >= N) return 0;
	long index = (r<<logN) + c;
	long number = index/8, offset = index%8;
	return vis[number]>>offset&0x1;
}

void mark_vis(long r, long c) {
	long index = (r<<logN) + c;
	long number = index/8, offset = index%8;
	vis[number] |= 1<<offset;
}

char count_adj_mine(long r, long c) {
	char cnt = test_is_mine(r-1, c-1) + test_is_mine(r-1, c) + test_is_mine(r-1, c+1)
			+ test_is_mine(r, c-1) + test_is_mine(r, c) + test_is_mine(r, c+1)
			+ test_is_mine(r+1, c-1) + test_is_mine(r+1, c) + test_is_mine(r+1, c+1);
	return cnt;
}

constexpr int delta_xy[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};

queue<long> q;
long bfs(long st_r, long st_c) {
	long count = 0;
	q.push(st_r<<logN | st_c);
	while (!q.empty()) {
		long q_front = q.front();
		long r = q_front>>logN, c = q_front&(N-1);
		q.pop();
		count += 1;
		for (int k = 0; k < 8; ++k) {
			int new_r = r + delta_xy[k][0], new_c = c + delta_xy[k][1];
			if (!test_is_mine(new_r, new_c) && !test_is_vis(new_r, new_c)) {
				if (count_adj_mine(new_r, new_c) == 0) {
					mark_vis(new_r, new_c);
					q.push(new_r<<logN | new_c);
				} else {
					mark_vis(new_r, new_c);
					count += 1;
				}
			}
		}
	}
	return count;
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

	is_mine = (char*)Malloc(N*N/8);
	size_t _ __attribute__((unused)) = fread(is_mine, 1, N*N/8, map_file);
	vis = (char*)Calloc(N*N/8, 1);
	
	long max_blank = 0;
	for (long r = 0; r < N; ++r)
		for (long c = 0; c < N; ++c)
			if (!test_is_mine(r, c) && !test_is_vis(r, c) && 
				count_adj_mine(r, c) == 0) {
				long t = bfs(r, c);
				max_blank = max(max_blank, t);
			}
	printf("max blank: %ld\n", max_blank);

	return 0;
}