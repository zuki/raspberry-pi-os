# 演習2.3.2

## `mgeneral-regs-only`をとった場合、

`tfp_printf`と`tfp_sprintf`で`q0-q7`がスタックに積まれている。たとえば、`tfp_printf`は次の通り。

```
0000000000080ae0 <tfp_printf>:
   80ae0:	a9ae7bfd 	stp	x29, x30, [sp, #-288]!
   80ae4:	910003fd 	mov	x29, sp
   80ae8:	f9001fa0 	str	x0, [x29, #56]
   80aec:	f90077a1 	str	x1, [x29, #232]
   80af0:	f9007ba2 	str	x2, [x29, #240]
   80af4:	f9007fa3 	str	x3, [x29, #248]
   80af8:	f90083a4 	str	x4, [x29, #256]
   80afc:	f90087a5 	str	x5, [x29, #264]
   80b00:	f9008ba6 	str	x6, [x29, #272]
   80b04:	f9008fa7 	str	x7, [x29, #280]
   80b08:	3d801ba0 	str	q0, [x29, #96]
   80b0c:	3d801fa1 	str	q1, [x29, #112]
   80b10:	3d8023a2 	str	q2, [x29, #128]
   80b14:	3d8027a3 	str	q3, [x29, #144]
   80b18:	3d802ba4 	str	q4, [x29, #160]
   80b1c:	3d802fa5 	str	q5, [x29, #176]
   80b20:	3d8033a6 	str	q6, [x29, #192]
   80b24:	3d8037a7 	str	q7, [x29, #208]
   80b28:	910483a0 	add	x0, x29, #0x120
   80b2c:	f90023a0 	str	x0, [x29, #64]
   80b30:	910483a0 	add	x0, x29, #0x120
   80b34:	f90027a0 	str	x0, [x29, #72]
   80b38:	910383a0 	add	x0, x29, #0xe0
   80b3c:	f9002ba0 	str	x0, [x29, #80]
   80b40:	128006e0 	mov	w0, #0xffffffc8            	// #-56
   80b44:	b9005ba0 	str	w0, [x29, #88]
   80b48:	12800fe0 	mov	w0, #0xffffff80            	// #-128
   80b4c:	b9005fa0 	str	w0, [x29, #92]
   80b50:	90000000 	adrp	x0, 80000 <_start>
   80b54:	9133c000 	add	x0, x0, #0xcf0
   80b58:	f9400004 	ldr	x4, [x0]
   80b5c:	90000000 	adrp	x0, 80000 <_start>
   80b60:	9133a000 	add	x0, x0, #0xce8
   80b64:	f9400005 	ldr	x5, [x0]
   80b68:	910043a2 	add	x2, x29, #0x10
   80b6c:	910103a3 	add	x3, x29, #0x40
   80b70:	a9400460 	ldp	x0, x1, [x3]
   80b74:	a9000440 	stp	x0, x1, [x2]
   80b78:	a9410460 	ldp	x0, x1, [x3, #16]
   80b7c:	a9010440 	stp	x0, x1, [x2, #16]
   80b80:	910043a0 	add	x0, x29, #0x10
   80b84:	aa0003e3 	mov	x3, x0
   80b88:	f9401fa2 	ldr	x2, [x29, #56]
   80b8c:	aa0503e1 	mov	x1, x5
   80b90:	aa0403e0 	mov	x0, x4
   80b94:	97fffeb3 	bl	80660 <tfp_format>
   80b98:	d503201f 	nop
   80b9c:	a8d27bfd 	ldp	x29, x30, [sp], #288
   80ba0:	d65f03c0 	ret
```
