#include "dtype.h"
#include "segment.h"

_str_t err_a20=(_str_t)"Can't enable a20 line !\n";
_str_t err_file=(_str_t)"Can't find file: ";
_str_t err_read=(_str_t)"Error reading file: ";
_str_t err_read_block=(_str_t)"\r\nError reading file block: ";
_str_t err_long_mode=(_str_t)"No long mode support !\r\n";
_str_t err_mem_map=(_str_t)"Can't get memory map by BIOS !\n";
_str_t _boot_config=(_str_t)"/boot/boot.config";
_str_t str_default=(_str_t)"default";
_str_t str_countdown=(_str_t)"countdown";
_str_t str_display=(_str_t)"display";
_str_t str_mask=(_str_t)"=;;;;;";
_str_t str_load_kernel = (_str_t)"\r\nLoading kernel ";
_str_t str_run_kernel_x86 = (_str_t)"Running kernel x86...\r\n";
_str_t str_run_kernel_x64 = (_str_t)"Running kernel x86-64...\r\n";
_u16 _gdt_limit=0;
_u8 _gdt[MAX_GDT_RECORDS*8] __attribute__((aligned(16))) = "";

#include "stub.h"
#include "startup_context.h"
#include "boot.h"

_startup_context_t sc;

void __attribute__((aligned(4096))) _cpu_init_vector_x86_64(void) {
	// restore stack pointer
	__asm__ __volatile__ (
		"jmp	$0, $_lp_cpu_init_\n" /* normalize CS */
		"nop\n"
		"_lp_cpu_init_:\n"
		"movl 	%0, %%esp\n" 
		: :"m"(sc.stack_ptr) 
	);
	if(pre_init() != -1)
		// switch in long mode to call 'void (*core_cpu_init_vector)(_u32 data)'
		_lm_call((_t_lm_proc *)sc.cpu_info._cpu._x86.core_cpu_init_vector,
				(void *)sc.cpu_info._cpu._x86.core_cpu_init_data);
}

_u8 __end__;
