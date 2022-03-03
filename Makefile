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

# Arguments/parameters for QEMU, allowing for customization through make vars
_OVMF := /usr/share/OVMF/OVMF_CODE.fd
_OVMFSTATE := config/OVMF_VARS.fd
_BOOT := ,tftp=.,bootfile=$O/bootx64.efi
_NET  := -netdev user,id=eth0$(_BOOT) -device e1000,netdev=eth0
_WIN  := -cdrom win10.iso -drive format=raw,file=win10.img,index=0,media=disk

# see https://qemu-project.gitlab.io/qemu/specs/tpm.html
_TPMSTATE := -incoming "exec:cat < testvm.bin"
_TPM  := -chardev socket,id=chrtpm,path=/tmp/mytpm1/swtpm-sock \
	  -tpmdev emulator,id=tpm0,chardev=chrtpm -device tpm-tis,tpmdev=tpm0
ifeq ($(TPMSTATE),1)
	_TPM += $(_TPMSTATE)
endif

# Extra arguments and optional changes for QEMU invocation
_QEMUARGS := $(_NET) $(_WIN)
ifeq ($(TPM),1)
	_QEMUARGS += $(_TPM)
endif
_NOGRAPHIC := -nographic -monitor /dev/null
ifeq ($(NOGRAPHIC),1)
 	_QEMUARGS += $(_NOGRAPHIC)
endif

qemu: $O/bootx64.efi
	qemu-system-x86_64 -M q35,accel=kvm -cpu host -m 2G -serial stdio \
		-drive if=pflash,format=raw,readonly=on,file=$(_OVMF) \
		-drive if=pflash,format=raw,file=$(_OVMFSTATE) \
		$(_QEMUARGS)

FORCE:
