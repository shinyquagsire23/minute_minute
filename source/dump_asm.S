/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2023          Max Thomas <mtinc2@gmail.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

.arm

.section .text

.globl boot1_prshhax_payload
.globl boot1_prshhax_payload_end


.align 16
boot1_prshhax_payload:
    PUSH {LR}
    LDR R1, =0x10009000
    ldr r12, =0x4F545044 /* OTPD */
    str r12, [r1]
    add r1, r1, #0x4
    LDR R12, =0xD8001EC
    LDR LR, =0xD8001F0
    MOV R2, #0
    
loc_10100AD0:
    MOV     R3, #0

loc_10100AD4:
    ORR     R0, R2, R3
    ORR     R0, R0, #0x80000000
    STR     R0, [R12]
    LDR     R0, [LR]
    STR     R0, [R1,R3,LSL#2]
    ADD     R3, R3, #1
    CMP     R3, #0x20
    BNE     loc_10100AD4
    ADD     R2, R2, #0x100
    CMP     R2, #0x800
    ADD     R1, R1, #0x80
    BNE     loc_10100AD0
    
    /* the Pico will take us back */
    b .

.pool
boot1_prshhax_payload_end:
nop

