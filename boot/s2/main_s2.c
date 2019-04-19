#include "stub.h"

extern _boot_stub_t _g_stub;

_str_t _boot_s3_name=(_str_t)"/boot/boot_s3.bin";
_u8 _io_buffer[]="_io_buffer";

typedef void start_s3_t(_u32 dev, _boot_stub_t *p_stub);

void main_s2(_u8 dev) {
	_u8 file_info[FILE_INFO_SZ];
	_u32 sz_s3 = 0;
	_u16 sz_block = 0;
	_u32 count;
	_u8 *_io = _io_buffer;
	_u8 *s3 = _g_stub._mem.s3_address();
	start_s3_t *start_s3 = (start_s3_t *)s3;

	if(_g_stub._fs.file_info(dev,_boot_s3_name,file_info,sizeof(file_info),_io,_g_stub._mem.io_size())) {
		sz_s3 = _g_stub._fs.file_size(file_info);
		sz_block = _g_stub._fs.block_size(file_info);
		
		count = sz_s3 / sz_block;
		if(sz_s3 % sz_block)
		 	count++;
		
		if(_g_stub._fs.read_block(file_info,0,count,s3,count * sz_block)) {
			// call stage 3
			start_s3((_u32)dev, &_g_stub);
		}
	}
	
	while(1);
}
