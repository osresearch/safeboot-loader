all: loader.efi

loader.exe: loader.o pe.o

CFLAGS = \
	-O3 \
	-W \
	-Wall \
	-m64 \
	-g \
	-MMD \
	-MF .$(notdir $@).d \
	-D__efi__ \
	-DGNU_EFI_USE_MS_ABI \
	-fshort-wchar \
	-mno-red-zone \
	-fno-stack-protector \
	-I /usr/include/efi \
	-I /usr/include/efi/x86_64 \
	-static \
	-fpic \
	-nostdlib \

LDFLAGS = \
	-fpic \
	-nostdlib \
	-znocombreloc \

LDFLAGS.dynamic = \
	-shared \
	-Bsymbolic \

%.exe:
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


%.bin: %
	objcopy \
		-O binary \
		-j .text \
		-j .rodata \
		$< \
		$@

%.efi: %.exe
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


-include .*.d

clean:
	$(RM) .*.d *.o *.a *.so core a.out
