/*
	expand_with_queue.cpp - A single threaded solution, with amazing accuracy (~0.01%
	mine hit rate) and an acceptable speed.
*/
#include <functional>
#include <utility>
#include <queue>
#include <deque>
#include <random>
#include <cassert>
using std::max, std::min;

#include "csapp.h"
#include "minesweeper_helpers.h"
constexpr int MAXN = 65536;
constexpr int delta_xy[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};
constexpr int delta_xy_2[16][2] = {
	{-2, -2}, {-2, -1}, {-2, 0}, {-2, 1}, {-2, 2},
	{-1, -2}, {-1, 2},
	{0, -2}, {0, 2},
	{1, -2}, {1, 2},
	{2, -2}, {2, -1}, {2, 0}, {2, 1}, {2, 2}
};

std::mt19937 rng(0);

// N: 地图的边长
// K: 地图中的雷的数量
long N, K;

Channel channel;

long cnt_empty_opened = 0;

char* map[MAXN];
constexpr char MAP_UNKNOWN = -1;
constexpr char MAP_MINE = -2;

// char* inferred[MAXN];

inline bool within_range(int r, int c) {
	return r >= 0 && c >= 0 && r < N && c < N;
}

inline bool map_eq(int r, int c, char x) {
	if (r < 0 || c < 0 || r >= N || c >= N) return false;
	return map[r][c] == x;
}
inline bool map_is_empty(int r, int c) {
	if (r < 0 || c < 0 || r >= N || c >= N) return false;
	return map[r][c] >= 0;	
}

int count_adj_unknown(int r, int c) {
	return (int)map_eq(r-1, c-1, MAP_UNKNOWN) + map_eq(r-1, c, MAP_UNKNOWN) + map_eq(r-1, c+1, MAP_UNKNOWN)
		+  map_eq(r, c-1, MAP_UNKNOWN) + map_eq(r, c+1, MAP_UNKNOWN)
		+  map_eq(r+1, c-1, MAP_UNKNOWN) + map_eq(r+1, c, MAP_UNKNOWN) + map_eq(r+1, c+1, MAP_UNKNOWN);
}
int count_adj_mine(int r, int c) {
	return (int)map_eq(r-1, c-1, MAP_MINE) + map_eq(r-1, c, MAP_MINE) + map_eq(r-1, c+1, MAP_MINE)
		+  map_eq(r, c-1, MAP_MINE) + map_eq(r, c+1, MAP_MINE)
		+  map_eq(r+1, c-1, MAP_MINE) + map_eq(r+1, c, MAP_MINE) + map_eq(r+1, c+1, MAP_MINE);
}
int count_adj_empty(int r, int c) {
	return (int)map_is_empty(r-1, c-1) + map_is_empty(r-1, c) + map_is_empty(r-1, c+1)
		+  map_is_empty(r, c-1) + map_is_empty(r, c+1)
		+  map_is_empty(r+1, c-1) + map_is_empty(r+1, c) + map_is_empty(r+1, c+1);
}

// 如果不是雷，返回 0；如果是雷，返回 1
bool click_and_put_result_no_expand(int r, int c) {
	ClickResult result = channel.click_do_not_expand(r, c);
	if (result.is_mine) {
		map[r][c] = MAP_MINE;
	} else {
		if (map[r][c] == MAP_UNKNOWN)
			cnt_empty_opened += 1;
		map[r][c] = (*result.open_grid_pos)[0][2];
	}
	return result.is_mine;
}


struct QueueItem {
	int r, c;
	int re_put_cnt;
};

