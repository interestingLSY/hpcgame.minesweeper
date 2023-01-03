#include "wrappers.h"
#include "common.h"

void exit_when_parent_dies() {
	Prctl(PR_SET_PDEATHSIG, SIGKILL);
	if (getppid() == 1) exit(0);
}