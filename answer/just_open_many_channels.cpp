/*
	just_open_many_channels.cpp - Just open many channels, and perform `click(0, 0, 0)`
	on each channel. For stability testing.
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

	for (int i = 0; i < 1024; ++i) {
		Channel channel = create_channel();
		channel.click(0, 0, 0);
	}

	return 0;
}