void expand(int start_r, int start_c) {
	std::deque<QueueItem> border;	// 所有与当前展开的区域相邻、周围有至少 1 个未知区域的非雷格子
	bool first_is_mine = click_and_put_result_no_expand(start_r, start_c);
	if (first_is_mine) {
		return;
	}
	border.push_back({start_r, start_c, 0});

	while (!border.empty()) {
		if (cnt_empty_opened == N*N-K) break;
		auto [cur_r, cur_c, re_put_cnt] = border.front();
		border.pop_front();

		// if (inferred[cur_r][cur_c]) continue;

		int num_in_grid = map[cur_r][cur_c];
		assert (num_in_grid >= 0);
		int adj_unknown = count_adj_unknown(cur_r, cur_c);
		if (!adj_unknown) {
			continue;
		}
		int adj_mine = count_adj_mine(cur_r, cur_c);
		if (adj_mine == num_in_grid) {
			// inferred[cur_r][cur_c] = true;
			// 所有未知的格子都是安全的
			for (int k = 0; k < 8; ++k) {
				int new_r = cur_r + delta_xy[k][0];
				int new_c = cur_c + delta_xy[k][1];
				if (__builtin_expect(!within_range(new_r, new_c), false)) continue;
				if (map[new_r][new_c] != MAP_UNKNOWN) continue;
				bool is_mine = click_and_put_result_no_expand(new_r, new_c);
				assert(!is_mine);
				border.push_front({new_r, new_c, 0});
			}
		} else if (adj_unknown+adj_mine == num_in_grid) {
			// inferred[cur_r][cur_c] = true;
			// 所有未知的格子都是雷
			for (int k = 0; k < 8; ++k) {
				int new_r = cur_r + delta_xy[k][0];
				int new_c = cur_c + delta_xy[k][1];
				if (__builtin_expect(!within_range(new_r, new_c), false)) continue;
				if (map[new_r][new_c] != MAP_UNKNOWN) continue;
				map[new_r][new_c] = MAP_MINE;
			}
			// 拓展一下周围的曼哈顿距离 = 2 的格子
			// 不过我发现，如果不加这段代码的话，程序运行会快很多，同时踩雷的次数不会变多很多
			// 所以就不加了
			// for (int k = 0; k < 16; ++k) {
			// 	int new_r = cur_r + delta_xy_2[k][0];
			// 	int new_c = cur_c + delta_xy_2[k][1];
			// 	if (__builtin_expect(!within_range(new_r, new_c), false)) continue;
			// 	if (map[new_r][new_c] < 0) continue;
			// 	// if (inferred[new_r][new_c]) continue;
			// 	if (!count_adj_unknown(new_r, new_c)) continue;
			// 	border.push_front({new_r, new_c, 0});
			// }
		} else {
			// 暂时推不出来
			if (re_put_cnt < 16)
				border.push_back({cur_r, cur_c, re_put_cnt+1});
		}
	}
	
}

int main() {
	minesweeper_init(N, K);

	for (int i = 0; i < N; ++i) {
		map[i] = (char*)Malloc(N);
		memset(map[i], MAP_UNKNOWN, N);
		// inferred[i] = (char*)Malloc(N);
		// memset(inferred[i], 0, N);

	}
	channel = create_channel();

	// 随机开操
	while (cnt_empty_opened <= (N*N-K)*98/100) {
		int start_r, start_c;
		do {
			start_r = rng()&(N-1);
			start_c = rng()&(N-1);
		} while (map[start_r][start_c] != MAP_UNKNOWN);
		expand(start_r, start_c);
	}

	// 如果不执行下面这段“按顺序开操”，那么会漏掉一些格子没有点开
	// 但是踩雷数量会大幅下降

	// 按顺序开操
	for (int start_r = 0; start_r < N; start_r++) {
		for (int start_c = 0; start_c < N; start_c++) {
			if (map[start_r][start_c] == MAP_UNKNOWN) {
				bool infer_is_non_mine __attribute__((unused)) = false, infer_is_mine = false;
				for (int k = 0; k < 8; ++k) {
					int new_r = start_r + delta_xy[k][0];
					int new_c = start_c + delta_xy[k][1];
					if (__builtin_expect(!within_range(new_r, new_c), false)) continue;
					if (map[new_r][new_c] < 0) continue;
					int adj_unknown = count_adj_unknown(new_r, new_c);
					// assert(adj_unknown);
					int adj_mine = count_adj_mine(new_r, new_c);
					if (adj_mine == map[new_r][new_c]) {
						infer_is_non_mine = true;
						break;
					} else if (adj_mine + adj_unknown == map[new_r][new_c]) {
						infer_is_mine = true;
						break;
					}
				}
				if (!infer_is_mine)
					expand(start_r, start_c);
			}
		}
	}

	return 0;
}