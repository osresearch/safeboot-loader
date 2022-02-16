all: build/vmlinuz module/uefidev.ko

build/vmlinuz: patches/linux*.config
	$(MAKE) -C kernel
module/uefidev.ko: build/vmlinuz FORCE
	$(MAKE) -C $(dir $@)

FORCE:
