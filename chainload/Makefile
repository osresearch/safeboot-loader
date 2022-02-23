O ?= ../build

all: $O/chainload


$O:
	mkdir -p $O

CFLAGS = \
	-O3 \
	-W \
	-Wall \
	-g \
	-MMD \
	-MF .$(notdir $@).d \
	-I .. \

EFI_CFLAGS = \
	$(CFLAGS) \
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

$O/chainload: chainload.c $O/chainload.bin
	$(CC) $(CFLAGS) \
		-DCHAINLOAD_BIN="\"$O/chainload.bin\"" \
		-o $@ \
		$<

$O/chainload.efi: efi-entry.c efi-chainload.c
	$(CC) $(EFI_CFLAGS) \
		-o $@ \
		$^ \
		-lefi \

$O/%.bin: $O/%.efi
	objcopy \
		-O binary \
		-j .text \
		-j .rodata \
		$< \
		$@

$O/%.efi: $O/%
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
