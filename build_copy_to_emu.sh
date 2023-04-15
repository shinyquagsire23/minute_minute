#!/bin/zsh

disk=$(hdiutil attach -imagekey diskimage-class=CRawDiskImage -nomount /Users/maxamillion/workspace/Wii-U-Firmware-Emulator/files/sdcard.bin | head -n1 | cut -d " " -f1)
disk_part=$disk
disk_part+="s1"

mount -t msdos $disk_part fat_mnt
cp fw.img fat_mnt/
#cp ios_orig.img fat_mnt/
umount fat_mnt
hdiutil detach $disk