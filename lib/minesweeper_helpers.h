#ifndef __MINESWEEPER_HELPERS_H__
#define __MINESWEEPER_HELPERS_H__

#ifndef MINESWEEPER_BIND_CORE_MODE
#define MINESWEEPER_BIND_CORE_MODE 1	// Default: Bind thread to core
#endif

// ClickResult - 某一次点击的结果
struct ClickResult {
	// 点击的方格是不是地雷
	bool is_mine;

	// 如果点击的方格不是地雷，那么这个变量代表有多少个方格被点开
	// 别忘了，如果你点的方格中的数字是零，那么它周围的方格也会被点开
	// 并且这个过程可以递归
	int open_grid_count;

	// 一个长度为 reveal_count 的数组
	// reveal_grid_r[i][0] 代表第 i 个被点开的格子所在的行
	// reveal_grid_r[i][1] 代表第 i 个被点开的格子所在的列
	unsigned short (*open_grid_pos)[16384][2];
};

// Channel - 选手程序和 game server 间相互通信的信道
class Channel {
private:
	// Channel ID
	int id;

	char* shm_pos;
public:
	ClickResult click(long r, long c);
	friend Channel create_channel(void);
};

// 整个程序的初始化。
// This should be called once and only once in the player's program
void minesweeper_init(long &N, long &K);

// 创建一个新的信道
Channel create_channel(void);

#endif	// __MINESWEEPER_HELPERS_H__