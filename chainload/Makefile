all: chainload.bin

CFLAGS = \
	-O3 \
	-W \
	-Wall \
	-g \
	-MMD \
	-MF .$(notdir $@).d \
	-D__efi__ \
	-DGNU_EFI_USE_MS_ABI \
	-fshort-wchar \
	-mno-red-zone \
	-fno-stack-protector \
	-m64 \
	-I /usr/include/efi \
	-I /usr/include/efi/x86_64 \
	-fpic \
	-static \
	-nostdlib \
	-Wl,-eentry \

chainload.efi: entry.c chainload.c
	$(CC) $(CFLAGS) \
		-o $@ \
		$^ \
		-lefi \

%.bin: %.efi
	objcopy \
		-O binary \
		-j .text \
		-j .rodata \
		$< \
		$@

%.efi: %
	objcopy \
		-j .text \
		-j .sdata \
		-j .data \
		-j .dynamic \
		-j .dynsym \
		-j .rel \
		-j .rela \
		-j .reloc \
		--subsystem efi-bsd \
		--target efi-app-x86_64 \
		$^ \
		$@
