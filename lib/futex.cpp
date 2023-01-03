#include <linux/futex.h>
#include <sys/syscall.h>
#include "wrappers.h"
#include "futex.h"
#include "log.h"

static int futex(uint32_t *uaddr, int futex_op, uint32_t val,
	const struct timespec *timeout, uint32_t *uaddr2, uint32_t val3) {
	return syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr, val3);
}

int futex_wait(uint32_t *futex_ptr, uint32_t old_val) {
	int rc = futex(futex_ptr, FUTEX_WAIT, old_val, NULL, NULL, 0);
	if (rc == -1 && errno != EAGAIN) {
		unix_error("futex_wait error");
	}
	return rc;
}

int futex_wake(uint32_t *futex_ptr) {
	int rc = futex(futex_ptr, FUTEX_WAKE, 1, NULL, NULL, 0);
	if (rc == -1) {
		unix_error("futex_wake error");
	}
	return rc;
}