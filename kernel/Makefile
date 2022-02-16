# enable dependency tracking during builds
OLD_SHELL := $(SHELL)
SHELL = $(if $@,$(warning $(OLD_SHELL date "+%Y-%m-%d %H:%M:%S") Building $@$(if $?, ($? newer))))$(OLD_SHELL)

GIT_DIRTY := $(shell if git status -s >/dev/null ; then echo dirty ; else echo clean ; fi)
GIT_HASH  := $(shell git rev-parse HEAD)
TOP := $(shell pwd)

O ?= $(TOP)/../build

all: $O/vmlinuz

$O:
	mkdir -p "$@"

#
# Linux kernel for the PXE boot image
#
LINUX		:= linux-5.4.117
#LINUX		:= linux-5.10.100
#LINUX		:= linux-5.15.23
#LINUX		:= linux-5.7.2
LINUX_TAR	:= $(LINUX).tar.xz
LINUX_SIG	:= $(LINUX).tar.sign
LINUX_URL	:= https://cdn.kernel.org/pub/linux/kernel/v5.x/$(LINUX_TAR)

$(LINUX_TAR):
	[ -r $@.tmp ] || wget -O $@.tmp $(LINUX_URL)
	[ -r $(LINUX_SIG) ] || wget -nc $(dir $(LINUX_URL))/$(LINUX_SIG)
	#unxz -cd < $@.tmp | gpg2 --verify $(LINUX_SIG) -
	mv $@.tmp $@

$(LINUX): $(LINUX)/.patched
$(LINUX)/.extract: $(LINUX_TAR)
	tar xf $(LINUX_TAR)
	touch $@
$(LINUX)/.patched: $(TOP)/../patches/$(LINUX).patch $(LINUX)/.extract
	patch -p1 < $<
	touch $@

$O/vmlinuz: $O/$(LINUX)/.config
	$(MAKE) \
		KBUILD_HOST=safeboot \
		KBUILD_BUILD_USER=builder \
		KBUILD_BUILD_TIMESTAMP="$(GIT_HASH)" \
		KBUILD_BUILD_VERSION="$(GIT_DIRTY)" \
		-C $O/$(LINUX)
	cp $O/$(LINUX)/arch/x86/boot/bzImage $@

$O/$(LINUX)/.config: $(TOP)/../patches/$(LINUX).config $(LINUX)/.patched
	mkdir -p $(dir $@)
	cp $< $@
	$(MAKE) \
		-C $(LINUX) \
		O=$(dir $@) \
		olddefconfig

linux-menuconfig: $O/$(LINUX)/.config
	$(MAKE) -j1 -C $(dir $<) menuconfig savedefconfig
	cp $(dir $<)defconfig initramfs/linux.config

