/*
	expand_with_queue_mt.cpp - The multithread version of `expand_with_queue`.

	    This is the standard solution (STD). This solution achieves an amazing
	accuracy (~0.01% mine hit rate) and an fascinating speed. (Can solve map
	with a side length of 65536 in ~120s on my 5800H).

		Idea: we can see each thread as a "miner". It starts with a non-mine
	grid (by simply clicking until we find one) and tries its best to expand the
	"safe region" (namely, the non-mine connected component containing the starting grid).
		In detail, it maintains the "border" of the current "safe region" with
	a queue. The "border" includes all grids that:
		- In the current "safe region"
		- There are at least one unknown grid in its neighbour
		For more details, please refer to `expand()`.

		What to do when two threads (miners) "meet" at one grid (that is, their
	"safe region"s overlap): We create another N*N array, `marker`. When a thread
	wants to put a particular grid (r, c) into its queue, it first checks whether
	`marker[r][c] == 0`. If the answer is YES, it marks `marker[r][c]` as its
	own thread id and put the grid into the queue. If the answer is NO, which
	means that the grid has been "snatched" by another worker, it just skips
	the grid.
*/

#include <functional>
#include <utility>
#include <queue>
#include <deque>
#include <random>
#include <cassert>
#include <atomic>
using std::max, std::min, std::atomic;

#include "csapp.h"
#include "minesweeper_helpers.h"

constexpr int NUM_WORKER_THREAD = 8;

constexpr int MAXN = 65536;
constexpr int delta_xy[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};

std::mt19937 rng(0);

// N: 地图的边长
// K: 地图中的雷的数量
// constant_A: 评分过程中用到的参数 A
long N, K;
int constant_A;

// We use "approximate counting" to avoid data race while achieving a better performance
// That is, each thread maintains a "local counter", and there is one "global
// counter" (`cnt_empty_opened`). A threads adds to its own local counter by
// default, and when the local counter reaches `APPROX_COUNTING_THRES`, the thread
// adds `APPROX_COUNTING_THRES` to the global counter and sets the local counter
// to 0.
// This is the technique called "Approximate Counting", which balances accuracy
// and performance.
atomic<long> cnt_empty_opened = 0;
constexpr int APPROX_COUNTING_THRES = 32;

// Variables and functions for maintaining the map
char* map[MAXN];
constexpr char MAP_UNKNOWN = -1;
constexpr char MAP_MINE = -2;

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
bool click_and_put_result_no_expand(Channel &channel, int r, int c, long &local_cnt_empty_opened) {
	ClickResult result = channel.click_do_not_expand(r, c);
	if (result.is_mine) {
		map[r][c] = MAP_MINE;
	} else {
		if (map[r][c] == MAP_UNKNOWN) {
			local_cnt_empty_opened++;
			if (local_cnt_empty_opened == APPROX_COUNTING_THRES) {
				cnt_empty_opened += APPROX_COUNTING_THRES;
				local_cnt_empty_opened = 0;
			}
			map[r][c] = (*result.open_grid_pos)[0][2];
		}
	}
	return result.is_mine;
}


// 原则：每个数字格子最多被一个线程入队
// 所以我们用 marker 记录第一个点开这个格子的线程的 TID
char* marker[MAXN];

struct QueueItem {
	int r, c;
	int re_put_cnt;
};

