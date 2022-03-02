#ifndef _uefidev_chainload_h_
#define _uefidev_chainload_h_

#include <stdint.h>

#define CHAINLOAD_IMAGE_ADDR	0x040100000
#define CHAINLOAD_ARGS_ADDR	0x040010000
#define CHAINLOAD_ARGS_MAGIC	0x434841494e4c4431uL

typedef struct {
	uint64_t	magic;
	uint64_t	boot_device; // device handle
	uint64_t	image_addr; // physical address
	uint64_t	image_size;
} chainload_args_t;

#endif
