#ifndef _uefidev_chainload_h_
#define _uefidev_chainload_h_

#include <stdint.h>

#define CHAINLOAD_IMAGE_ADDR	0x040100000
#define CHAINLOAD_ARGS_ADDR	0x040010000
#define CHAINLOAD_ARGS_MAGIC	0x434841494e4c4430uL

typedef struct {
	uint64_t	magic;
	unsigned	boot_device;
	uint64_t	image_addr;
	uint64_t	image_size;
} chainload_args_t;

#endif
