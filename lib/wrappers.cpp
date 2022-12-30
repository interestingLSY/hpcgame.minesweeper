#include "wrappers.h"

void Pipe(int fds[2]) {
	if (pipe(fds) < 0) {
		unix_error("pipe error");
	}
}

void Pipe(int &read_fd, int &write_fd) {
	int fds[2];
	if (pipe(fds) < 0) {
		unix_error("pipe error");
	}
	read_fd = fds[0];
	write_fd = fds[1];
}

void Usleep(useconds_t usec) {
	if (usleep(usec) < 0) {
		unix_error("usleep error");
	}
}