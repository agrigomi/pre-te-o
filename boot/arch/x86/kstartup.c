#include "stub.h"
#include "boot.h"
#include "startup_context.h"
#include "lib.h"
#include "segment.h"

typedef void kernel_startup_t(_startup_context_t *p_context);

extern _boot_stub_t *_g_p_stub;
extern _u8 __end__;
extern _startup_context_t sc;

// PML4
extern void _build_pml4(_u8 *pt_addr, _startup_context_t *p_sc);

void _cpu_init_vector_x86(void) {
	__asm__ __volatile__ (
		"_idle_loop_:\n"
		"nop\n"
		"hlt\n"
		"nop\n"
		"jmp _idle_loop_\n"
	);
}

extern void __attribute__((aligned(4096))) _cpu_init_vector_x86_64(void);

void call_kernel(_u8 dev, _u8 kcpu_mode, _u32 kernel_address,_u32 sz, _str_t kargs) {
	_device_parameters_t dp;
	_u16 sz_mem_map=sizeof(sc.mmap); // memory map in bytes
	_u32 n = 0;
	_u32 i;
	
	mem_set((_u8 *)sc.mmap, 0, sizeof(sc.mmap));
	mem_set((_u8 *)sc.rmap, 0, sizeof(sc.rmap));
	sc.mm_cnt = 0;
	sc.rm_cnt = 0;
	
	// fix the stack pointer
	__asm__ __volatile__("movl %%esp, %0" :"=m"(sc.stack_ptr):);

	// get memory map
	n = _g_p_stub->_mem.mmap((_u8 *)&sc.mmap[sc.mm_cnt], sz_mem_map);
	sc.mm_cnt += n / sizeof(_mem_tag_t);

	if(!n) {
	 	_g_p_stub->_display.text_out(err_mem_map,str_len(err_mem_map));
		_g_p_stub->_kbd.wait_key();
	}
	
	// boot device parameters
	if(!_g_p_stub->_dev.parameters(dev,&dp)) {
		sc.sector_sz = dp.sz_sector;
		sc.device_sz = dp.sectors;
	} else {
		sc.sector_sz = 512;
		sc.device_sz = -1;
	}

	// fs
	sc.dev = dev;
	sc.fs_type = _g_p_stub->_fs.fs_type();

	// I/O
	sc.rmap[sc.rm_cnt].address = 0x00;
	sc.rmap[sc.rm_cnt].size = 0x1000;
	sc.rmap[sc.rm_cnt].type = MEM_TYPE_RESERVED;
	sc.rm_cnt++;

	// add stack area to memory map
	sc.rmap[sc.rm_cnt].address = 0x1000;
	sc.rmap[sc.rm_cnt].size = BOOT_S2_ADDRESS - 0x1000;
	sc.rmap[sc.rm_cnt].type = MEM_TYPE_STACK;
	sc.rm_cnt++;

	// add boot area in memory map
	sc.rmap[sc.rm_cnt].address = BOOT_S2_ADDRESS;
	sc.rmap[sc.rm_cnt].size = (_u32)kernel_address - BOOT_S2_ADDRESS;
	sc.rmap[sc.rm_cnt].type = MEM_TYPE_BOOT_CODE;
	sc.rm_cnt++;
	
	// add core area to memory map
	sc.rmap[sc.rm_cnt].address = kernel_address;
	sc.rmap[sc.rm_cnt].size = sz;
	sc.rmap[sc.rm_cnt].type = MEM_TYPE_CORE_CODE;
	sc.rm_cnt++;
	
	sc.p_kargs = (_u64)(_u32)kargs;

	// paging
	sc.pt_page_size = 0;
	sc.pt_range = 0;
	sc.pt_type = 0;
	
	// setments
	sc.cpu_info._cpu._x86.p_gdt = (_u32)_gdt;
	sc.cpu_info._cpu._x86.gdt_limit = _gdt_limit;
	sc.cpu_info._cpu._x86.free_selector = FREE_SEGMENT_INDEX;
	
	// CPU info by default
	// the bootloader knows only about the current CPU
	sc.cpu_info.nsock = 1; // one socket
	sc.cpu_info._cpu._x86.nlcpu = 1;
	sc.cpu_info._cpu._x86.ncore = 1;
	sc.cpu_info._cpu._x86.iapic_id = get_initial_apic_id();
	
	switch(kcpu_mode) {
		case 0x32:
			_g_p_stub->_display.text_out(str_run_kernel_x86,str_len(str_run_kernel_x86));
			sc.cpu_info._cpu._x86.cpu_init_vector_rm = (_u32)_cpu_init_vector_x86;

			// call kernel without 'pagging' (segmentation only)
			_pm_call((_t_pm_proc *)kernel_address, &sc);
			break;

		case 0x64:
			if(lm_support()) {
				sc.cpu_info._cpu._x86.code_selector=FLAT_CODE_SEGMENT_64;
				sc.cpu_info._cpu._x86.data_selector=FLAT_DATA_SEGMENT_64;
				sc.cpu_info._cpu._x86.cpu_init_vector_rm = (_u32) _cpu_init_vector_x86_64;
				sc.cpu_info._cpu._x86.core_cpu_init_vector = 0;
				sc.cpu_info._cpu._x86.core_cpu_init_data = 0;
				
				sc.pt_address = PML4_PT_ADDRESS;

				//build long mode 'pagetable' (PML4)
				_build_pml4((_u8 *)PML4_PT_ADDRESS, &sc);
				
				_g_p_stub->_display.char_out('\r');
				_g_p_stub->_display.char_out('\n');
				for(i = 0; i < sc.mm_cnt; i++) {
					_g_p_stub->_display.qword_out(sc.mmap[i].address);
					_g_p_stub->_display.char_out('/');
					_g_p_stub->_display.qword_out(sc.mmap[i].size);
					_g_p_stub->_display.char_out('/');
					_g_p_stub->_display.dword_out(sc.mmap[i].type);
					_g_p_stub->_display.char_out('\r');
					_g_p_stub->_display.char_out('\n');
				}
				_g_p_stub->_display.text_out((_str_t)"---\r\n",5);
				for(i = 0; i < sc.rm_cnt; i++) {
					_g_p_stub->_display.qword_out(sc.rmap[i].address);
					_g_p_stub->_display.char_out('/');
					_g_p_stub->_display.qword_out(sc.rmap[i].size);
					_g_p_stub->_display.char_out('/');
					_g_p_stub->_display.dword_out(sc.rmap[i].type);
					_g_p_stub->_display.char_out('\r');
					_g_p_stub->_display.char_out('\n');
				}
				
				_g_p_stub->_display.text_out(str_run_kernel_x64, str_len(str_run_kernel_x64));
				_lm_call((_t_lm_proc *)kernel_address, &sc);
			} else {
				_g_p_stub->_display.text_out(err_long_mode, str_len(err_long_mode));
			}
			
			break;
	}
}

