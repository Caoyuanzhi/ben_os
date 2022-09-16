#include "mm.h"
#include "sysregs.h"

.section  .rodata
.align 3
.globl el_string1
el_string1:
	.string "Booting at EL"

.section ".text.boot"
.globl _start
_start:
	mrs	x0, mpidr_el1		
	and	x0, x0,#0xFF		// Check processor id
	cbz	x0, master		// Hang for all non-primary CPU
	b	proc_hang

proc_hang: 
	b 	proc_hang

master:
        /* init uart and print the string*/
	bl __init_uart

	mrs x5, CurrentEL
	cmp x5, #CurrentEL_EL3
	b.eq el3_entry
	b el2_entry


el3_entry:
	eret

el2_entry:
	bl print_el
	/*	HCR_EL2: 
		bit[31] RW	: exectution state for lower exception level aarch64 0
	*/
	ldr x0, =HCR_HOST_NVHE_FLAGS
	msr hcr_el2,x0

	/*	SCTLR_EL1: 
		bit[0] 	M 	: MMU disable 0
		bit[24]	EE	: EL0 little-end 0
		bit[25]	EE	: EL1 little-end 0
	*/
	ldr x0, =SCTLR_VALUE_MMU_DISABLED
	msr sctlr_el1, x0

	/* 	SPSR_EL2:saved program status register 
		bit[3:0] M	: exception level and selected Stack Pointer 101	
		bit[9:6] DAIF	: 1111 	
	*/
	ldr x0, =SPSR_EL1
	msr spsr_el2, x0

	adr x0, el1_entry
	msr elr_el2, x0

	/* set the VBAR_EL2 */
	ldr     x5, =exception_vector_table
	msr     vbar_el2, x5
	isb

	eret

el1_entry:
	bl print_el

	/* set the VBAR_EL1 */
	ldr     x5, =exception_vector_table
	msr     vbar_el1, x5
	isb

	adr	x0, _bss
	adr	x1, _ebss
	sub	x1, x1, x0
	bl 	memzero


	mov	sp, #LOW_MEMORY 
	bl	kernel_main
	b 	proc_hang		// should never come here

print_el:
	mov x10, x30

	/*
	   print EL
	 */
	adrp x0, el_string1
	add x0, x0, :lo12:el_string1
	bl put_string_uart

	mrs x5, CurrentEL
	/* get the currentEL value */
	lsr x2, x5, #2
	mov x0, #48
	add x0, x0, x2
	bl put_uart
	/* print the new line tab */
	mov x0, #10
	bl put_uart

	mov x30, x10
	ret






