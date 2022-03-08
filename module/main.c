/** UEFI Device Driver module
 *
 * Provides an interface to several different UEFI device drivers
 * as if they were normal Linux devices.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include "efiwrapper.h"
#include "efinet.h"



static int uefi_dev_init(void)
{
	if (uefi_memory_map_add() < 0)
		return -1;

	if (uefi_loader_init() < 0)
		return -1;

	uefi_ramdisk_init();

#ifdef CONFIG_UEFIBLOCK
	if (uefi_blockdev_init() < 0)
		return -1;
#endif

#ifdef CONFIG_UEFINET
	if (uefi_nic_init() < 0)
		return -1;
#endif

#ifdef CONFIG_UEFITPM
	if (uefi_tpm_init() < 0)
		return -1;
#endif

	// todo: tear down the other devices in the event of failure

	return 0;
}

module_init(uefi_dev_init);


static void uefi_dev_exit(void)
{
	// block does not need any shutdown
	// tpm does not require any shutdown
	// ramdisk explicitly does not want to shutdown

#ifdef CONFIG_UEFINET
	uefi_nic_exit();
#endif
}

module_exit(uefi_dev_exit);


MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
