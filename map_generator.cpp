/*
	map_generator - generate a map for the game minesweeper

	The map looks like:
	```plain
		4 7
		<bit stream (stored in bytes), indicating the map>
	```
	Where 4 is the side length of the map, and 7 is the number of mines.

	Usage: ./map_generator <N> <K>
	It prints the map to the stdout.

	Principle:
		- Stage 1: It first create NUM_THREAD threads, and each thread puts mines
		in the map randomly (without the guarantee that one thread put mines in
		all-unique locations. The i-th thread is responsible for putting mines
		in rows within the range [i*(N/NUM_THREAD), (i+1)*(N/NUM_THREAD).
		- Stage 2: Then it exams how many mines are put, and put the mines left
		with one thread.
*/

#include <cstdio>
#include <random>
#include <chrono>
#include <cassert>
#include "lib/wrappers.h"

void usage(char* prog_name) {
	printf("Usage: %s <N> <K> [seed]\n", prog_name);
	printf("\t`N` is the side length of the map\n");
	printf("\t`K` is the number of mines\n");
	printf("\t`seed` is the random seed for the random number generator. ");
	printf("If it is not present, then the current time is used");
	exit(0);
}

constexpr int NUM_THREAD = 16;

std::mt19937 rng;

long N, K, seed;
int logN;

long* is_mine;	// A big array. It ith-bit indicate whether (i/N, i%N) is a mine or not

inline void put_mine(uint r, uint c) {
	long index = r<<logN | c;
	long number = index/64, offset = index%64;
	is_mine[number] |= 1L<<offset;
}

inline void unput_mine(uint r, uint c) {
	long index = r<<logN | c;
	long number = index/64, offset = index%64;
	is_mine[number] &= ~(1L<<offset);
}

long test_is_mine(uint r, uint c) {
	long index = (r<<logN) + c;
	long number = index/64, offset = index%64;
	return is_mine[number]>>offset&0x1;
}

// The thread routine. Put (at most) `num_mine` mines in the map randomly
void* thread_routine(void* argp) {
	long arg = (long)argp;
	long seed = arg>>32&0xffffffff;
	uint thread_id = arg&0xffffffff;
	std::mt19937 local_rng(seed);	// std::mt19937 is not thread safe. So we must create one for each thread

	uint num_mine = K/NUM_THREAD*1.1;
	uint row_mask = N/NUM_THREAD-1;
	uint col_mask = N-1;
	for (uint i = 0; i < num_mine; ++i) {
		uint t = local_rng(); // Since calling `rng()` is slow, we generate it once and extract r and c from it
		uint r = (t&row_mask) + thread_id*(N/NUM_THREAD);	// Since N is power of 2, doing this won't result in ununiform distribution
		uint c = (t>>16)&col_mask;
		put_mine(r, c);
	}

	return NULL;
}

int main(int argc, char* argv[]) {
	// Initialization
	if (argc != 3 && argc != 4) {
		usage(argv[0]);
	}

	N = atol(argv[1]);
	K = atol(argv[2]);
	if (N <= 0 || K <= 0 || K > N*N) {
		usage(argv[0]);
	}
	logN = (int)(log2((double)N)+0.01);
	if ((1<<logN) != N) {
		app_error("N must be power of 2");
	}
	if (N % NUM_THREAD != 0) {
		app_error("N must be a multiple of NUM_THREAD");
	}
	if (N < 8) {
		app_error("N must be greater or equal to 8");
	}

	if (argc == 4) {
		seed = atol(argv[3]);
	} else {
		seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
	}
	rng = std::mt19937(seed);

	is_mine = (long*)Calloc(N*N/8, 1);

	// Stage 1: Create some threads and begin to put mines in the map
	pthread_t tids[NUM_THREAD];
	for (int i = 0; i < NUM_THREAD; ++i) {
		long seed = rng()+1;
		long arg = seed<<32 | i;
		Pthread_create(tids+i, NULL, thread_routine, (void*)arg);
	}
	for (int i = 0; i < NUM_THREAD; ++i) {
		Pthread_join(tids[i], NULL);
	}


	// Stage 2: Count the number of put mines, and put the lacked mines
	long mine_put = 0;
	for (long i = 0; i < N*N/64; ++i) {
		mine_put += __builtin_popcountll(is_mine[i]);
	}
	// printf("%ld\n", mine_put);
	if (mine_put < K) {
		uint mask = (1<<logN)-1;
		for (long i = 0; i < K-mine_put; ++i) {
			uint r = 0, c = 0;
			do {
				uint t = rng();
				r = t&mask;
				c = (t>>16)&mask;
			} while (test_is_mine(r, c));
			put_mine(r, c);
		}
	} else {
		uint mask = (1<<logN)-1;
		for (long i = 0; i < mine_put-K; ++i) {
			uint r = 0, c = 0;
			do {
				uint t = rng();
				r = t&mask;
				c = (t>>16)&mask;
			} while (!test_is_mine(r, c));
			unput_mine(r, c);
		}
	}

	// mine_put = 0;
	// for (long i = 0; i < N*N/64; ++i) {
	// 	mine_put += __builtin_popcountll(is_mine[i]);
	// }
	// printf("%ld\n", mine_put);


	// Stage 3: Print the map out
	// We use fwrite() to speed up writing
	printf("%ld %ld\n", N, K);
	fwrite((char*)is_mine, 1, N*N/8, stdout);
	fflush(stdout);
	return 0;
}