/** UEFI Block Device.
 *
 * Implements a simplistic block device that calls back
 * into the UEFI BlockDeviceProtocol.
 */

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/efi.h>
#include <asm/efi.h>
/*
#include "efibind.h"
#include "efidef.h"
#include "efiapi.h"
*/
//#include "efi.h"
#include "blockio.h"

#define DRIVER_NAME	"uefiblockdev"
#define DRIVER_VERSION	"v0.1"
#define DRIVER_AUTHOR	"Trammell Hudson"
#define DRIVER_DESC	"UEFI Block Device driver"

static int major;



typedef struct {
	spinlock_t lock;
	struct gendisk *gd;

	EFI_BLOCK_IO_PROTOCOL * uefi;
} uefiblockdev_t;

static void * __init uefiblockdev_add(int minor, EFI_BLOCK_IO_PROTOCOL * bio)
{
	uefiblockdev_t * const dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return NULL;

	spin_lock_init(&dev->lock);
	dev->gd = alloc_disk(1);
	if (!dev->gd)
		return NULL;

	const EFI_BLOCK_IO_MEDIA * const media = bio->Media;
	printk("bio=%p rev=%08x media=%p\n", bio, bio->Revision, media);

	printk("id=%d removable=%d present=%d logical=%d ro=%d caching=%d bs=%u size=%zu\n",
		media->MediaId,
		media->RemovableMedia,
		media->MediaPresent,
		media->LogicalPartition,
		media->ReadOnly,
		media->WriteCaching,
		media->BlockSize,
		media->LastBlock * media->BlockSize);
/*
	dev->uefi = uefi;
	dev->major = major;
	dev->first_minor = minor;
	dev->minors = 1;
	strncpy(dev->disk_name, "UEFI-DISK", sizeof(dev->disk_name));
	dev->fops = ;
	dev->queue = ;
	dev->flags = 0;
	dev->capacity = ;

	add_disk(dev->gd);

	blk_register_
*/

	return dev;
}

static int __init uefiblockdev_init(void)
{
	major = register_blkdev(0, DRIVER_NAME);
	if (major < 0)
		return -EIO;

	// figure out how many disks we have
	//extern void ** efi;
	//printk("efi=%p\n", efi);
	//EFI_BOOT_SERVICES * BS = (void*) efi.systab->boottime;
	//efi_enter_virtual_mode();

	// get our real CR3 and add an entry for the low memory
	uint64_t cr3_phys;
	__asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3_phys));
	uint64_t * linux_pagetable = phys_to_virt(cr3_phys & ~0xFFF);
	printk("linux CR3=%016lx CR3[0]=%016lx\n", cr3_phys, linux_pagetable[0]);

	if (linux_pagetable[0] != 0)
		printk("UH OH: Linux has something mapped at 0x0, this will go poorly\n");

	const uint64_t * const uefi_context = phys_to_virt(0x100); // hack!
	const uint64_t uefi_cr3 = uefi_context[0x40/8];
	printk("UEFI CR3=%016lx\n", uefi_cr3);
	const uint64_t * uefi_pagetable = ioremap(uefi_cr3, 0x1000);
	printk("UEFI CR3[0]=%016lx\n", uefi_pagetable[0]);

	// poke in the entry for the page table that we've stored
	linux_pagetable[0] = uefi_pagetable[0];

	// we should be able to do stuff with UEFI now...

	//EFI_SYSTEM_TABLE * const ST = phys_to_virt(0x7fbee018); // hack!
	efi_system_table_t * const ST = 0x7fbee018; // hack!
	printk("efi->systab=%016lx\n", (unsigned long) ST);
	efi_boot_services_t * BS = ST->boottime;
	printk("bootservices=%016lx\n", (unsigned long) BS);

/*
	pgd_t * const save_pgd = efi_call_phys_prolog();
	unsigned long flags;
	local_irq_save(flags);
	arch_efi_call_virt_setup();
*/


	void * handles[64];

	uint64_t handlesize = sizeof(handles);
	//arch_efi_call_virt(BS, locate_handle,
	int status = efi_call(BS->locate_handle,
		EFI_LOCATE_BY_PROTOCOL,
		&EFI_BLOCK_IO_PROTOCOL_GUID,
		NULL,
		&handlesize,
		handles
	);
	const unsigned handle_count = handlesize / sizeof(handles[0]);
	printk("rc=%d %d block devices\n", status, handle_count);

	for(unsigned i = 0 ; i < handle_count ; i++)
	{
		const void * handle = handles[i];
		EFI_BLOCK_IO_PROTOCOL * bio = NULL;
		status = efi_call(BS->handle_protocol,
			handle,
			&EFI_BLOCK_IO_PROTOCOL_GUID,
			(void**) &bio
		);
	
		if (status != 0)
			continue;

		printk("%d: handle %p works: block proto %p\n", i, handle, bio);
		uefiblockdev_add(i, bio);
	}

/*
	local_irq_restore(flags);
	efi_call_phys_epilog(save_pgd);
	arch_efi_call_virt_teardown();
*/


	return -1;
}

module_init(uefiblockdev_init);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
