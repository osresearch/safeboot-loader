/*
 * Chainload into a new UEFI executable.
 * This is not normal code; it has to run on bare metal and
 * can't make any system calls or use many libraries,
 * and has to re-configure the UEFI that had been on the machine.
 *
 * Oh, and it uses the wrong ABI.
 * Calling convention is Microsoft x64: RCX, RDX, R8, R9
 *
 * when linux is started with memmap=exactmap,32K@0G,512M@1G
 * it is safe to load this at the start of Linux's region
 * so that it does not disturb UEFI:
 *
[    0.000000] user-defined physical RAM map:
[    0.000000] user: [mem 0x0000000000000000-0x0000000000007fff] usable
[    0.000000] user: [mem 0x0000000040000000-0x000000005fffffff] usable
 *
 *
kexec-load \
	-e 0x40000000 \
	0x0=context.bin \
	0x40000000=bin/chainload.bin \
	0x40010000=/path/to/next/file
&& kexec -e
 */

// flag all EFI calls as using the Microsoft ABI
// Calling convention is args in RCX, RDX, R8, R9,
// plus shadow stack allocation for arguments.
#define EFIAPI __attribute__((__ms_abi__))

#include <stdint.h>
#include <efi.h>
#include <efilib.h>
#include <sys/io.h>

EFI_DEVICE_PATH_PROTOCOL * find_sfs(unsigned which)
{
	EFI_HANDLE handles[64];
	UINTN handlebufsz = sizeof(handles);
	EFI_GUID sfsguid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
	EFI_GUID devguid = EFI_DEVICE_PATH_PROTOCOL_GUID;

	int status = gBS->LocateHandle(
		ByProtocol,
		&sfsguid,
		NULL,
		&handlebufsz,
		handles);
	if (status != 0)
		return NULL;

	// Now we must loop through every handle returned, and open it up
	UINTN num_handles = handlebufsz / sizeof(EFI_HANDLE);
	Print(u"%d handles\n", num_handles);

	if (num_handles < which)
		return NULL;

	EFI_DEVICE_PATH_PROTOCOL* devpath = NULL;
	status = gBS->HandleProtocol(
		handles[which],
		&devguid,
		(VOID**)&devpath);
	if (status != 0)
		return NULL;

	return devpath;
}


int
efi_entry(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE * const ST)
{
	InitializeLib(image_handle, ST);
	gBS = ST->BootServices;

	Print(u"chainload says hello\r\n");

	// let's find the path to the boot device
	// for now we hard code it as the second filesystem device
	int which_fs = 1;
	EFI_DEVICE_PATH_PROTOCOL * dp = find_sfs(which_fs);

	if (!dp)
	{
		Print(u"unable to find filesystem %d\r\n", which_fs);
	} else {
		CHAR16* dp_str = DevicePathToStr(dp);
		Print(u"boot device %s\n", dp_str);
	}

	// let's try to load image on the boot loader..
	// even if we don't have a filesystem
	EFI_HANDLE new_image_handle;
	int status = gBS->LoadImage(
		0,
		image_handle,
		dp,
		(void*) 0x040100000,
		1474896,
		&new_image_handle
	);
	
	status = gBS->StartImage(new_image_handle, NULL, NULL);

	Print(u"status? %d\r\n", status);

	// returning from here *should* resume UEFI
	return 0;
}
