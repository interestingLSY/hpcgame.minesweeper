#include "wrappers.h"

void Pipe(int fds[2]) {
	if (pipe(fds) < 0) {
		unix_error("pipe error");
	}
}

void Usleep(useconds_t usec) {
	if (usleep(usec) < 0) {
		unix_error("usleep error");
	}
}