# UEFI Stub for Linux Boot Loader

This is a special EFI wrapper for the Linux kernel, initrd and command line
is used that saves the UEFI context into low memory and allocates
a range of contiguous memory for the kernel.  This context
is restored following a `kexec_load()` call in the kernel, which
looks to the UEFI firmware as if the wrapper has returned.

The Linux kernel must not disturb devices or touch memory that
UEFI is using, so it is started with the `memmap` kernel commandline
parameter to restrict it to the allocated chunk.  The kernel also
has to be told not to interact with ACPI, EFI and PCI.

```
memmap=exactmap,32K@0G,512M@1G noefi
```

The 32KiB at 0x0 is necessary for the SMP trampoline, even with
a non-SMP kernel.

