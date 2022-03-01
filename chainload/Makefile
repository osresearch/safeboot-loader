O ?= ../build/chainload

all: $O/chainload $O/loader.efi $O/unify-kernel


$O:
	mkdir -p $O

CFLAGS = \
	-O3 \
	-W \
	-Wall \
	-g \
	-m64 \
	-MMD \
	-MF $O/.$(notdir $@).d \

EFI_CFLAGS = \
	$(CFLAGS) \
	-D__efi__ \
	-DGNU_EFI_USE_MS_ABI \
	-fshort-wchar \
	-mno-red-zone \
	-fno-stack-protector \
	-I /usr/include/efi \
	-I /usr/include/efi/x86_64 \
	-fpic \
	-static \
	-nostdlib \
	$(EXTRA_CFLAGS)

LDFLAGS = \
	-fpic \
	-nostdlib \
	-znocombreloc \

LDFLAGS.dynamic = \
	-shared \
	-Bsymbolic \


$O/unify-kernel: unify-kernel | $O
	cp $< $@

$O/chainload: chainload.c $O/chainload.bin | $O
	$(CC) $(CFLAGS) \
		-DCHAINLOAD_BIN="\"$O/chainload.bin\"" \
		-o $@ \
		$<

# This is not really an EFI application, so it overrides the entry point
$O/chainload.rawefi: $O/efi-entry.o $O/efi-chainload.o | $O
$O/chainload.rawefi: EXTRA_CFLAGS=-Wl,-eentry

# This is really an EFI application, so it uses the full libraries
$O/loader.exe: $O/loader.o $O/pe.o

$O/%.o: %.c | $O
	$(CC) $(EFI_CFLAGS) \
		-c \
		-o $@ \
		$< \

$O/%.rawefi:
	$(CC) $(EFI_CFLAGS) \
		-o $@ \
		$^ \

$O/%.bin: $O/%.rawefi
	objcopy \
		-O binary \
		-j .text \
		-j .rodata \
		$< \
		$@

$O/%.exe:
	ld \
		$(LDFLAGS) \
		$(LDFLAGS.dynamic) \
		-o $@ \
		-T /usr/lib/elf_x86_64_efi.lds \
		/usr/lib/crt0-efi-x86_64.o \
		$^ \
		-L /usr/lib \
		-l:libgnuefi.a \
		-l:libefi.a \

$O/%.efi: $O/%.exe
	objcopy \
		-j .text \
		-j .sdata \
		-j .data \
		-j .dynamic \
		-j .dynsym \
		-j .rel \
		-j .rela \
		-j .reloc \
		--target efi-app-x86_64 \
		$^ \
		$@

clean: FORCE
	$(RM) $O/*.efi $O/*.bin $O/*.o $O/*.exe $O/.*.d $O/*.rawefi
FORCE:

-include $O/.*.d
