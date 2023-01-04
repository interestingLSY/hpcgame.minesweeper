#include <functional>
#include <utility>
using std::max, std::min;

#include "csapp.h"
#include "minesweeper_helpers.h"
constexpr int MAXN = 65536;
constexpr int delta_xy[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};

int N, K;
Channel channel;

long cnt_empty_opened = 0;

char* map[MAXN];
constexpr char MAP_UNKNOWN = -1;
constexpr char MAP_MINE = -2;

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

void click_and_put_result(int r, int c) {
	ClickResult result = channel.click(r, c, true);
	if (result.is_skipped) return;
	if (result.is_mine) {
		map[r][c] = MAP_MINE;
	} else {
		auto open_grid_pos = *result.open_grid_pos;
		for (int i = 0; i < result.open_grid_count; ++i) {
			if (map[open_grid_pos[i][0]][open_grid_pos[i][1]] == MAP_UNKNOWN)
				cnt_empty_opened += 1;
			map[open_grid_pos[i][0]][open_grid_pos[i][1]] = open_grid_pos[i][2];
		}
	}
}

bool try_solve(int r, int c) {
	if (map[r][c] <= 0) return false;	// Not an empty grid, or the number inside it is 0
	int adj_unknown = count_adj_unknown(r, c);
	if (adj_unknown == 0) return false;
	int adj_mine = count_adj_mine(r, c);
	if (adj_mine == map[r][c]) {
		// all adjacent&unknown grids are empty grids
		for (int k = 0; k < 8; ++k) {
			int new_r = r+delta_xy[k][0], new_c = c+delta_xy[k][1];
			if (new_r < 0 || new_c < 0 || new_r >= N || new_c >= N) continue;
			if (map[new_r][new_c] == MAP_UNKNOWN) {
				click_and_put_result(new_r, new_c);
			}
		}
		return true;
	}
	// int adj_empty = count_adj_empty(r, c);
	if (adj_unknown+adj_mine == map[r][c]) {
		// all adjacent&unknown grids are mines
		for (int k = 0; k < 8; ++k) {
			int new_r = r+delta_xy[k][0], new_c = c+delta_xy[k][1];
			if (new_r < 0 || new_c < 0 || new_r >= N || new_c >= N) continue;
			map[new_r][new_c] = MAP_MINE;
		}
		return true;
	}
	return false;
}

bool try_solve_region(int r1, int c1, int r2, int c2) {
	bool flag = false;
	for (int r = r1; r < r2; ++r)
		for (int c = c1; c < c2; ++c)
			flag |= try_solve(r, c);
	return flag;
}

int main() {
	minesweeper_init(N, K);
	for (int i = 0; i < N; ++i) {
		map[i] = (char*)Malloc(N);
		memset(map[i], MAP_UNKNOWN, N);
	}
	channel = create_channel();

	int last_r = 0;
	click_and_put_result(0, 0);
	for (int i = 0; i < 128 && cnt_empty_opened < N*N-K; ++i) {
		// printf("%ld\n", cnt_empty_opened);
		for (int r = max(0, last_r-1); r < N; ++r) {
			for (int c = 0; c < N; ++c) {
				if (try_solve(r, c)) {
					while (try_solve_region(max(r-6, 0), max(c-6, 0), min(r+6, N-1), min(c+6, N-1)));
					// goto mid;
				}
			}
		}
		// mid:;
		for (int r = last_r; r < N; ++r) {
			for (int c = 0; c < N; ++c) {
				if (map[r][c] == MAP_UNKNOWN) {
					click_and_put_result(r, c);
					last_r = r;
					goto next_loop;
				}
			}
		}
		next_loop:;
	}

	for (int r = 0; r < N; ++r) {
		for (int c = 0; c < N && cnt_empty_opened < N*N-K; ++c) {
			if (map[r][c] == MAP_UNKNOWN) {
				click_and_put_result(r, c);
				while (try_solve_region(max(r-7, 0), max(c-7, 0), min(r+7, N-1), min(c+7, N-1)));
			}
		}
	}

	
	return 0;
}