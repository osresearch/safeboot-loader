all: build/vmlinuz module/uefidev.ko build/initrd.cpio.xz

build/vmlinuz: FORCE
	$(MAKE) -C kernel
module/uefidev.ko: build/vmlinuz FORCE
	$(MAKE) -C $(dir $@)
build/initrd.cpio.xz: FORCE
	$(MAKE) -C initrd

FORCE:
