#include "mm.h"

.section ".text.boot"

.globl _start
_start:
	mrs	x0, mpidr_el1
	and	x0, x0,#0xFF		// Check processor id
	cbz	x0, master		// Hang for all non-primary CPU
	b	set_sp

master:
	adr	x0, bss_begin
	adr	x1, bss_end
	sub	x1, x1, x0
	bl 	memzero

set_sp:
    mrs	x0, mpidr_el1
	and	x0, x0,#0xFF        // x0 = cpuid            | LOW_MEMORY | __ sp for cpu0
    mov x1, #0x2000         // 2 page                |  2page=8KB | __ for cpu1
    mul x1, x1, x0          // x1 = 2 page * cpuid   |  2page=8KB | __ sp for cpu2
    add x1, x1, #LOW_MEMORY // x1 = x1 + LOW_MEMORY  |  2page=8KB | __ sp for cpu3
	mov	sp, x1
	bl	kernel_main

proc_hang:
	b 	proc_hang       // should never come here
