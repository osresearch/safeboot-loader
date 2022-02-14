# uefiblockdev

This provides an interface to the vendor firmware's registered
`EFI_BLOCK_IO_PROTOCOL` handlers, which allows Linux to use them
as if they were normal block devices.  Normally this is not possible
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

## Kernel command line

```
efi=noexitbootservices,debug memmap=exactmap,32K@0G,512M@1G noefi acpi=off pci=noacpi
```


