ifneq ($(KERNELRELEASE),)
# kbuild part of makefile
obj-m  := uefidev.o
uefidev-y += main.o
uefidev-y += blockdev.o
uefidev-y += nic.o
uefidev-y += efiwrapper.o

ccflags-y += -std=gnu99
ccflags-y += -DGNU_EFI_USE_MS_ABI
ccflags-y += -I$(src)/include
#ccflags-y += -I/usr/include/efi
#ccflags-y += -I/usr/include/efi/x86_64

else
# normal makefile
KDIR ?= /lib/modules/`uname -r`/build

default:
	$(MAKE) -C $(KDIR) M=$$PWD

# Module specific targets
genbin:
	#echo "X" > 8123_bin.o_shipped

clean:
	$(RM) \
		Module.symvers modules.order \
		*.o *.a *.ko *.mod *.mod.* .*.cmd 

endif
