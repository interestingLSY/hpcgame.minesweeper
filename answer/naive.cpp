/*
	naive.cpp - A naive solution
	This solution just clicks all grids one by one
*/

#include "minesweeper_helpers.h"

long N, K;

int main() {
	minesweeper_init(N, K);
	Channel channel = create_channel();
	for (long r = 0; r < N; ++r) {
		for (long c = 0; c < N; ++c) {
			channel.click(r, c, true);
		}
	}
	return 0;
}