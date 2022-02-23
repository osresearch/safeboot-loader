all: bootx64.efi

bootx64.efi: build/chainload/loader.efi build/vmlinuz build/initrd.cpio.xz
	./build/chainload/unify-kernel $@ \
		linux=build/vmlinuz \
		initrd=build/initrd.cpio.xz \
		cmdline=patches/cmdline-5.4.117.txt

build/vmlinuz: FORCE
	$(MAKE) -C kernel
module/uefidev.ko: build/vmlinuz FORCE
	$(MAKE) -C $(dir $@)
build/chainload/loader.efi:
#build/chainload/chainload: FORCE
	#$(MAKE) -C chainload
build/initrd.cpio.xz: build/chainload/chainload FORCE
	$(MAKE) -C initrd

FORCE:
