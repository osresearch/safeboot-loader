#include <stdint.h>

extern int efi_entry(void *, void*);

void __attribute__((__naked__))
entry(void)
{
	// restore the context from when UEFI invoked Linux
	// must match arch/x86/boot/compressed/head_64.S
	// and this must be invoked with the 0 page mapped
	// to us so that we can retrieve the context data
	void * UEFI_CONTEXT = (void*) 0x100;

	__asm__ __volatile__(
		"cli;"
		//"lldt 0x80(%0);"
		/* callee-saved registers */
		"movq 0x00(%0), %%rbx;"
		"movq 0x08(%0), %%rbp;"
		"movq 0x10(%0), %%r12;"
		"movq 0x18(%0), %%r13;"
		"movq 0x20(%0), %%r14;"
		"movq 0x28(%0), %%r15;"
		/* control registers */
		"movq 0x38(%0), %%rcx;"
		"movq %%rcx, %%cr0;"
		"movq 0x40(%0), %%rcx;"
		"movq %%rcx, %%cr3;"
		"movq 0x48(%0), %%rcx;"
		"movq %%rcx, %%cr4;"
		"movq 0x50(%0), %%rcx;"
		"movq %%rcx, %%cr8;"
		/* descriptor tables and segment registers*/
		"lidt 0x60(%0);"
		"lgdt 0x70(%0);"
		"mov $0x30, %%rcx;"
		"mov %%cx, %%ds;"
		"mov %%cx, %%es;"
		"mov %%cx, %%fs;"
		"mov %%cx, %%gs;"
		"mov %%cx, %%ss;"

		/* long jump to set %cs; need to use temporary stack */
		"lea 0x90(%0), %%rsp;"
		"pushq $0x38;"
		"lea new_universe(%%rip), %%rcx;"
		"pushq %%rcx;"
		"lretq;"
		// use uefi stack while calling efi_entry */
		"new_universe: nop;"
		"movq 0x30(%0), %%rsp;"
		"movq 0x90(%0), %%rdi;" // image handle
		"movq 0x98(%0), %%rsi;" // system table pointer
		"lea efi_entry(%%rip), %%rcx;"
		"pushq %%rcx;"
		"retq;"
		:
		: "a"(UEFI_CONTEXT)
		: "memory"
	);

	// should not be reached
}
