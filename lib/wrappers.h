/*
 * wrappers.h - various wrapper functions for unix functions
 */

#ifndef __MINESWEEPER_WRAPPERS_H__
#define __MINESWEEPER_WRAPPERS_H__

#include <sys/mman.h>
#include <sys/prctl.h>
#include "csapp.h" // include all wrapper functions from csapp.h

/* Create a one-way communication channel (pipe).
   If successful, two file descriptors are stored in PIPEDES;
   bytes written on fds[1] can be read from fds[0]. */
void Pipe(int fds[2]);

/* Create a one-way communication channel (pipe).
   If successful, two file descriptors are stored.
   Bytes can be written to write_fd and read from read_fd. */
void Pipe(int &read_fd, int &write_fd);

/* Sleep USECONDS microseconds, or until a signal arrives that is not blocked
   or ignored. */
void Usleep(useconds_t usec);

/* Set NAME to VALUE in the environment.
   If REPLACE is nonzero, overwrite an existing value.  */
void Setenv(const char* name, const char* value, int replace);

/* Return the value of envariable NAME, or NULL if it doesn't exist.  */
char* Getenv(const char* name);

/* Return the value of envariable NAME. If it does not exist,
   report an error and exit.  */
char* Getenv_must_exist(const char* name);

/* Execute PATH with all arguments after PATH until
   a NULL pointer  */
#define Execl(...) {execl(__VA_ARGS__); unix_error("execl error");}

void Prctl(int option, unsigned long arg2);

/* Open shared memory segment.  */
int Shm_open(const char *name, int oflag, mode_t mode);

/* Truncate the file FD is open on to LENGTH bytes.  */
void Ftruncate(int fd, off_t length);

/* Seek to a certain position on STREAM. */
void Fseek(FILE *stream, long offset, int whence);

/* Set cancelability state of current thread to STATE, returning old
   state in *OLDSTATE if OLDSTATE is not NULL.  */
void Pthread_setcancelstate(int state, int* oldstate);

/* Set cancellation state of current thread to TYPE, returning the old
   type in *OLDTYPE if OLDTYPE is not NULL.  */
void Pthread_setcanceltype(int type, int* oldtype);

#endif	// __MINESWEEPER_WRAPPERS_H__