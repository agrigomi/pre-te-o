#ifndef __BOOT_H__
#define __BOOT_H__

#include "mgtype.h"
#include "startup_context.h"

#define FILE_INFO_SIZE		64
#define MAX_FILE_BLOCKS		0x4c2

extern _startup_context_t _g_sc_;

void halt(void);

// bios
void wait(_u32 micro_s);
_u16 get_key(void);
_u16 wait_key(void);

// addr 
_u32 get_core_space_ptr(void);
_u32 get_core_space_end(void);
_u32 get_boot_address(void);

// display
void print_char(_char_t c);
void display_text(_str_t _ptr, _u16 sz, _u8 row, _u8 col, _u8 color);
void clear_screen(void);
void set_cursor_pos(_u8 row, _u8 col);
void print_hex(_u8 *ptr,_u16 sz);
void print_qword(_u32 qw[2]);
void print_dword(_u32 dw);
void print_word(_u16 w);
void print_byte(_u8 c);
void print_text(_str_t p, _u16 sz);
void hide_cursor(void);

// I/O
void io_buffer_init(void);
void *alloc_io_buffer(_u16 size);
void free_io_buffer(void *p_io_buffer);


// conetxt
void init_context(void);
_u8 load_kernel(void *finfo);
void start_kernel(_str_t arg);

// filesystem
#define FSERR		0xff

_u16 sector_size(void);
_u16 get_unit_size(void);
_u8 fs_superblock_sector(void);
_u8 fs_superblock_offset(void);
void fs_init(void);
_u8 fs_get_file_info(_str_t fpath, void *finfo);
_u32 fs_get_file_blocks(void *finfo);
_u32 fs_get_file_size(void *finfo);
_u32 fs_read_file(void *finfo, _u32 dst_addr);
_u8 fs_read_file_block(void *finfo, _u32 block, _u32 dst_address);
void fs_get_serial(_u8 *bserial, _u32 sz_bserial);

// disk
_u32 read_sector(_u32 sector,   // sector number  (at begin of partition)
		 _u32 count, 	// number of sectors to read
		 _u32 dst 	// destination address
		);
void menu(_u8 *p_cfg, _u32 cfg_sz, _str_t p_file_name, _u16 *name_sz,_str_t p_args,_u16 *args_sz);

#endif

