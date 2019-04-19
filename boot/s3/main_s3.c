#include "stub.h"
#include "lib.h"

extern _str_t err_file;
//extern _u8 _io_buffer[];
extern _str_t _boot_config;
extern void call_kernel(_u8 dev, _u8 kcpu_mode, _u32 kernel_address,_u32 sz, _str_t kargs);
extern void menu(_u8 *p_buffer, _u32 cfg_sz, _str_t p_file_name, _u16 *name_sz, _str_t args, _u16 *sz_args);
extern _str_t str_load_kernel;
extern _boot_stub_t *_g_p_stub;

void print_file_error(_str_t err_text, _str_t name) {
 	_g_p_stub->_display.text_out(err_text, str_len(err_text));
	_g_p_stub->_display.text_out(name, str_len(name));
	_g_p_stub->_display.char_out('\r');
	_g_p_stub->_display.char_out('\n');
	_g_p_stub->_kbd.wait_key();
	_g_p_stub->_display.clear_screen();
}

void main_s3(_u32 dev, _boot_stub_t *p_stub) {
	_char_t file_name[128];
	_char_t args[256];
	_u8 file_info[FILE_INFO_SZ];
	_u16 sz_name = sizeof(file_name);
	_u16 sz_args = sizeof(args);
	_u32 sz_cfg = 0;
	_u32 sz_file = 0;
	_u16 sz_block = 0;
	_u32 count = 0;
	_u8 *_io = (_u8 *)p_stub->_mem.io_buffer();
	_u32 _io_sz = p_stub->_mem.io_size();
	_u8 cpu_mode = 0x32;
	_u8 *kaddress = get_kernel_address();
	
	_g_p_stub = p_stub;
	
	if(pre_init() != -1) {

		while(1) {
			sz_name = sizeof(file_name);
			sz_args = sizeof(args);
			
			mem_set((_u8 *)file_name,0,sz_name);
			mem_set((_u8 *)args,0,sz_args);
			
			if(_g_p_stub->_fs.file_info(dev,_boot_config,file_info,sizeof(file_info),_io,_io_sz)) {
				sz_cfg = _g_p_stub->_fs.file_size(file_info);
				sz_block = _g_p_stub->_fs.block_size(file_info);
			
				count = sz_cfg / sz_block;
				if(sz_cfg % sz_block)
					count++;
				
				if(_g_p_stub->_fs.read_block(file_info,0,count,_io,_io_sz)) {
					_g_p_stub->_display.clear_screen();
					menu(_io,sz_cfg,file_name,&sz_name,args,&sz_args);
					_g_p_stub->_display.clear_screen();
				}

				if(_g_p_stub->_fs.file_info(dev,file_name,file_info,sizeof(file_info),_io,_io_sz)) {
					_g_p_stub->_display.text_out(str_load_kernel, str_len(str_load_kernel));
					if(_g_p_stub->_fs.read_block(file_info, 0, 1, _io, _io_sz)) {
						cpu_mode = _io[4];
						sz_file = _load_file(file_info, _io, _io_sz, kaddress);
						if(sz_file)
							call_kernel(dev, cpu_mode, (_u32)kaddress, sz_file, args);
					}
				} else
					print_file_error(err_file,file_name);
			} else
				print_file_error(err_file,_boot_config);
		}
	}
 }	

