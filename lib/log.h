/*
	log.h - various logging functions
*/
#ifndef __MINESWEEPER_LOG_H__
#define __MINESWEEPER_LOG_H__

#include "csapp.h"	// for "extern const char* prog_name"

// Log something to stderr. The `[$prog_name]` is added automatically as prefix
void log(const char* format, ...);

// Log something to stderr. Use `write` syscall directly. Suitable for signal handlers
void sio_log(const char* msg);

// "This is a bug. Please contact ..."
extern const char this_is_a_bug_str[];

#endif	// __MINESWEEPER_LOG_H__