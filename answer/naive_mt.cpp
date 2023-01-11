/*
	naive_mt.cpp - A naive solution, with multithread optimization
	This solution just clicks all grids one by one, using multiple threads.

		It summons `THREAD_COUNT` threads. The i-th thread is responsible for 
	clicking rows within the range [i*N/THREAD_COUNT, (i+1)*N/THREAD_COUNT)
*/

#include <pthread.h>
#include <cstdlib>
#include <cstdio>
#include <string.h>
#include "minesweeper_helpers.h"

constexpr int THREAD_COUNT = 8;

void Pthread_create_wrapper(pthread_t *tidp, pthread_attr_t *attrp, 
		    void * (*routine)(void *), void *argp) 
{
    int rc;

    if ((rc = pthread_create(tidp, attrp, routine, argp)) != 0)
		fprintf(stderr, "Pthread_create error");
}

void Pthread_join_wrapper(pthread_t tid, void **thread_return) {
    int rc;

    if ((rc = pthread_join(tid, thread_return)) != 0)
		fprintf(stderr, "Pthread_join error");
}



long N, K;
int constant_A;

void* thread_routine(void* arg) {
	long thread_id = (long)arg;
	Channel channel = create_channel();
	long row_start = thread_id*N/THREAD_COUNT;
	long row_end = (thread_id+1)*N/THREAD_COUNT;
	for (long r = row_start; r < row_end; ++r) {
		// printf("R = %ld\n", r);
		for (long c = 0; c < N; ++c) {
			channel.click(r, c, true);
		}
	}
	return 0;
}

int main() {
	minesweeper_init(N, K, constant_A);
		
	// Summon those worker threads
	pthread_t tids[THREAD_COUNT];
	for (int i = 0; i < THREAD_COUNT; ++i) {
		Pthread_create_wrapper(tids+i, NULL, thread_routine, (void*)(long)i);
	}
	// Wait for worker threads to finish
	for (int i = 0; i < THREAD_COUNT; ++i) {
		Pthread_join_wrapper(tids[i], NULL);
	}

	return 0;
}