#include "wrappers.h"
#include "log.h"

extern const char* prog_name;

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void log(const char* format, ...) {
	va_list arglist;
    va_start(arglist, format);
	Pthread_mutex_lock(&log_mutex);
	if (prog_name)
		fprintf(stderr, "[%s] ", prog_name);
    vfprintf(stderr, format, arglist);
	Pthread_mutex_unlock(&log_mutex);
    va_end(arglist);
}

void sio_log(const char* msg) {
	const char buf[] = "[] ";
	Write(STDERR_FILENO, buf, 1);
	Write(STDERR_FILENO, prog_name, sio_strlen(prog_name));
	Write(STDERR_FILENO, buf+1, 2);
	Write(STDERR_FILENO, msg, sio_strlen(msg));
}

const char this_is_a_bug_str[] = "This is a bug. Please contact interestingLSY (interestingLSY@gmail.com). \
Thank you!\n";
