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
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef void VOID;
typedef void * EFI_HANDLE;
typedef uint16_t CHAR16;
typedef uint64_t EFI_PHYSICAL_ADDRESS;
typedef efi_guid_t EFI_GUID;

#define CONST const
#define IN /* in */
#define OUT /* out */
#define OPTIONAL /* optional */
#define EFIAPI __attribute__((ms_abi))

typedef struct {
    UINT8                   Addr[4];
} EFI_IPv4_ADDRESS;

typedef struct {
    UINT8                   Addr[16];
} EFI_IPv6_ADDRESS;

typedef struct {
    UINT8                   Addr[32];
} EFI_MAC_ADDRESS;

typedef union {
    UINT32      Addr[4];
    EFI_IPv4_ADDRESS    v4;
    EFI_IPv6_ADDRESS    v6;
} EFI_IP_ADDRESS;

#define EFI_DEVICE_PATH_PROTOCOL_GUID EFI_GUID(0x9576e91, 0x6d3f, 0x11d2, 0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b)
typedef void * EFI_DEVICE_PATH_PROTOCOL;

#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID EFI_GUID( 0x964e5b22, 0x6459, 0x11d2, 0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b)


typedef void * EFI_DEVICE_PATH;

typedef void * EFI_EVENT;


#define INTERFACE_DECL(x) struct x

#ifndef EFI_LOCATE_BY_PROTOCOL
#define EFI_LOCATE_BY_PROTOCOL			2
#endif

/* Helper functions to make it bearable to call EFI functions */
extern efi_boot_services_t * gBS;

extern int uefi_memory_map_add(void);
extern void * uefi_alloc(size_t len);
extern char * uefi_device_path_to_name(EFI_HANDLE dev_handle);
extern int uefi_locate_handles(efi_guid_t * guid, EFI_HANDLE * handles, int max_handles);
extern EFI_HANDLE uefi_locate_handle(efi_guid_t * guid);
extern void * uefi_handle_protocol(efi_guid_t * guid, EFI_HANDLE handle);
extern void * uefi_locate_and_handle_protocol(efi_guid_t * guid);
extern EFI_HANDLE uefi_load_and_start_image(void * buf, size_t len, EFI_DEVICE_PATH * filepath);
extern void * uefi_alloc_and_read_file(const char * filename, size_t * size_out);

extern int uefi_register_protocol_callback(
	EFI_GUID * guid,
	void (*handler)(void*),
	void * context
);

/* Device driver init functions go here */
extern int uefi_loader_init(void);
extern int uefi_ramdisk_init(void);
extern int uefi_blockdev_init(void);
extern int uefi_nic_init(void);
extern int uefi_tpm_init(void);

#endif

