/*
	shm.h - Utilities for manuplating the shm (shared memory)
*/
#ifndef __MINESWEEPER_SHM_H__
#define __MINESWEEPER_SHM_H__

// The maximum number of channels the player's program can have
#define MAX_CHANNEL 1024

// The size of each shared memory region held by a channel, in bytes
#define CHANNEL_SHM_SIZE (256*1024)	// 256 KB

// The size of the shm region
#define TOTAL_SHM_SIZE (MAX_CHANNEL*CHANNEL_SHM_SIZE)

// Macros for accessing members inside a shm region
#define SHM_PENDING_BIT(pos) (*((volatile unsigned int*)(pos)))
#define SHM_PENDING_BIT_PTR(pos) ((unsigned int*)(pos))
#define SHM_SLEEPING_BIT(pos) (*((volatile unsigned int*)(pos+4)))
#define SHM_SLEEPING_BIT_PTR(pos) (((unsigned int*)(pos+4)))
#define SHM_DONE_BIT(pos) (*((volatile unsigned int*)(pos+8)))
#define SHM_SKIP_WHEN_REOPEN_BIT(pos) (*((volatile unsigned int*)(pos+12)))
#define SHM_DO_NOT_EXPAND_BIT(pos) (*((volatile unsigned int*)(pos+16)))
#define SHM_CLICK_R(pos) (*((volatile unsigned short*)(pos+20)))
#define SHM_CLICK_C(pos) (*((volatile unsigned short*)(pos+22)))
#define SHM_OPENED_GRID_COUNT(pos) (*((volatile int*)(pos+24)))
#define SHM_OPENED_GRID_ARR(pos) ((unsigned short (*)[16384][3])(pos+28))

// Open the shared memory (shm), and return a pointer pointing to its head
char* open_shm(const char* shm_name);

// Initialize a shm region
// This is supposed to be called by the game server
void init_shm_region(char* pos);

// Generate a random shm name
// Looks like `/minesweeper_shm_12556206635472641212`
void generate_random_shm_name(char* result);

#endif // __MINESWEEPER_SHM_H__