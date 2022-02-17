ifneq ($(KERNELRELEASE),)
# kbuild part of makefile
obj-$(CONFIG_UEFIDEV)  := uefidev.o
uefidev-objs += main.o
uefidev-objs += efiwrapper.o
uefidev-$(CONFIG_UEFINET) += efinet.o
uefidev-$(CONFIG_UEFIBLOCK) += blockio.o
uefidev-$(CONFIG_UEFITPM) += tpm.o

ccflags-y += -std=gnu99
#ccflags-y += -DGNU_EFI_USE_MS_ABI
#ccflags-y += -I$(src)/include
#ccflags-y += -I/usr/include/efi
#ccflags-y += -I/usr/include/efi/x86_64

else
# normal makefile
#KDIR ?= /lib/modules/`uname -r`/build
KDIR ?= ../build/linux-5.4.117

default:
	$(MAKE) \
		-C $(KDIR) \
		M=$$PWD \
		CONFIG_UEFIDEV=m \

# Module specific targets
genbin:
	#echo "X" > 8123_bin.o_shipped

clean:
	$(RM) \
		Module.symvers modules.order \
		*.o *.a *.ko *.mod *.mod.* .*.cmd 

endif
