/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	linker script
	
Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

OUTPUT_FORMAT("elf32-bigarm", "elf32-bigarm", "elf32-bigarm")
OUTPUT_ARCH(arm)
EXTERN(_start)
ENTRY(_start)

__stack_size = 0x3000;
__irqstack_size = 0x1000;
__excstack_size = 0x1000;

MEMORY {
	sram0_0		: ORIGIN = 0xFFFF0000, LENGTH = 0x800
	sram0_1		: ORIGIN = 0x0D410800, LENGTH = 0x7800
	sram1_0		: ORIGIN = 0x0D400000, LENGTH = 0xE000
	sram1_1		: ORIGIN = 0x0D40E000, LENGTH = 0x2000
}

SECTIONS
{
	.init :
	{
		*(.start)
		*(.init)
		*(.sram.text)
		. = ALIGN(4);
	} >sram0_0

	.text :
	{
		*(.text*)
		*(.text.*)
		*(.gnu.warning)
		*(.gnu.linkonce.t*)
		*(.glue_7)
		*(.glue_7t)
		. = ALIGN(4);
	} >sram0_1

	.rodata :
	{
		*(.rodata)
		*all.rodata*(*)
		*(.roda)
		*(.rodata.*)
		*(.gnu.linkonce.r*)
		. = ALIGN(4);
	} >sram1_1

	.data :
	{
		*(.data)
		*(.data.*)
		*(.gnu.linkonce.d*)
		. = ALIGN(4);
	} >sram1_1

	.bss :
	{
		__bss_start = . ;
		*(.dynbss)
		*(.gnu.linkonce.b*)
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		__bss_end = . ;
	} >sram1_0

	.stack :
	{
		. = ALIGN(16);
		__stack_end = .;
		. = . +__stack_size;
		. = ALIGN(16);
		__stack_addr = .;
		__irqstack_end = .;
		. = . +__irqstack_size;
		. = ALIGN(16);
		__irqstack_addr = .;
		__excstack_end = .;
		. = . +__excstack_size;
		. = ALIGN(16);
		__excstack_addr = .;
		. = ALIGN(4);
	} >sram1_0

	.heap :
	{
		__end__ = .;
		__heap_start__ = .;
		__heap_end__ = (ORIGIN(sram1_0) + LENGTH(sram1_0));
		. = __heap_end__;
	} >sram1_0

	/DISCARD/ :
	{
		*(.ARM.exidx*)
		*(.ARM.extab*)
	}

}
