/*
	naive_optim.cpp - A naive solution
	This solution just clicks all grids one by one, with a simple optimization,
	which terminates the program (do not do further clicking) when we detect that
	we have opened all non-mine grids
*/

#include <cstdio>

// 请在程序开头引用此头文件
#include "minesweeper_helpers.h"

constexpr int MAXN = 65536;

// N: 地图的边长
// K: 地图中的雷的数量
long N, K;
int constant_A;

int main() {
	// 请在程序开始时调用此函数来初始化 & 获得 N 和 K 的值
	minesweeper_init(N, K, constant_A);

	// 使用这个函数来创建一个 Channel
	Channel channel = create_channel();

	// 依次点击 N*N 地图中的每个格子。
	// 顺便记录一下我们点开的非雷格子数量。如果这个数量达到了 N*N-K，那么我们提前退出。
	long open_non_mine = 0;
	for (long r = 0; r < N; ++r) {
		for (long c = 0; c < N; ++c) {
			// 点开 (r, c) 这个位置的格子，并且不要执行间接点开
			ClickResult result = channel.click_do_not_expand(r, c);
			if (result.is_mine) {
				// 踩到地雷了
			} else {
				// 没踩到地雷
				open_non_mine += 1;
				if (open_non_mine == N*N-K) {
					// 所有的非雷格子都被点开了，傻逼才继续点
					goto loop_end;
				}
			}
		}
	}

	loop_end:;

	return 0;
}