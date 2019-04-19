// architecture independent layer
#ifndef __BOOT_STUB__
#define __BOOT_STUB__

#include "dtype.h"

_u8 *get_s3_address(void);
_u8 *get_kernel_address(void);
_u8 *get_io_buffer(void);


_u16 get_memory_map(_u8 *buffer,_u16 sz);
_s32 pre_init(void);

#define SZ_SECTOR	512
#define FILE_INFO_SZ	512

typedef struct {
	_u64	sectors; /* Number of sectors. This shall be one greater than the maximum sector number. If this field is greater
				than 15,482,880 then word 2, bit 1 shall be cleared to zero.*/
	_u16	sz_sector; /* Number of bytes in a sector.*/
}_device_parameters_t;

_u8 get_device_parameters(_u8 dev, _device_parameters_t *dp);

//fs
#define BOOT_MGFS	1
#define BOOT_CDFS	2

// read file information structure in 'p_info_buffer' and return number of bytes in p_info_buffer
_u16 fs_get_file_info(_u8 dev,			// device number
		      _str_t p_file_name,		// full path to file
		      _u8 *p_info_buffer,	// file information buffer needed by other file operations
		      _u16 sz_buffer,		// size of info buffer ( this function return 0 if 'sz_buffer' is to small )
		      _u8 *p_io_buffer,		// buffer for i/o operations 
		      _u16 sz_io_buffer		// size of i/o buffer ( min. one block) see 'fs_get_block_size'
		      );
// return file system block size
_u16 fs_get_block_size(_u8 *p_file_info);
// return the size of file in bytes
_u32 fs_get_file_size(_u8 *p_file_info);
// return the file system type
_u8 fs_get_type(void);
// read one block from file content & return number of reade bytes
_u32 fs_read_file_block(_u8 *p_file_info, 	// file information buffer
			_u32 block_num, 	// block number in file
			_u32 count,		// number of file blocks to read
			_u8 *p_buffer, 
			_u32 sz_buffer
			);
// read 'size' bytes from file 'offset' and return number of actual bytes
// 	this function using i/o buffer 'p_file_info->io'
_u32 fs_read_file(_u8 *p_file_info,		// file information buffer
		  _umax offset,			// offset at begin of file in bytes
		  _u32 size,			// number of bytes to read
		  _u8 *p_buffer,		// destination buffer
		  _u32 sz_buffer		// size of destination buffer
		  );

_u8 rw_sector(_u8 mode, _lba_t *p_lba, _u8 dev, _u8 count, _u8 *_ptr);

// return number of loaded bytes
_u32 _load_file(_u8 *p_file_info,
		_u8 *_io,		// pointer to I/O buffer
		_u32 io_sz,		// size of I/O buffer
		_u8 *p_dst		// destination address
	       );
// return number of loaded bytes
_u32 load_file(_u8 dev,			// device number
		_str_t p_file_name,	// full path to file
		_u8 *_io,		// pointer to I/O buffer
		_u32 io_sz,		// size of I/O buffer
	        _u8 *p_dst		// destination address
	     );


typedef struct {
	struct {
		_u8 *(*s3_address)(void);
		_u8 *(*io_buffer)(void);
		_u32 (*io_size)(void);
		_u16 (*mmap)(_u8 *buffer,_u16 sz);
		void (*enable_a20)(void);
	} _mem;
	
	struct {
		_u8 (*read)(_lba_t *p_lba, _u8 dev, _u8 count, _u8 *_ptr);
		_u8 (*write)(_lba_t *p_lba, _u8 dev, _u8 count, _u8 *_ptr);
		_u8 (*parameters)(_u8 dev, _device_parameters_t *dp);
	}_dev;
	
	struct {
		void (*char_out)(_char_t c);
		void (*byte_out)(_u8 b);
		void (*word_out)(_u16 w);
		void (*dword_out)(_u32 dw);
		void (*qword_out)(_u64  dqw);
		void (*text_out)(_str_t str, _u16 sz);
		void (*hex_out)(_u8 *ptr,_u16 sz);
		void (*display_text)(_str_t _ptr, _u16 sz, _u8 row, _u8 col, _u8 color);
		void (*cursor_pos)(_u8 row, _u8 col);
		void (*hide_cursor)(void);
		void (*clear_screen)(void);
	}_display;
	
	struct {
		_u16 (*wait_key)(void);
		_u16 (*get_key)(void);
	}_kbd;
	
	struct {
		// read file information structure in 'p_info_buffer' and return number of bytes in p_info_buffer
		_u16 (*file_info)(_u8 dev,		// device number
				  _str_t p_file_name,	// full path to file
				  _u8 *p_info_buffer,	// file information buffer needed by other file operations
				  _u16 sz_buffer,	// size of info buffer (this function return 0 if 'sz_buffer' is to small)
				  _u8 *p_io_buffer,	// buffer for i/o operations 
				  _u16 sz_io_buffer	// size of i/o buffer ( min. one block) see 'fs_get_block_size'
				 );
		// return file system block size
		_u16 (*block_size)(_u8 *p_file_info);
		// return the size of file in bytes
		_u32 (*file_size)(_u8 *p_file_info);
		// return the file system type
		_u8 (*fs_type)(void);
		// read one block from file content & return number of bytes
		_u32 (*read_block)(_u8 *p_file_info, 	// file information buffer
					_u32 block_num,	// block number in file
					_u32 count,	// number of file blocks to read
					_u8 *p_buffer, 
					_u32 sz_buffer
					);
		// read 'size' bytes from file 'offset' and return number of actual received bytes
		// 	this function using i/o buffer 'p_file_info->io'
		_u32 (*read_file)(_u8 *p_file_info,	// file information buffer
				  _umax offset,		// offset at begin of file in bytes
				  _u32 size,		// number of bytes to read
				  _u8 *p_buffer,	// destination buffer
				  _u32 sz_buffer	// size of destination buffer
				  );
	}_fs;

	void (*wait)(_u32 micro_s);
}_boot_stub_t;
#endif

