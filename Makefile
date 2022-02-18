all: build/vmlinuz build/initrd.cpio.xz

build/vmlinuz: FORCE
	$(MAKE) -C kernel
module/uefidev.ko: build/vmlinuz FORCE
	$(MAKE) -C $(dir $@)
build/initrd.cpio.xz: FORCE
	$(MAKE) -C initrd

FORCE:
