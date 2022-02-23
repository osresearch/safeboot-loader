# Chainload from Linux to Windows

This has two important pieces for the Linux-as-a-bootloader to be able
to boot Windows:

* An EFI stub that saves UEFI context and prevents `ExitBootServices` from being called
* The `chainload` tool that will pass control back to UEFI and invoke a new image.

## UEFI Stub for Linux Boot Loader

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

TODO: Have the stub add this parameter based on its allocation.

The memory is allocated at 1 GB so that the first page table entry is
free and all of the UEFI memory can be restored by replacing the first
entry in the table with the stored value from the UEFI CR3.


## Chainload

This tool retrieves the stored context and the image to be invoked, then
wraps the `kexec_load()` system call to shutdown the Linux devices, restore
the UEFI context, and then invoke `gBS->LoadImage()` and `gBS->StartImage()`
on the next boot loader.

If things fail, this *should* return to UEFI and maybe invoke the shell or
the `BootNext` behaviour.
