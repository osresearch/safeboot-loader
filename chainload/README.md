# Chainload from Linux boot loader to another UEFI application

This assumes that the kernel has been booted in UEFI boot loader mode
with `exactmap` and `noexitbootservices`, so that it is possible to
resume the UEFI execution.
