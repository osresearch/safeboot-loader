all: bootx64.efi

bootx64.efi: loader/loader.efi build/vmlinuz build/initrd.cpio.xz
	./loader/unify-kernel $@ \
		linux=build/vmlinuz \
		initrd=build/initrd.cpio.xz \
		cmdline=patches/cmdline-5.4.117.txt

build/vmlinuz: FORCE
	$(MAKE) -C kernel
module/uefidev.ko: build/vmlinuz FORCE
	$(MAKE) -C $(dir $@)
loader/loader.efi: FORCE
	$(MAKE) -C $(dir $@)
build/chainload: FORCE
	$(MAKE) -C chainload
build/initrd.cpio.xz: build/chainload FORCE
	$(MAKE) -C initrd

FORCE:
