#include "sched.h"

.globl cpu_switch_to
cpu_switch_to:
	mov	x10, #THREAD_CPU_CONTEXT
	add	x8, x0, x10
	mov	x9, sp
	stp	x19, x20, [x8], #16		// store callee-saved registers
	stp	x21, x22, [x8], #16
	stp	x23, x24, [x8], #16
	stp	x25, x26, [x8], #16
	stp	x27, x28, [x8], #16
	stp	x29, x9, [x8], #16
	str	x30, [x8], #16
	add	x8, x1, x10
	ldp	x19, x20, [x8], #16		// restore callee-saved registers
	ldp	x21, x22, [x8], #16
	ldp	x23, x24, [x8], #16
	ldp	x25, x26, [x8], #16
	ldp	x27, x28, [x8], #16
	ldp	x29, x9, [x8], #16
    ldr	x30, [x8], #16
	mov	sp, x9

    mov x10, #THREAD_FPSIMD_CONTEXT
    add x8, x0, x10
    stp q8, q9, [x8], #32       // stroe FP/SIMD registers
    stp q10, q11, [x8], #32
    stp q12, q13, [x8], #32
    stp q14, q15, [x8], #32
    mrs x11, fpsr
    mrs x12, fpcr
    stp x11, x12, [x8]

    add x8, x1, x10
    ldp q8, q9, [x8], #32       // stroe FP/SIMD registers
    ldp q10, q11, [x8], #32
    ldp q12, q13, [x8], #32
    ldp q14, q15, [x8], #32
    ldp x11, x12, [x8]
    msr fpsr, x11
    msr fpcr, x12

	ret
