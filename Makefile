O ?= ./build

all: $O/bootx64.efi | $O

$O:
	mkdir -p $@

clean:
		rm -rf $O/chainload
		rm -rf $O/initrd*

$O/bootx64.efi: $O/chainload/loader.efi $O/vmlinuz $O/initrd.cpio.xz
	$O/chainload/unify-kernel $@ \
		linux=$O/vmlinuz \
		initrd=$O/initrd.cpio.xz \
		cmdline=config/cmdline-5.4.117.txt

$O/vmlinuz: FORCE
	$(MAKE) -C kernel
module/uefidev.ko: build/vmlinuz FORCE
	$(MAKE) -C $(dir $@)
$O/chainload/loader.efi: build/chainload/chainload
$O/chainload/chainload: FORCE
	$(MAKE) -C chainload
$O/initrd.cpio.xz: build/chainload/chainload FORCE
	$(MAKE) -C initrd

qemu: $O/bootx64.efi
	qemu-system-x86_64 \
		-M q35,accel=kvm \
		-m 2G \
		-drive if=pflash,format=raw,readonly,file=/usr/share/OVMF/OVMF_CODE.fd \
		-drive if=pflash,format=raw,file=config/OVMF_VARS.fd \
		-netdev user,id=eth0,tftp=.,bootfile=$O/bootx64.efi \
		-device e1000,netdev=eth0 \
		-serial stdio \
		-cdrom win10.iso \
		-hda win10.img


FORCE:
