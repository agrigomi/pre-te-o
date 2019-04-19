// architecture specific interface
#include "dtype.h"
#include "boot.h"
#include "stub.h"
#include "lib.h"

#ifdef _STAGE_2_
#include "bios.h"
_u8 *get_s3_address(void) {
	return (_u8 *)BOOT_S3_ADDRESS;
}

extern _u8 _io_buffer[];
_u8 *get_io_buffer(void) {
	return _io_buffer;
}

void text_out(_str_t str, _u16 sz) {
	bios_print_text(str, sz);
}

_u8 get_device_parameters(_u8 dev, _device_parameters_t *dp) {
	_bios_device_parameters_t bdp;
	_u8 r = bios_get_device_parameters(dev, &bdp);
	
	dp->sectors = bdp.sectors;
	dp->sz_sector = bdp.sz_sector;
	
	return r;
}

_u8 sector_read(_lba_t *p_lba, _u8 dev, _u8 count, _u8 *_ptr) {
	return bios_rw_sector(BIOS_FN_READ, p_lba, dev, count, _ptr);
}

_u8 sector_write(_lba_t *p_lba, _u8 dev, _u8 count, _u8 *_ptr) {
	return bios_rw_sector(BIOS_FN_WRITE, p_lba, dev, count, _ptr);
}

void qword_out(_u64 qw) {
	bios_print_qword((_u32 *)&qw);
}

_u32 buffer_size(void) {
	return 4096;
}

_boot_stub_t _g_stub = {
	._mem = { // _mem
		.s3_address	= get_s3_address,
		.io_buffer	= get_io_buffer,
		.io_size	= buffer_size,
		.mmap		= bios_get_memory_map_e820,
		.enable_a20	= bios_enable_a20,
	},
	
	._dev = { // _dev
		.read		= sector_read,
		.write		= sector_write,
		.parameters	= get_device_parameters,
	},
	
	._display = { // _display
		.char_out	= bios_print_char,
		.byte_out	= bios_print_byte,
		.word_out	= bios_print_word,
		.dword_out	= bios_print_dword,
		.qword_out	= qword_out,
		.text_out	= bios_print_text,
		.hex_out	= bios_print_hex,
		.display_text	= bios_display_text,
		.cursor_pos	= bios_set_cursor_pos,
		.hide_cursor	= bios_hide_cursor,
		.clear_screen	= bios_clear_screen,
	},
	
	._kbd = { // _kbd
		.wait_key	= bios_wait_key,
		.get_key	= bios_get_key,
	},
	
	._fs = { // _fs
		.file_info	= fs_get_file_info,
		.block_size	= fs_get_block_size,
		.file_size	= fs_get_file_size,
		.fs_type	= fs_get_type,
		.read_block	= fs_read_file_block,
		.read_file	= fs_read_file,
	},
	
	.wait	= bios_wait,
};

#endif

#ifdef _STAGE_3_

_boot_stub_t *_g_p_stub;

_u8 *get_kernel_address(void) {
	return (_u8 *)KERNEL_ADDRESS;
}


_u16 get_memory_map(_u8 *buffer,_u16 sz) {
	return _g_p_stub->_mem.mmap(buffer, sz);
}

_s32 pre_init(void) {
	_s32 r = 0;
	
	pm_init_gdt();
	if((r = enable_a20()) == -1) 
		_g_p_stub->_display.text_out(err_a20,str_len(err_a20));
	
	return r;
}

_u32 copy(_u8 *p_dst, _u8 *p_src, _u32 n) {
	_u32 r = 0;
	copy_32_t cpy;

	cpy.dst = (_u32)p_dst;
	cpy.src = (_u32)p_src;
	cpy.sz = n;
	
	_pm_call((_t_pm_proc *)_pm_copy,&cpy);
	return r;
}

// return number of loaded bytes
_u32 _load_file(_u8 *p_file_info,	// file system specific data
		_u8 *_io,		// pointer to I/O buffer
		_u32 _io_sz,		// size of I/O buffer
	       _u8 *p_dst		// destination address
	       ) {
	_u32 r = 0;
	_u32 file_size = 0;
	_u32 block_size = 0;
	_u32 nblock = 0; // file size in file system units
	_u32 i = 0;
	_u32 rb = 0; 
	
	file_size = _g_p_stub->_fs.file_size(p_file_info);
	block_size = _g_p_stub->_fs.block_size(p_file_info);
	nblock = (file_size / block_size);
	nblock += (file_size % block_size) ? 1 : 0;

	for(i = 0; i < nblock; i++) {
		rb = _g_p_stub->_fs.read_block(p_file_info,i, 1, _io, _io_sz);
		if(rb) {
			copy(p_dst + (i * block_size), _io, rb);
			_g_p_stub->_display.char_out('.');
			r += rb;
		} else {
			_g_p_stub->_display.text_out(err_read,str_len(err_read));
			break;
		}
	}
	
	return r;
}

// return number of loaded bytes
_u32 load_file(	_u8 dev,		// device number
		_str_t p_file_name,	// full path to file
		_u8 *_io,		// pointer to I/O buffer
		_u32 _io_sz,		// size of I/O buffer
	       _u8 *p_dst		// destination address
	     ) {
	_u32 r = 0;
	_u8 file_info[MIN_FILE_INFO_SZ];
	
	if(_g_p_stub->_fs.file_info(dev, p_file_name, file_info, sizeof(file_info), _io, _io_sz))
		r = _load_file(file_info, _io, _io_sz, p_dst);
	
	return r;
}

#endif
			
