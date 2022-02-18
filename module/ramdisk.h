#ifndef _uefi_ramdisk_h_
#define _uefi_ramdisk_h_

typedef struct _EFI_RAM_DISK_PROTOCOL  EFI_RAM_DISK_PROTOCOL;

typedef
EFI_STATUS
(EFIAPI *EFI_RAM_DISK_REGISTER_RAMDISK) (
  IN UINT64                       RamDiskBase,
  IN UINT64                       RamDiskSize,
  IN EFI_GUID                     *RamDiskType,
  IN EFI_DEVICE_PATH              *ParentDevicePath     OPTIONAL,
  OUT EFI_DEVICE_PATH_PROTOCOL    **DevicePath
  );

/**
  Unregister a RAM disk specified by DevicePath.

  @param[in] DevicePath      A pointer to the device path that describes a RAM
                             Disk device.

  @retval EFI_SUCCESS             The RAM disk is unregistered successfully.
  @retval EFI_INVALID_PARAMETER   DevicePath is NULL.
  @retval EFI_UNSUPPORTED         The device specified by DevicePath is not a
                                  valid ramdisk device path and not supported
                                  by the driver.
  @retval EFI_NOT_FOUND           The RAM disk pointed by DevicePath doesn't
                                  exist.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_RAM_DISK_UNREGISTER_RAMDISK) (
  IN  EFI_DEVICE_PATH_PROTOCOL    *DevicePath
  );

///
/// RAM Disk Protocol structure.
///
struct _EFI_RAM_DISK_PROTOCOL {
  EFI_RAM_DISK_REGISTER_RAMDISK        Register;
  EFI_RAM_DISK_UNREGISTER_RAMDISK      Unregister;
};

///
/// RAM Disk Protocol GUID variable.
///
//EFI_GUID gEfiRamDiskProtocolGuid = { 0xab38a0df, 0x6873, 0x44a9, { 0x87, 0xe6, 0xd4, 0xeb, 0x56, 0x14, 0x84, 0x49 }};
#define EFI_RAMDISK_PROTOCOL_GUID EFI_GUID(0xab38a0df, 0x6873, 0x44a9,  0x87, 0xe6, 0xd4, 0xeb, 0x56, 0x14, 0x84, 0x49 )
#define EFI_VIRTUAL_DISK_GUID EFI_GUID( 0x77AB535A, 0x45FC, 0x624B, 0x55, 0x60, 0xF7, 0xB2, 0x81, 0xD1, 0xF9, 0x6E )

///
/// Media ram disk device path.
///
#define MEDIA_RAM_DISK_DP         0x09

///
/// Used to describe the ram disk device path.
///
typedef struct {
  EFI_DEVICE_PATH_PROTOCOL        Header;
  ///
  /// Starting Memory Address.
  ///
  UINT32                          StartingAddr[2];
  ///
  /// Ending Memory Address.
  ///
  UINT32                          EndingAddr[2];
  ///
  /// GUID that defines the type of the RAM Disk.
  ///
  EFI_GUID                        TypeGuid;
  ///
  /// RAM Diskinstance number, if supported. The default value is zero.
  ///
  UINT16                          Instance;
} MEDIA_RAM_DISK_DEVICE_PATH;


#endif
