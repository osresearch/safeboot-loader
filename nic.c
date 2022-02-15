/* UEFI Network interface.
 *
 * This implements the simplest possible polled network interface
 * ontop of the EFI NIC protocol.
 */
#include "efiwrapper.h"
#include "efinet.h"

int uefi_nic_init(void)
{
	EFI_HANDLE handles[64];
	int handle_count = uefi_locate_handles(&EFI_SIMPLE_NETWORK_PROTOCOL_GUID, handles, 64);

	printk("found %d NIC handles\n", handle_count);

	for(int i = 0 ; i < handle_count ; i++)
	{
		EFI_HANDLE handle = handles[i];

		EFI_SIMPLE_NETWORK_PROTOCOL * nic = uefi_handle_protocol(&EFI_SIMPLE_NETWORK_PROTOCOL_GUID, handle);
		if (!nic)
			continue;

		printk("%d: type=%d media=%d addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
			i,
			nic->Mode->IfType,
			nic->Mode->MediaPresent,
			nic->Mode->CurrentAddress.Addr[0],
			nic->Mode->CurrentAddress.Addr[1],
			nic->Mode->CurrentAddress.Addr[2],
			nic->Mode->CurrentAddress.Addr[3],
			nic->Mode->CurrentAddress.Addr[4],
			nic->Mode->CurrentAddress.Addr[5]
		);
	}

	return 0;
}
