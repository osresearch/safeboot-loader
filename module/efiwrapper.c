/** UEFI Wrappers for common operations
 *
 */

#include <linux/kernel.h>
#include "efiwrapper.h"

static uint64_t uefi_pagetable_0;
static efi_system_table_t * gST;
efi_boot_services_t * gBS;
static EFI_HANDLE kernel_handle;

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
		kernel_handle = (void*) uefi_context[0x90/8]; // %rdi passed to the efi stub
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


void * uefi_alloc(size_t len)
{
	UINTN pages = (len + 4095) / 4096;
	EFI_PHYSICAL_ADDRESS uefi_buffer;

	efi_status_t EFIAPI (*allocate_pages)(int, int, unsigned long, efi_physical_addr_t *)
		= (void*) gBS->allocate_pages;


	int status = allocate_pages(
		EFI_ALLOCATE_ANY_PAGES,
		EFI_BOOT_SERVICES_DATA,
		pages,
		&uefi_buffer
	);

	if (status != 0)
		return NULL;

	return (void*) uefi_buffer;
}

#define EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID EFI_GUID(0x8b843e20, 0x8132, 0x4852,  0x90, 0xcc, 0x55, 0x1a, 0x4e, 0x4a, 0x7f, 0x1c)
#define EFI_DEVICE_PATH_PROTOCOL_GUID EFI_GUID(0x9576e91, 0x6d3f, 0x11d2, 0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b)

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

typedef
EFI_STATUS
(EFIAPI *EFI_IMAGE_LOAD) (
    IN BOOLEAN                      BootPolicy,
    IN EFI_HANDLE                   ParentImageHandle,
    IN EFI_DEVICE_PATH              *FilePath,
    IN VOID                         *SourceBuffer   OPTIONAL,
    IN UINTN                        SourceSize,
    OUT EFI_HANDLE                  *ImageHandle
    );

typedef
EFI_STATUS
(EFIAPI *EFI_IMAGE_START) (
    IN EFI_HANDLE                   ImageHandle,
    OUT UINTN                       *ExitDataSize,
    OUT CHAR16                      **ExitData  OPTIONAL
    );


EFI_HANDLE uefi_load_and_start_image(void * buf, size_t len)
{
	EFI_IMAGE_LOAD load_image = (void*) gBS->load_image;
	EFI_IMAGE_START start_image = (void*) gBS->start_image;
	EFI_HANDLE image_handle;
	CHAR16 * exit_data;
	UINTN exit_data_size;
	int status;

	status = load_image(
		0,
		kernel_handle,
		NULL,
		buf,
		len,
		&image_handle
	);

	if (status != 0)
		return NULL;

	status = start_image(image_handle, &exit_data_size, &exit_data);

	printk("uefi_loader: status=%d exit_data=%lld\n", status, exit_data_size);
	if (status != 0)
		return NULL;

	return image_handle;
}


void * uefi_alloc_and_read_file(const char * filename, size_t * size_out)
{
	loff_t file_size;
	loff_t pos = 0;
	void * image;
 	struct file * file;
	ssize_t rc;

	file = filp_open(filename, O_RDONLY, 0);
	if (file == NULL)
	{
		printk("uefi_loader: unable to open '%s'\n", filename);
		goto fail_open;
	}

	file_size = i_size_read(file_inode(file));
	printk("uefi_loader: %s => %lld\n", filename, file_size);

	// use UEFI to allocate the memory, which is a bit bonkers
	uefi_memory_map_add();

	image = uefi_alloc(file_size);
	if (!image)
	{
		printk("uefi_loader: could not allocate %lld bytes", file_size);
		goto fail_alloc;
	}

	rc = kernel_read(file, image, file_size, &pos);
	if (rc != file_size)
	{
		printk("uefi_loader: did not read entire file: %zu\n", rc);
		goto fail_read;
	}

	filp_close(file, NULL);
	if (size_out)
		*size_out = file_size;

	return image;

fail_read:
	// todo? should free image
fail_alloc:
	filp_close(file, NULL);
fail_open:
	return NULL;
}
