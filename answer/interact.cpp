#include <cstdio>
#include <iostream>
#include <cstring>

#include "minesweeper_helpers.h"

constexpr int MAXN = 256;

long N, K;
int constant_A;

// -1: unknown. -2: mine. >=0: empty grid
char map[MAXN][MAXN];

// Print the map on the terminal
void show_map() {
	// Print the header
	printf("        ");
	for (int i = 0; i < N; ++i)
		printf("%c", i%10+'0');
	putchar('\n');
	printf("        ");
	for (int i = 0; i < N; ++i)
		putchar('_');
	putchar('\n');
	// Print the map
	for (int r = 0; r < N; ++r) {
		// Print the left header
		printf("%7d|", r);
		// Print the map
		for (int c = 0; c < N; ++c) {
			if (map[r][c] == -1) putchar(' ');	// unknown
			else if (map[r][c] == -2) putchar('X');	// mine
			else putchar(map[r][c]+'0');
		}
		// Print the right header
		printf("|%d", r);
		putchar('\n');
	}
	// Print the footer
	putchar('\t');
	for (int i = 0; i < N; ++i)
		putchar('-');
	putchar('\n');
	putchar('\t');
	for (int i = 0; i < N; ++i)
		printf("%c", i%10+'0');
	putchar('\n');
}

int main() {
	memset(map, 0xff, sizeof(map));	// set to -1

	minesweeper_init(N, K, constant_A);
	
	if (N > MAXN) {
		printf("We strongly do not advise you to use this solution on maps with side length > %d\n", MAXN);
		printf("Because the map would be too big to display.\n");
		printf("P.S. The side length of the current map is %ld\n", N);
		printf("If you really want to play on a larger map, please change `MAXN` in `interact.cpp` and recompile it.\n");
		exit(1);
	}

	// 创建用于和 game server 通信的 Channel
	Channel channel = create_channel();

	while (true) {
		show_map();

		printf("Please enter the coordinate you want to click on, type `Q` to quit.\n");
		printf("> "); fflush(stdout);

		// Read the user's input
		char buf[64];
		std::cin.getline(buf, 64);
		if (buf[0] == 'Q' || buf[0] == 'q') {
			break;
		}
		int click_r, click_c;
		int num_read = sscanf(buf, "%d %d", &click_r, &click_c);
		if (num_read != 2) {
			printf("Bad input\n");
			continue;
		}

		// Click it
		ClickResult result = channel.click(click_r, click_c, false);

		// Parse the click result
		if (result.is_mine) {
			printf("BOOM!\n");
			map[click_r][click_c] = -2;
		} else {
			printf("%d grids opened\n", result.open_grid_count);
			// 遍历所有点开的格子，并把它们标注在 map[][] 数组中
			auto arr = *result.open_grid_pos;
			for (int i = 0; i < result.open_grid_count; ++i) {
				map[arr[i][0]][arr[i][1]] = arr[i][2];
			}
		}
	}

	return 0;
}