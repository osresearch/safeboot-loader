#!/bin/bash
CHAINLOAD="$1"
if [ -z "$CHAINLOAD" ]; then
	CHAINLOAD=/boot/EFI/Boot/bootx64.efi
fi

echo -n /bin/test.gpt > /sys/firmware/efi/ramdisk

sleep 1
if [ ! -r /dev/uefi7 ]; then
	echo "no uefi device yet?"
	sleep 5
	if [ ! -r /dev/uefi7 ]; then
		echo "no really, no uefi device"
		exit 1
	fi
fi

mount /dev/uefi7 /boot

if [ ! -r "$CHAINLOAD" ]; then
	echo "$CHAINLOAD: not present?"
	exit 1
fi

chainload -v "$CHAINLOAD"

