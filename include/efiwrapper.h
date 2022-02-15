/** UEFI Block Device.
 *
 * Implements a simplistic block device that calls back
 * into the UEFI BlockDeviceProtocol.
 */
#ifndef _uefiblockdev_efi_wrapper_h_
#define _uefiblockdev_efi_wrapper_h_

#include <linux/kernel.h>
#include <linux/efi.h>
#include <asm/io.h>
#include <asm/efi.h>
//#include "efistub.h"

#define DRIVER_NAME	"uefidev"
#define DRIVER_VERSION	"v0.1"
#define DRIVER_AUTHOR	"Trammell Hudson"
#define DRIVER_DESC	"UEFI Device Driver"


/* Things to make edk2 EFI headers work */
typedef uint64_t          EFI_LBA;
typedef int EFI_STATUS;
typedef uint8_t BOOLEAN;
typedef uint64_t UINTN;
typedef uint32_t UINT32;
typedef void VOID;
typedef void * EFI_HANDLE;
typedef uint16_t CHAR16;

#define CONST const
#define IN /* in */
#define OUT /* out */
#define EFIAPI __attribute__((ms_abi))

#ifndef EFI_LOCATE_BY_PROTOCOL
#define EFI_LOCATE_BY_PROTOCOL			2
#endif

/* Helper functions to make it bearable to call EFI functions */
extern efi_boot_services_t * gBS;

extern int uefi_memory_map_add(void);
extern char * uefi_device_path_to_name(EFI_HANDLE dev_handle);
extern int uefi_locate_handles(efi_guid_t * guid, EFI_HANDLE * handles, int max_handles);
extern EFI_HANDLE uefi_locate_handle(efi_guid_t * guid);
extern void * uefi_handle_protocol(efi_guid_t * guid, EFI_HANDLE handle);
extern void * uefi_locate_and_handle_protocol(efi_guid_t * guid);

/* Device driver init functions go here */
extern int uefi_blockdev_init(void);

#endif

