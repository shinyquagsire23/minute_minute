@ Wii U IOS ELF loader patch loader
@ v1.0 by WulfyStylez/SALT

@ patch format:
@ 0x0: 'SALTPTCH' magic
@ 0x8: version (0x1)
@ 0xC and on: u32 addr, u32 len, u8 data[len] (0x4-aligned len)
@ 0xDEADCAFE as addr == EOF
    .arm
	.text
	.global	elfldr_start
    .global elfldr_end

.align 16
elfldr_start:
    mov r4, #0x100  @ patches live here!
    add r4, #0x20    @ skip header
    
patch_it_up:
    ldrb r0, [r4,#3] @ get patch type from lobyte
    add r4, #4
    cmp r0, #0
    beq do_copy
    cmp r0, #1
    beq do_memset
    cmp r0, #0xFF
    beq jump_out
    
here:
    b here

do_copy:
    ldr r0, [r4], #4 @ get addr
    ldr r1, [r4], #4 @ get len
    add r1, r0  @ end addr
copy_loop:
    ldr r2, [r4], #4
    str r2, [r0], #4
    cmp r0, r1
    bne copy_loop
    b patch_it_up
    
do_memset:
    ldr r0, [r4], #4 @ get addr
    ldr r1, [r4], #4 @ get len
    add r1, r0  @ end addr
    mov r2, #0
memset_loop:
    str r2, [r0], #4
    cmp r0, r1
    bne memset_loop
    b patch_it_up

jump_out:
    ldr r4, =0x27FFFFF8 @ Check if we have a base plugin jumpout
    ldr r2, =0x504C5547 @ PLUG
    ldr r0, [r4]
    ldr r4, [r4, #0x4]
    cmp r0, r2
    beq plug_jumpout

    ldr r4, =0xFFFF0000
plug_jumpout:
    bx r4
.pool

elfldr_end:
.word 0x0