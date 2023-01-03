/*
	futex.h - Functions (wrappers) for manuplating futexes
*/

#ifndef __MINESWEEPER_FUTEX_H__
#define __MINESWEEPER_FUTEX_H__

int futex_wait(uint32_t *futex_ptr, uint32_t old_val);

int futex_wake(uint32_t *futex_ptr);

#endif	// __MINESWEEPER_FUTEX_H__