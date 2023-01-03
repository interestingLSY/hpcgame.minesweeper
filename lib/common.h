/*
	common.h - Common functions used by player's program, the game server and
	the judger.
*/
#ifndef __MINESWEEPER_COMMON_H__
#define __MINESWEEPER_COMMON_H__

// Let the process exits when its parent dies
void exit_when_parent_dies();

constexpr int MAX_OPEN_GRID = 16384;

#endif	// __MINESWEEPER_COMMON_H__