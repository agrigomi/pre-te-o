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

	. = 0x14000;
	.bstext		: { *(.bstext) }
	.bsdata		: { *(.bsdata) }


	.text		: { *(.text) }
	.text32		: { *(.text32) }

	. = ALIGN(16);
	.rodata		: { *(.rodata*) }

	. = ALIGN(16);
	.data		: { *(.data*) }

	. = ALIGN(16);
	.bss		:
	{
		__bss_start = .;
		*(.bss)
		__bss_end = .;
	}
	
	.ctors          :
  	{
	    	/* gcc uses crtbegin.o to find the start of
       		the constructors, so we make sure it is
       		first.  Because this is a wildcard, it
       		doesn't matter if the user does not
       		actually link against crtbegin.o; the
       		linker won't look for a file to match a
       		wildcard.  The wildcard also means that it
       		doesn't matter which directory crtbegin.o
       		is in.  */
    		KEEP (*crtbegin.o(.ctors))
    		KEEP (*crtbegin?.o(.ctors))
    		/* We don't want to include the .ctor section from
       		the crtend.o file until after the sorted ctors.
       		The .ctor section from the crtend file contains the
       		end of ctors marker and it must be last */
    		KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .ctors))
    		KEEP (*(SORT(.ctors.*)))
    		KEEP (*(.ctors))
  	}

       .dtors          :
       {
                KEEP (*crtbegin*.o(.dtors))
                KEEP (*(EXCLUDE_FILE (*crtend*.o ) .dtors))
                KEEP (*(SORT(.dtors.*)))
                KEEP (*(.dtors))
       }

        /DISCARD/ : {
                *(.stab*)
                *(.comment)
                *(.note)
        }
        
	_GLOBAL_OFFSET_TABLE_ = .;
        all : {*(.text, .text32, .rodata, .rdata, .data, .bss, .bstext, .bsdata, .ctors) }
	
	. = ALIGN(16);
	_end = .;
}
