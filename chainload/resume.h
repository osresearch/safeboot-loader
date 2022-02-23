#ifndef _uefi_resume_h_
#define _uefi_resume_h_

/**
 * UEFI Context resuming code.
 *
 * This stores the CPU state that needs to be restored
 * when kexec from Linux back to the UEFI firmware.
 */

#include <stdint.h>

typedef struct
__attribute__((__packed__))
{
        uint16_t limit;
        uint64_t base;
} x86_descriptor_t;

typedef struct
{
	uint64_t rbx;
	uint64_t rbp;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
	uint64_t rsp;
	uint64_t cr0;
	uint64_t cr3;
	uint64_t cr4;
	uint64_t cr8;
	uint64_t resv0;
	//x86_descriptor_t idt;
	uint8_t idt[16];
	//x86_descriptor_t gdt;
	uint8_t gdt[16];
	//x86_descriptor_t ldt;
	uint8_t ldt[16];
	uint64_t image_handle;
	uint64_t system_table;
	uint64_t magic;
	uint64_t temp_stack[4];
} uefi_context_t;

#define UEFI_CONTEXT_OFFSET ((uint64_t) 0x100)
#define UEFI_CONTEXT_MAGIC ((uint64_t) 0xdecafbad)
#define UEFI_CONTEXT ((uefi_context_t*) UEFI_CONTEXT_OFFSET)

#endif

