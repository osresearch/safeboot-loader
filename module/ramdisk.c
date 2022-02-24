/* UEFI ramdisk interface
 *
 * Create a ram disk given a disk image by catting into
 * /sys/firmware/efi/ramdisk
 */
#include <linux/kernel.h>
#include "efiwrapper.h"
#include "ramdisk.h"


static ssize_t store(struct kobject * kobj, struct kobj_attribute *attr, const char * buf, size_t count)
{
	// open that file and attempt to read it
	size_t file_size;
	void * image;
	EFI_DEVICE_PATH * devicepath;

	uefi_memory_map_add();

	EFI_RAM_DISK_PROTOCOL * ramdisk = uefi_locate_and_handle_protocol(&EFI_RAMDISK_PROTOCOL_GUID);
	if (!ramdisk)
	{
		printk("uefi_ramdisk: vendor firmware has no RamDisk driver\n");
		return -1;
	}

	image = uefi_alloc_and_read_file(buf, &file_size);
	if (!image)
	{
		printk("uefi_ramdisk: alloc and read failed\n");
		return -1;
	}

	printk("uefi_ramdisk: %s %zu bytes", buf, file_size);
	ramdisk->Register(
		(UINT64) image, // physical address, since UEFI allocated it
		file_size,
		&EFI_RAMDISK_PROTOCOL_GUID,
		NULL,
		&devicepath
	);

	return count;
}


static ssize_t show(struct kobject * kobj, struct kobj_attribute * attr, char * buf)
{
	printk("uefi_ramdisk: show %llx\n", (uint64_t) buf);
	return -1;
}

static struct kobj_attribute uefi_ramdisk_attr
	= __ATTR(ramdisk, 0600, show, store);

int uefi_ramdisk_init(void)
{
	// efi_kobj is the global for /sys/firmware/efi
	int status;
	status = sysfs_create_file(efi_kobj, &uefi_ramdisk_attr.attr);

	if (status < 0)
	{
		printk("uefi_ramdisk: unable to create /sys/firmware/efi/loader: rc=%d\n", status);
		return -1;
	}

	printk("uefi_ramdisk: created /sys/firmware/efi/ramdisk\n");
	return 0;
}

