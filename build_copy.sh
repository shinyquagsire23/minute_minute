#!/bin/zsh

cd elfloader
gmake clean
gmake
cd ..

rm -f fw.img boot1.img elfloader/elfloader.bin

gmake -f Makefile.boot1 && gmake && cat boot1.img fw.img > sdcard.img && dd if=sdcard.img of=../Wii-U-Firmware-Emulator/files/sdcard.bin conv=notrunc && cp boot1.img ../Wii-U-Firmware-Emulator/files/boot1.bin && ./build_copy_to_emu.sh