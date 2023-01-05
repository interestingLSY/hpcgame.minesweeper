#include "wrappers.h"
#include "common.h"
#include "log.h"
#include "shm.h"

char* open_shm() {
	// Open the shm region
	int mem_fd = Shm_open(SHM_NAME, O_CREAT | O_RDWR, S_IRWXU);
	// Mmap-ing
	char* ptr = (char*)Mmap(NULL, TOTAL_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, 0);
	return ptr;
}

void init_shm_region(char* pos) {
	SHM_PENDING_BIT(pos) = 0;
	SHM_SLEEPING_BIT(pos) = 0;
	SHM_DONE_BIT(pos) = 0;
}