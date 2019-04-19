OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
/* OUTPUT_ARCH(x64) */
ENTRY(_start)

SECTIONS
{

	. = 0x00;
	.bstext		: { *(.bstext) }
	.bsdata		: { *(.bsdata) }

	. = ALIGN(16);
	.text		: { *(.text) }
	.group		: { *(.group) }

	. = ALIGN(16);
	.rodata		: { *(.rodata*) }

	. = ALIGN(16);
	.data		: { *(.data*) }

	.bss		: {
		__bss_start = .;
		*(.bss)
		__bss_end = .;
	}
	
	.ctors          : {
		start_ctors = .; _start_ctors = .; __start_ctors = .;
		*(.init_array);
		*(.ctors);
		end_ctors = .; _end_ctors = .; __end_ctors = .; 
    		/*KEEP (*(SORT(.ctors.*)))
    		KEEP (*(.ctors))*/
  	}

       .dtors          : {
		start_dtors = .; _start_dtors = .; __start_dtors = .;
		*(.dtors);
		*(.fini_array);
		end_dtors = .; _end_dtors = .; __end_dtors = .;
                /*KEEP (*(SORT(.dtors.*)))
                KEEP (*(.dtors))*/
		
		
       }
       .rela.dyn	:{*(.rela.dyn) }

        /DISCARD/ : {
                *(.stab*)
                *(.comment)
                *(.note)
        }
        
	_GLOBAL_OFFSET_TABLE_ = .;
        all : {*(.text, .rodata, .rdata, .data, .bss, .bstext, .bsdata, .ctors, .init_array, .dtors, .fini_array, .rela.dyn) }
	
	. = ALIGN(16);
	_end = .;
}


