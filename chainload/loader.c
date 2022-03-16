/*
 * Wrap the Linux kernel, initrd and commandline in an EFI PE.
 *
 * This is similar to the systemd EFI stub, except that it allocates
 * UEFI memory for the Linux kernel to run inside of, preserves the
 * UEFI context state and, by blocking the call to ExitBootServices,
 * allows Linux to return to UEFI.
 */

#include <stdint.h>
#include <string.h>
#include <efi.h>
#include <efilib.h>
#include <sys/io.h>
#include <asm/bootparam.h>
#include "resume.h"
#include "pe.h"


static void
context_save(uefi_context_t * const context)
{
	__asm__ __volatile__(
		"mov %%cr3, %0;"
		"mov %%cr0, %1;"
		"mov %%cr4, %2;"
		"mov %%cr8, %3;"
		: "=r"(context->cr3),
		  "=r"(context->cr0),
		  "=r"(context->cr4),
		  "=r"(context->cr8)
		:
		: "memory"
	);

	__asm__ __volatile__( "sidt %0" : "=m"(context->idt) : : "memory");
	__asm__ __volatile__( "sgdt %0" : "=m"(context->gdt) : : "memory");
	__asm__ __volatile__( "sldt %0" : "=m"(context->ldt) : : "memory");
}

static inline uint32_t virt2phys(const void * p)
{
	return (uint32_t)(uintptr_t) p;
}

static void * alloc_lowmem(const unsigned size)
{
	EFI_PHYSICAL_ADDRESS addr = UINT32_MAX;
	int err = gBS->AllocatePages(
		AllocateMaxAddress,
		EfiLoaderData,
		EFI_SIZE_TO_PAGES(size),
		&addr);
	if (EFI_ERROR(err))
		return NULL;

	void * const ptr = (void*) addr;

	memset(ptr, 0, size);

	return ptr;
}

static void * alloc_phys(uintptr_t base, size_t len)
{
	EFI_PHYSICAL_ADDRESS addr = base;
	
	int err = gBS->AllocatePages(
		AllocateAddress,
		EfiLoaderData,
		EFI_SIZE_TO_PAGES(len),
		&addr
	);

	if (err)
	{
		Print(u"%016lx + %08x: memory allocation failed!\n", base, len);
		return NULL;
	}

	return (void*) base;
}


static int __attribute__((__noinline__))
exec_linux(
	EFI_HANDLE * image_handle,
	EFI_SYSTEM_TABLE * ST,
	struct boot_params * boot_params,
	uefi_context_t * const context
)
{
#ifdef __x86_64__
	unsigned long start = boot_params->hdr.code32_start + 512;
#else
	unsigned long start = boot_params->hdr.code32_start;
#endif

	int (*linux_entry)(EFI_HANDLE *, EFI_SYSTEM_TABLE *, struct boot_params *)
		= (void*)(start + boot_params->hdr.handover_offset);

	context->image_handle = (uintptr_t) image_handle;
	context->system_table = (uintptr_t) ST;
	context->magic = UEFI_CONTEXT_MAGIC;

	__asm__ __volatile__(
		"cli;"
		"mov %%rsp, %0;" // cache the current stack
		"mov %%rbp, %1;" // cache the callee-saved registers
		"mov %%rbx, %2;"
		"mov %%r12, %3;"
		"mov %%r13, %4;"
		"mov %%r14, %5;"
		"mov %%r15, %6;"
		: "=m"(context->rsp),
		  "=m"(context->rbp),
		  "=m"(context->rbx),
		  "=m"(context->r12),
		  "=m"(context->r13),
		  "=m"(context->r14),
		  "=m"(context->r15)
		:
		: "memory", "cc"
	);

	// this should be a jmpq *%rax
	return linux_entry(image_handle, ST, boot_params);
}

static void * orig_exit_boot_services;
static EFI_STATUS EFIAPI
do_not_exit_boot_services(EFI_HANDLE handle, UINTN mapkey)
{
	(void) handle;
	(void) mapkey;

	Print(u"Not exiting boot services this time...\r\n");
	gBS->ExitBootServices = orig_exit_boot_services;

	return EFI_SUCCESS;
}


