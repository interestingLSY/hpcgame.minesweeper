#include <random>
#include <chrono>

#include "wrappers.h"
#include "common.h"
#include "log.h"
#include "shm.h"

char* open_shm(const char* shm_name) {
	// Open the shm region
	int mem_fd = Shm_open(shm_name, O_CREAT | O_RDWR, S_IRWXU);
	// Mmap-ing
	char* ptr = (char*)Mmap(NULL, TOTAL_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, 0);
	return ptr;
}

void init_shm_region(char* pos) {
	SHM_PENDING_BIT(pos) = 0;
	SHM_SLEEPING_BIT(pos) = 0;
	SHM_DONE_BIT(pos) = 0;
}

void generate_random_shm_name(char* result) {
	static std::mt19937_64 rng(std::chrono::high_resolution_clock::now().time_since_epoch().count());
	rng(); rng();
	unsigned long long shm_tag = rng();
	sprintf(result, "/minesweeper_shm_%llu", shm_tag);
}