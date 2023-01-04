/*
	naive.cpp - A naive solution
	This solution just clicks all grids one by one
*/

#include <cstdio>

// 请在程序开头引用此头文件
#include "minesweeper_helpers.h"

constexpr int MAXN = 65536;

// N: 地图的边长
// K: 地图中的雷的数量
long N, K;

int main() {
	// 请在程序开始时调用此函数来初始化 & 获得 N 和 K 的值
	minesweeper_init(N, K);

	// 使用这个函数来创建一个 Channel
	Channel channel = create_channel();

	// 依次点击 N*N 地图中的每个格子。
	for (long r = 0; r < N; ++r) {
		for (long c = 0; c < N; ++c) {
			ClickResult result = channel.click(r, c, true);
			if (result.is_skipped) {
				// 我们之前（直接地或间接地）点开过这个方格
			} else if (result.is_mine) {
				// 踩到地雷了
			} else {
				// 没踩到地雷
			}
		}
	}

	return 0;
}