void expand(int start_r, int start_c, int thread_id, Channel &channel, long &local_cnt_empty_opened) {
	std::deque<QueueItem> border;
	bool first_is_mine = click_and_put_result_no_expand(channel, start_r, start_c, local_cnt_empty_opened);
	if (first_is_mine) {
		return;
	}
	border.clear();
	border.push_back({start_r, start_c, 0});

	while (!border.empty()) {
		if (cnt_empty_opened == N*N-K) break;

		auto [cur_r, cur_c, re_put_cnt] = border.front();
		border.pop_front();

		int num_in_grid = map[cur_r][cur_c];

		int adj_unknown = count_adj_unknown(cur_r, cur_c);
		if (!adj_unknown) {
			// 它周围已经没有未知的格子了
			continue;
		}

		int adj_mine = count_adj_mine(cur_r, cur_c);
		if (adj_mine == num_in_grid) {
			// 所有未知的格子都是安全的
			for (int k = 0; k < 8; ++k) {
				int new_r = cur_r + delta_xy[k][0];
				int new_c = cur_c + delta_xy[k][1];
				if (__builtin_expect(!within_range(new_r, new_c), false)) continue;
				if (map[new_r][new_c] != MAP_UNKNOWN) continue;
				if (marker[new_r][new_c] != 0 && marker[new_r][new_c] != thread_id) continue;
				marker[new_r][new_c] = thread_id;
				bool is_mine = click_and_put_result_no_expand(channel, new_r, new_c, local_cnt_empty_opened);
				assert(!is_mine);
				border.push_front({new_r, new_c, 0});
			}
		} else if (adj_unknown+adj_mine == num_in_grid) {
			// 所有未知的格子都是雷
			for (int k = 0; k < 8; ++k) {
				int new_r = cur_r + delta_xy[k][0];
				int new_c = cur_c + delta_xy[k][1];
				if (__builtin_expect(!within_range(new_r, new_c), false)) continue;
				if (map[new_r][new_c] != MAP_UNKNOWN) continue;
				map[new_r][new_c] = MAP_MINE;
			}
		} else {
			// 暂时推不出来，把这个格子放到队列的最后。
			// 每个格子最多被如此重新入队 16 次。
			if (re_put_cnt < 16) {
				if (marker[cur_r][cur_c] != 0 && marker[cur_r][cur_c] != thread_id) continue;
				marker[cur_r][cur_c] = thread_id;
				border.push_back({cur_r, cur_c, re_put_cnt+1});
			}
		}
	}	
}


void* thread_routine(void* arg) {
	// 随机开操
	Channel channel = create_channel();
	int thread_id = (int)(long)arg;
	long local_cnt_empty_opened = 0;
	const long break_thres = (N*N-K)*95/100;
	while (cnt_empty_opened <= break_thres) {
		int start_r, start_c;
		do {
			start_r = rng()&(N-1);
			start_c = rng()&(N-1);
			if (cnt_empty_opened > break_thres) {
				return NULL;
			}
		} while (map[start_r][start_c] != MAP_UNKNOWN);
		expand(start_r, start_c, thread_id, channel, local_cnt_empty_opened);
		cnt_empty_opened += local_cnt_empty_opened;
		local_cnt_empty_opened = 0;
	}
	return NULL;
}

int main() {
	minesweeper_init(N, K, constant_A);

	for (int i = 0; i < N; ++i) {
		map[i] = (char*)Malloc(N);
		memset(map[i], MAP_UNKNOWN, N);
		marker[i] = (char*)Calloc(N, 1);
	}
	
	pthread_t tids[NUM_WORKER_THREAD];
	for (int thread_id = 1; thread_id <= NUM_WORKER_THREAD; ++thread_id) {
		Pthread_create(tids+thread_id-1, NULL, thread_routine, (void*)(long)thread_id);
	}
	for (int i = 0; i < NUM_WORKER_THREAD; ++i) {
		Pthread_join(tids[i], NULL);
	}

	// 如果不执行下面这段“按顺序开操”，那么会漏掉一些格子没有点开
	// 但是踩雷数量会大幅下降
	// 不过不论这里加不加 return 0，这份程序都可以满分
	// return 0;

	// printf("Enter phase 2\n");

	// 按顺序开操
	Channel channel = create_channel();
	long local_cnt_empty_opened = 0;
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
				// 如果我们不知道这个格子一定是雷，我们就尝试从这个点开始 expand
				if (!infer_is_mine)
					expand(start_r, start_c, 100, channel, local_cnt_empty_opened);
			}
		}
	}

	return 0;
}