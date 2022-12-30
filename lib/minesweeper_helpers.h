#ifndef __MINESWEEPER_HELPERS_H__
#define __MINESWEEPER_HELPERS_H__

#include "wrappers.h"

// ClickResult - 某一次点击的结果
struct ClickResult {
	// 点击的方格是不是地雷
	bool is_mine;

	// 如果点击的方格不是地雷，那么这个变量代表有多少个方格被点开
	// 别忘了，如果你点的方格中的数字是零，那么它周围的方格也会被点开
	// 并且这个过程可以递归
	int reveal_count;

	// 一个长度为 reveal_count 的数组，第 i 个元素代表第 i 个被点开的格子所在的行
	int* reveal_grid_r;

	// 一个长度为 reveal_count 的数组，第 i 个元素代表第 i 个被点开的格子所在的列
	int* reveal_grid_c;
};

// Channel - 选手程序和 game server 间相互通信的信道
class Channel {
private:
public:
	ClickResult click(int r, int c);
};

// 创建一个新的信道
Channel create_channel(void);

#endif	// __MINESWEEPER_HELPERS_H__