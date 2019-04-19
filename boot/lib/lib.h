#ifndef __BOOT_LIB__
#define __BOOT_LIB__

#define KEY_UP_ARROU	0x4800
#define KEY_DOWN_ARROW	0x5000

//lib
_umax _pow32(_umax base, _u8 p);
_u32 mem_cmp(_u8 *p1, _u8 *p2, _u32 sz);
void mem_cpy(_u8 *dst, _u8* src, _u32 sz);
_u16 str_len(_str_t str);
_u32 txt_cmp(_str_t p1, _str_t p2, _u32 sz);
_u32 str2i(_str_t str, _s8 sz);
void mem_set(_u8 *ptr, _u8 pattern, _u16 sz);


#endif
