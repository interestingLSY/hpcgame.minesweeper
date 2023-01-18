/*
	log.h - various logging functions
*/
#ifndef __MINESWEEPER_LOG_H__
#define __MINESWEEPER_LOG_H__

#include "csapp.h"	// for "extern const char* prog_name"

// Log something to stderr. The `[$prog_name]` is added automatically as a prefix
void log(const char* format, ...);

// Log something to stderr. Use the syscall `write` directly. Suitable for signal handlers
void sio_log(const char* msg);

// A string containing "This is a bug. Please contact ..."
extern const char this_is_a_bug_str[];

#endif	// __MINESWEEPER_LOG_H__