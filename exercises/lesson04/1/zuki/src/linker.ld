SECTIONS
{
	. = 0x80000;
	text_start = .;
	.text.boot : { *(.text.boot) }
	.text : { *(.text) }
	data_start = .;
	.rodata : { *(.rodata) }
	.data : { *(.data) }
	. = ALIGN(0x8);
	bss_begin = .;
	.bss : { *(.bss*) }
	bss_end = .;
}
