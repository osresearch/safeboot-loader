/** UEFI Wrappers for common operations
 *
 */

#include <linux/kernel.h>
#include "efiwrapper.h"

static uint64_t uefi_pagetable_0;
static efi_system_table_t * gST;
efi_boot_services_t * gBS;

int uefi_memory_map_add(void)
{
	uint64_t cr3_phys;
	volatile uint64_t * linux_pagetable;
	uint64_t linux_pagetable_0;

	if (uefi_pagetable_0 == 0)
	{
		const uint64_t * const uefi_context = phys_to_virt(0x100); // hack!
		const uint64_t uefi_cr3 = uefi_context[0x40/8];
		const uint64_t * uefi_pagetable;

		if (uefi_context[0xa0/8] != 0xdecafbad)
		{
			printk("uefi context bad magic %llx, things will probably break\n", uefi_context[0xa0/8]);
			return -1;
		}

		uefi_pagetable = ioremap(uefi_cr3 & ~0xFFF, 0x1000);
		uefi_pagetable_0 = uefi_pagetable[0];
		gST = (void*) uefi_context[0x98/8]; // %rsi passed to the efi stub

		printk("UEFI CR3=%016llx CR3[0]=%016llx gST=%016llx\n", uefi_cr3, uefi_pagetable_0, (uint64_t) gST);

	}

	// get our current CR3, masking out the ASID, to get the
	// pointer to our page table
	__asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3_phys));

	linux_pagetable = phys_to_virt(cr3_phys & ~0xFFF);
	linux_pagetable_0 = linux_pagetable[0];


	if (linux_pagetable_0 != 0
	&&  linux_pagetable_0 != uefi_pagetable_0)
	{
		printk("UH OH: linux has something mapped at 0x0: CR3=%016llx CR3[0]=%016llx\n", cr3_phys, linux_pagetable_0);
		return -1;
	}

	// poke in the entry for the UEFI page table that we've stored
	// reusing UEFI's linear map of low memory
	linux_pagetable[0] = uefi_pagetable_0;

	// are we supposed to force a TLB flush or something?
	__asm__ __volatile__("mov %0, %%cr3" : : "r"(cr3_phys) : "memory");


	// we can use the UEFI address space pointers now
	gBS = gST->boottime;

	if (gBS == 0)
	{
		printk("UH OH: boot services is a null pointer?\n");
		return -1;
	}

#if 0 // debug on 5.15
	printk("gST=%llx\n", (uint64_t) gST);
	for(int i = 0 ; i < 64 ; i++)
		printk("%08x", i[(uint32_t*)gST]);
	printk("\n");

	printk("gBS=%llx\n", (uint64_t) gBS);
	for(int i = 0 ; i < 64 ; i++)
		printk("%08x", i[(uint32_t*)gBS]);
	printk("\n");
#endif

	// success!
	return 0;
}

#define EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID EFI_GUID(0x8b843e20, 0x8132, 0x4852,  0x90, 0xcc, 0x55, 0x1a, 0x4e, 0x4a, 0x7f, 0x1c)
#define EFI_DEVICE_PATH_PROTOCOL_GUID EFI_GUID(0x9576e91, 0x6d3f, 0x11d2, 0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b)

typedef void * EFI_DEVICE_PATH_PROTOCOL;

typedef CHAR16*
(EFIAPI *EFI_DEVICE_PATH_TO_TEXT_PATH)(
  IN CONST EFI_DEVICE_PATH_PROTOCOL   *DevicePath,
  IN BOOLEAN                          DisplayOnly,
  IN BOOLEAN                          AllowShortcuts
  );

typedef struct {
  void *        ConvertDeviceNodeToText;
  EFI_DEVICE_PATH_TO_TEXT_PATH ConvertDevicePathToText;
} EFI_DEVICE_PATH_TO_TEXT_PROTOCOL;

char * uefi_device_path_to_name(EFI_HANDLE dev_handle)
{
	EFI_DEVICE_PATH_TO_TEXT_PROTOCOL * dp2txt = uefi_locate_and_handle_protocol(&EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID);
	EFI_DEVICE_PATH_PROTOCOL * dp = uefi_handle_protocol(&EFI_DEVICE_PATH_PROTOCOL_GUID, dev_handle);
	char * dp2 = NULL; // wide-char return
	static char buf[256];

	if (!dp2txt || !dp)
		return "LocateHandle DevicePath failed";

	dp2 = (char*) dp2txt->ConvertDevicePathToText(dp, 0, 0);
	if (!dp2)
		return "ConvertDevicePathToText failed";

	// convert it to a normal string, ensuring there is a nul terminator at the end
	for(int i = 0 ; i < sizeof(buf)-1 ; i++)
	{
		uint16_t c = dp2[2*i];
		buf[i] = c;
		if (c == 0)
			break;
	}

	return buf;
}


int uefi_locate_handles(efi_guid_t * guid, EFI_HANDLE * handles, int max_handles)
{
	unsigned long handlesize = max_handles * sizeof(*handles);
	efi_status_t EFIAPI (*locate_handle)(int, efi_guid_t *, void *,
                                      unsigned long *, efi_handle_t *) = (void*) gBS->locate_handle;

//	printk("gBS=%llx locate=%llx\n", (uint64_t) gBS, (uint64_t) locate_handle);
//	if (locate_handle == 0)
//		print_hex_dump_bytes("bootservices", KERN_ERR, gBS, 512);

	int status = locate_handle(
		EFI_LOCATE_BY_PROTOCOL,
		guid,
		NULL,
		&handlesize,
		handles
	);

	if (status != 0)
		return -1;

	// return the count of handles
	return handlesize / sizeof(*handles);
}

void * uefi_locate_and_handle_protocol(efi_guid_t * guid)
{
	void * handles[1];
	int count = uefi_locate_handles(guid, handles, 1);
	if (count < 1)
		return NULL;
	return uefi_handle_protocol(guid, handles[0]);
}

void * uefi_handle_protocol(efi_guid_t * guid, EFI_HANDLE handle)
{
	efi_status_t EFIAPI (*handle_protocol)(efi_handle_t, efi_guid_t *, void **) = (void*) gBS->handle_protocol;

	void * proto = NULL;
	int status = handle_protocol(
		handle,
		guid,
		&proto
	);

	if (status != 0)
		return NULL;

	return proto;
}
