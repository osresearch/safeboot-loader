#ifndef _loader_pe_h_
#define _loader_pe_h_

#include <stdint.h>

extern void *
pe_find_section(
	void *image,
	const size_t image_size,
	const char *section_name,
	size_t *size_out
);

#endif
