/*
 * Chainload into a new UEFI executable.
 *
 * This is sort-of-UEFI code; it runs on the bare metal after
 * the kexec_load() has transfered control to the entry point,
 * which restored UEFI context.
 *
 * It links against the gnuefi library and uses the sysV ABI,
 * and calls into UEFI MS ABI functions (RCX, RDX, R8, R9).
 *
 * You're not expected to run this directly; it will be linked
 * into the Linux chainload tool and then passed via kexec_load
 * to the new universe.
 */

#include <stdint.h>
#include <efi.h>
#include <sys/io.h>
#include "chainload.h"

#ifdef CONFIG_EFILIB
#include <efilib.h>
#else
static EFI_BOOT_SERVICES * gBS;
#endif

static EFI_DEVICE_PATH_PROTOCOL * find_boot_device(uint64_t handle)
{
	EFI_GUID devpath_guid = EFI_DEVICE_PATH_PROTOCOL_GUID;
	EFI_DEVICE_PATH_PROTOCOL* boot_device = NULL;

	if (handle == 0)
		return NULL;

	int status = gBS->HandleProtocol(
		(EFI_HANDLE) handle,
		&devpath_guid,
		(VOID**)&boot_device);
	if (status != 0)
		return NULL;

	return boot_device;
}

static CHAR16 * devpath2txt(EFI_DEVICE_PATH_PROTOCOL * dp)
{
	EFI_HANDLE handles[64];
	UINTN handlebufsz = sizeof(handles);
	EFI_GUID devpath2txt_guid = EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID;

	int status = gBS->LocateHandle(
		ByProtocol,
		&devpath2txt_guid,
		NULL,
		&handlebufsz,
		handles);
	if (status != 0)
		return u"NO-PROTOCOL";

	// Now we must loop through every handle returned, and open it up
	UINTN num_handles = handlebufsz / sizeof(EFI_HANDLE);
	if (num_handles < 1)
		return u"NO-HANDLES";

	EFI_DEVICE_PATH_TO_TEXT_PROTOCOL * dp2txt = NULL;

	status = gBS->HandleProtocol(
		handles[0],
		&devpath2txt_guid,
		(void**) &dp2txt);
	if (status != 0)
		return u"NO-HANDLER";

	return dp2txt->ConvertDevicePathToText(dp, 0, 0);
}


int
efi_entry(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE * const ST)
{
	ST->ConOut->OutputString(ST->ConOut, u"chainload says hello\r\n");

#ifdef CONFIG_EFILIB
	InitializeLib(image_handle, ST);
#endif
	gBS = ST->BootServices;

	const chainload_args_t * args = (void*) CHAINLOAD_ARGS_ADDR;

	if (args->magic != CHAINLOAD_ARGS_MAGIC)
#ifdef CONFIG_EFILIB
		Print(u"chainload wrong magic %x != %x\r\n",
			args->magic, CHAINLOAD_ARGS_MAGIC);
#else
		ST->ConOut->OutputString(ST->ConOut, u"chainload wrong magic\r\n");
#endif


	// let's find the path to the boot device
	// for now we hard code it as the second filesystem device
	EFI_DEVICE_PATH_PROTOCOL * boot_device = find_boot_device(args->boot_device);

	CHAR16 * boot_path = boot_device ? devpath2txt(boot_device) : u"NONE";
#ifdef CONFIG_EFILIB
	Print(u"Boot device %d: %s\r\n", args->boot_device, boot_path);
#else
	ST->ConOut->OutputString(ST->ConOut, u"Boot device ");
	ST->ConOut->OutputString(ST->ConOut, boot_path);
	ST->ConOut->OutputString(ST->ConOut, u"\r\n");
#endif

	// let's try to load image on the boot loader..
	// even if we don't have a filesystem
	EFI_HANDLE new_image_handle;
	int status = gBS->LoadImage(
		0, // BootPolicy; ignored since addr is not NULL
		image_handle,
		boot_device,
		(void*) args->image_addr,
		args->image_size,
		&new_image_handle
	);

	status = gBS->StartImage(new_image_handle, NULL, NULL);

#ifdef CONFIG_EFILIB
	Print(u"status? %d\r\n", status);
#else
	ST->ConOut->OutputString(ST->ConOut, status ? u"FAILED\r\n" : u"SUCCESS\r\n");
#endif

	// returning from here *should* resume UEFI
	return 0;
}
