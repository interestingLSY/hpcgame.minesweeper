#include "log.h"

extern const char* prog_name;

void log(const char* format, ...) {
	va_list arglist;
    va_start(arglist, format);
	if (prog_name)
		fprintf(stderr, "[%s] ", prog_name);
    vfprintf(stderr, format, arglist);
    va_end(arglist);
}