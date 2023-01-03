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

void Setenv(const char* name, const char* value, int replace) {
	if (setenv(name, value, replace) < 0) {
		unix_error("setenv error");
	}
}

char* Getenv(const char* name) {
	return getenv(name);
}

char* Getenv_must_exist(const char* name) {
	char* result;
	if ((result=getenv(name)) == NULL) {
		app_error("Environment variable %s does not exists.", name);
	}
	return result;
}

void Prctl(int option, unsigned long arg2) {
	if (prctl(option, arg2) < 0) {
		unix_error("prctl error");
	}
}

int Shm_open(const char *name, int oflag, mode_t mode) {
	int rc;
	if ((rc = shm_open(name, oflag, mode)) == -1) {
		unix_error("shm_open error");
	}
	return rc;
}

void Ftruncate(int fd, off_t length) {
	if (ftruncate(fd, length) == -1) {
		unix_error("ftruncate error");
	}
}

void Fseek(FILE *stream, long offset, int whence) {
	if (fseek(stream, offset, whence) == -1) {
		unix_error("fseek error");
	}
}