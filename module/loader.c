/* UEFI image loader
 *
 * Allow new EFI modules to be loaded with the Boot Services
 * loaded image protocol by catting the EFI image into
 * /sys/firmware/efi/loader
 */
#include <linux/kernel.h>
#include "efiwrapper.h"


static ssize_t store(struct kobject * kobj, struct kobj_attribute *attr, const char * buf, size_t count)
{
	// open that file and attempt to read it
	size_t file_size;
	void * image = uefi_alloc_and_read_file(buf, &file_size);

	if (!image)
		return -1;

	printk("uefi_loader: starting %s (%zu bytes)\n", buf, file_size);
	if (!uefi_load_and_start_image(image, file_size))
		return -1;

	return count;
}


static ssize_t show(struct kobject * kobj, struct kobj_attribute * attr, char * buf)
{
	printk("uefi_loader: show %llx\n", (uint64_t) buf);
	return -1;
}

static struct kobj_attribute uefi_loader_attr
	= __ATTR(loader, 0600, show, store);

int uefi_loader_init(void)
{
	// efi_kobj is the global for /sys/firmware/efi
	int status;
	status = sysfs_create_file(efi_kobj, &uefi_loader_attr.attr);

	if (status < 0)
	{
		printk("uefi_loader: unable to create /sys/firmware/efi/loader: rc=%d\n", status);
		return -1;
	}

	printk("uefi_loader: created /sys/firmware/efi/loader\n");
	return 0;
}
