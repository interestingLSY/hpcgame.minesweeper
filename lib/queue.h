/*
	queue.h - Queue for BFS
*/

#ifndef __MINESWEEPER_QUEUE_H__
#define __MINESWEEPER_QUEUE_H__

#include "common.h"

struct Queue {
	int q[MAX_OPEN_GRID+16][2];
	int head, tail;

	void clear();
	void push(int r, int c);
	void pop(int &r, int &c);
	bool empty();
};

#endif	// __MINESWEEPER_QUEUE_H__