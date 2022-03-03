O ?= ./build
TPM_DIR ?= $O/tpm-state

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
_OVMFSTATE := $O/OVMF_VARS.fd
_BOOT := ,tftp=.,bootfile=$O/bootx64.efi
_NET  := -netdev user,id=eth0$(_BOOT) -device e1000,netdev=eth0
_WIN  := -cdrom win10.iso -drive format=raw,file=win10.img,index=0,media=disk

# see https://qemu-project.gitlab.io/qemu/specs/tpm.html
_TPM  := \
	-chardev socket,id=chrtpm,path="$(TPM_DIR)/swtpm-sock.ctrl" \
	-tpmdev emulator,id=tpm0,chardev=chrtpm \
	-device tpm-tis,tpmdev=tpm0 \
	$(if $(TPMSTATE), -incoming "exec:cat < testvm.bin" ) \


# Extra arguments and optional changes for QEMU invocation
_QEMU_ARGS := \
	$(_NET) \
	$(_WIN) \
	$(if $(TPM),$(_TPM)) \
	$(if $(NOGRAPHIC), -nographic -monitor /dev/null ) \

# Copy the clean net-boot OVMF state to the build directory
$(_OVMFSTATE): config/OVMF_VARS.fd
	cp $< $@

qemu: $O/bootx64.efi $(_OVMFSTATE)
	$(if $(TPM), ./tpm.sh "$(TPM_DIR)" )
	qemu-system-x86_64 \
		-M q35,accel=kvm \
		-cpu host \
		-m 2G \
		-serial stdio \
		-drive if=pflash,format=raw,readonly=on,file=$(_OVMF) \
		-drive if=pflash,format=raw,file=$(_OVMFSTATE) \
		$(_QEMU_ARGS) \


FORCE:
