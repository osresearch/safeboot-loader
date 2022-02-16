#
# Build a safeboot initrd.cpio
#
O ?= ../build
WORKDIR := $O/initrd

all: $O/initrd.cpio.xz

$O:
	mkdir -p $O

$(WORKDIR)/gitstatus: files.txt
	rm -rf "$(dir $@)"
	mkdir -p "$(dir $@)"
	./populate "$(dir $@)" "$<"
	git status -s > "$@"

-include $O/initrd.deps

$O/initrd.cpio: $(WORKDIR)/gitstatus
	( cd $(dir $<) ; \
		find . -print0 \
		| cpio \
			-0 \
			-o \
			-H newc \
	) \
	| ./cpio-clean \
		../kernel/dev.cpio \
		- \
	> $@
	sha256sum $@

$O/initrd.cpio.xz: $O/initrd.cpio
	xz \
		--check=crc32 \
		--lzma2=dict=256KiB \
		--threads=0 \
		< "$<" \
	| dd bs=512 conv=sync status=none \
	> "$@.tmp"
	@if ! cmp --quiet "$@.tmp" "$@" ; then \
		mv "$@.tmp" "$@" ; \
	else \
		echo "$@: unchanged" ; \
		rm "$@.tmp" ; \
	fi
	sha256sum $@

$O/initrd.cpio.bz: $O/initrd.cpio
	bzip2 -z \
		< "$<" \
	| dd bs=512 conv=sync status=none \
	> "$@.tmp"
	@if ! cmp --quiet "$@.tmp" "$@" ; then \
		mv "$@.tmp" "$@" ; \
	else \
		echo "$@: unchanged" ; \
		rm "$@.tmp" ; \
	fi
	sha256sum $@

