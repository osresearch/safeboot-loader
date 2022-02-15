# uefidev

This module provides Linux device driver wrappers for several of the
UEFI vendor firmwre provided interfaces.  Normally this is not possible
since Linux calls `gBS->ExitBootServices()`, which tears down most
of the UEFI device drivers, and because Linux does not have a memory
mapping for the UEFI linear memory.

This module depends on a specially modified Linux kernel with the
`efi=noexitbootservices` option to leave the UEFI Boot Services
available, as well as booting with Linux with the `exactmap` option
to ensure that the Linux kernel doesn't accidentally modify any of
the UEFI data structures.

The technique of writing directly to CR3 is a total expedient hack
and definitely not a production ready sort of way to restore the
memory map.

## uefiblockdev

This submodule provides an interface to the vendor firmware's registered
`EFI_BLOCK_IO_PROTOCOL` handlers, which allows Linux to use them
as if they were normal block devices.  UEFI tends to create a block
device for the entire disk and then separate ones for each partitions.
You can also have Linux detect the partitions by using `losetup` on
the whole disk device:

```
losetup -f -P /dev/uefi0
mount /dev/loop0p2 /boot
```

## Building

```
make KDIR=$HOME/safeboot/build/linux-5.4.117
```

## Kernel command line

```
efi=noexitbootservices,debug memmap=exactmap,32K@0G,512M@1G noefi acpi=off pci=noacpi
```

