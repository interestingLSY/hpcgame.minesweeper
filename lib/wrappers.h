/*
 * wrappers.h - various wrapper functions for unix functions
 */

#ifndef __MINESWEEPER_WRAPPERS_H__
#define __MINESWEEPER_WRAPPERS_H__

#include "csapp.h" // include all wrapper functions from csapp.h

/* Create a one-way communication channel (pipe).
   If successful, two file descriptors are stored in PIPEDES;
   bytes written on PIPEDES[1] can be read from PIPEDES[0]. */
void Pipe(int fds[2]);
/* Sleep USECONDS microseconds, or until a signal arrives that is not blocked
   or ignored. */
void Usleep(useconds_t usec);

#endif	// __MINESWEEPER_WRAPPERS_H__