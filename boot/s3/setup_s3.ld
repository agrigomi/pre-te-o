/*
 * setup.ld
 *
 * Linker script for the i386 setup code
 */
OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS
{

	. = 0x7e00;
	.bstext		: { *(.bstext) }
	.bsdata		: { *(.bsdata) }


	.text		: { *(.text) }
	.text32		: { *(.text32) }

	. = ALIGN(16);
	.rodata		: { *(.rodata*) }
	.bss		: { *(.rodata*) }
	.data		: { *(.data*) }
	
	/*
	.videocards	: {
		video_cards = .;
		*(.videocards)
		video_cards_end = .;
	}
	
	
	. = ALIGN(16);
	.data		: { *(.data*) }
	*/
	
	/DISCARD/ : { *(.note*) }

}