EFI_STATUS
//EFIAPI // not EFIAPI, since this is called via gnuefilib
efi_main(
	EFI_HANDLE image_handle,
	EFI_SYSTEM_TABLE * const ST
)
{
	InitializeLib(image_handle, ST);
	gBS = ST->BootServices;

	ST->ConOut->OutputString(ST->ConOut, u"UEFI string test\r\n");
	Print(u"efilib says hello\r\n");

	// get the memory address of our actual image
	EFI_LOADED_IMAGE * loaded_image;
	EFI_STATUS err = gBS->OpenProtocol(
		image_handle,
		&LoadedImageProtocol,
		(void **) &loaded_image,
		image_handle,
		NULL,
		EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (err)
	{
		Print(u"Could not find own image. We're done here\n");
		return EFI_NOT_FOUND;
	}

	void * const image = loaded_image->ImageBase;
	const size_t image_size = loaded_image->ImageSize;

	// find the kernel, initrd and command line PE sections
	size_t kernel_size = 0;
	size_t initrd_size = 0;
	size_t cmdline_size = 0;


	void * const kernel = pe_find_section(image, image_size, "linux", &kernel_size);
	void * const initrd = pe_find_section(image, image_size, "initrd", &initrd_size);
	void * const cmdline = pe_find_section(image, image_size, "cmdline", &cmdline_size);

	Print(u"SysTab:  %016lx\n", (unsigned long) ST);
	Print(u"BootSvc: %016lx\n", (unsigned long) ST->BootServices);
	Print(u"kernel:  %016lx + %08x\n", (unsigned long) kernel, kernel_size);
	Print(u"initrd:  %016lx + %08x\n", (unsigned long) initrd, initrd_size);
	Print(u"cmdline: %016lx + %08x\n", (unsigned long) cmdline, cmdline_size);

	if (!kernel)
	{
		Print(u"NO KERNEL. WE'RE DONE HERE\n");
		return EFI_NOT_FOUND;
	}

	// allocate some contiguous memory for the Linux application
	// start at 1GB so that it doesn't squash any other data
	// and so that the entire bottom entry for the CR3 is available
	EFI_PHYSICAL_ADDRESS linux_addr = 1 << 30;
	const size_t linux_size = 512 << 20;
	void * base = NULL;

	for(int i = 0 ; i < 64 ; i++)
	{
		base = alloc_phys(linux_addr, linux_size);
		if (base)
			break;

		//Print(u"AllocateAddress %016lx failed\r\n", (unsigned long) linux_addr);
		linux_addr += 1 << 30;
	}

	if (base == NULL)
	{
		Print(u"****** no memory allocated for linux! this is bad *****\r\n");
	} else {
		Print(u"Reserved %016lx + %08x for Linux\n", linux_addr, linux_size);
	}

	// allocate some low memory for the boot params
	struct boot_params * const boot_params = alloc_lowmem(0x4000);
	struct boot_params * const image_params = kernel;

	Print(u"params:  %016lx + %08x\n", (unsigned long) boot_params, cmdline_size);
	boot_params->hdr = image_params->hdr;
	boot_params->hdr.type_of_loader = 0xFF;

	const unsigned setup_sectors = image_params->hdr.setup_sects > 0
		? image_params->hdr.setup_sects
		: 4;

	boot_params->hdr.code32_start = virt2phys(kernel) + (setup_sectors + 1) * 512;

	// reserve space in the command line for our memory allocation
	char * cmdline_copy = alloc_lowmem(cmdline_size + 256);
	boot_params->hdr.cmd_line_ptr = virt2phys(cmdline_copy);

	if (cmdline)
	{
		Print(u"cmdline: %016lx + %08x\n", (unsigned long) cmdline_copy, cmdline_size+1);
		memcpy(cmdline_copy, cmdline, cmdline_size);
	}

	// add in the memory allocation
	CHAR16 extra_cmd[256];
	int extra_len = SPrint(extra_cmd, sizeof(extra_cmd),
		u" memmap=exactmap,128K@0G,512M@%luG",
		linux_addr >> 30
	);
	for(int i = 0 ; i < extra_len ; i++)
		cmdline_copy[cmdline_size + i - 1] = (extra_cmd[i]) & 0xFF;

	boot_params->hdr.ramdisk_image = virt2phys(initrd);
	boot_params->hdr.ramdisk_size = initrd_size;


	//alloc_phys(0, 32768); // smp trampoline

	uefi_context_t * const context = UEFI_CONTEXT;
	//Print(u"magic offset=%x\n", ((uintptr_t) &context->image_handle) - ((uintptr_t) context));

	context_save(context);
	Print(u"CR0=%016lx\n", context->cr0);
	Print(u"CR3=%016lx\n", context->cr3);
	Print(u"CR4=%016lx\n", context->cr4);
	Print(u"CR8=%016lx\n", context->cr8);
	const x86_descriptor_t * d = (void*) context->gdt;
	Print(u"GDT=%08lx + %04x\n", d->base, d->limit);
	d = (void*) context->idt;
	Print(u"IDT=%08lx + %04x\n", d->base, d->limit);
	d = (void*) context->ldt;
	Print(u"LDT=%08lx + %04x\n", d->base, d->limit);

	orig_exit_boot_services = gBS->ExitBootServices;
	gBS->ExitBootServices = do_not_exit_boot_services;
	err = exec_linux(image_handle, ST, boot_params, context);

	// restore exit boot services, in case they didn't call it
	gBS->ExitBootServices = orig_exit_boot_services;

	Print(u"Welcome back! Hope you had fun in Linux: %d\n", err);

	// returning from here *should* resume UEFI
	return 0;
}
