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


## Block Devices

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

Todo:

* [ ] Benchmark the performance
* [ ] Test with the ramdisk module

## Ramdisk

The Linux boot loader can pass data to the next stage via a UEFI
ramdisk, which can be created by echo'ing the disk image file name into
`/sys/firmware/efi/ramdisk`.

* [ ] Need to trim newlines

## Loader

New UEFI modules can be loaded by echo'ing the file name into
`/sys/firmware/efi/loader`.  This should measure them into
the TPM and eventlog.  It can also be used to chain load
the next stage.

## Network Interfaces

This submodule create an ethernet interface for each of the
vendor firmware's registered `EFI_SIMPLE_NETWORK_PROTOCOL` devices.
The Linux `skb` transmit functions put packets directly on the wire,
and there is a periodic timer that polls at 100 Hz for up to ten packets.
It's not going to be a fast interface, but it will hopefully be enough
to perform attestations or other boot time activities.

Todo:

* [ ] Make polling timer a parameter
* [ ] Interface with the UEFI event system?


## TPM Devices

Because ACPI and PCI are disabled, the TPM is not currently visible
to Linux via the normal channels.  Instead this submodule will
query the `EFI_TCG2_PROTOCOL` objects and create TPM character
devices for each of them.  While the UEFI object has methods for
high-level things like "Extend a PCR and create an event log entry",
this module uses the `SubmitCommand` method to send the raw commands
that the Linux driver generates.  It buffers the response and returns
it immediately; there is no overlapping of commands or multi-threading
allowed.

Todo:

* [X] Figure out how to expose the TPM.
* [X] Figure out how to export the TPM event log
* [ ] Change the event log to be "live" rather than a copy


## Building

The `Makefile` will download and patch a 5.4.117 kernel with the
`noexitbootservices` option and to add the `uefidev` kernel module
as an in-tree build option.  It will then apply a minimal config that
has no PCI drivers and uses the EFI framebuffer for video.

```
make -j32
```

This will produce after a while `build/vmlinuz` that is ready to
be unified with an appropriate `initrd` and then booted on an EFI
system (over PXE or as a boot menu item).

Todo:

* [X] Wrap kernel building in the `Makefile`
* [X] `initrd.cpio` building
* [ ] LinuxKit or buildroot integration?

## Kernel command line

```
efi=noexitbootservices,debug memmap=exactmap,32K@0G,512M@1G noefi acpi=off
```

