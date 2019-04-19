#ifndef __STR__
#define __STR__

#include "mgtype.h"


#ifndef __GNUC_VA_LIST
#define __GNUC_VA_LIST
typedef __builtin_va_list __gnuc_va_list;
typedef __gnuc_va_list va_list;
#endif


#define INVALID_STRING_POSITION	0xffffffff

#define va_start(v,l)	__builtin_va_start(v,l)
#define va_end(v)	__builtin_va_end(v)
#define va_arg(v,l)	__builtin_va_arg(v,l)

#ifdef __cplusplus
extern "C" {
#endif

void mem_cpy(_u8 *dst, _u8 *src, _u32 sz);
_u32 mem_cmp(_u8 *p1, _u8 *p2, _u32 sz);
void mem_set(_u8 *ptr, _u8 pattern, _u32 sz);
_u32 str_len(_str_t str);
_s32 str_cmp(_str_t str1, _str_t str2);
_s32 str_ncmp(_str_t str1, _str_t str2, _u32 sz);
_u32 str_cpy(_str_t dst, _str_t src, _u32 sz_max);
_u32 _vsnprintf(_str_t buf, _u32 size, _cstr_t fmt, va_list args);
_u32 _snprintf(_str_t buf, _u32 size, _cstr_t fmt, ...);
_u32 find_string(_str_t str1,_str_t str2);
_str_t _toupper(_str_t s);
_u32 str2i(_str_t str, _s32 sz);
void trim_left(_str_t str);
void trim_right(_str_t str);
void clrspc(_str_t str);
_u8  div_str(_str_t str,_str_t p1,_u32 sz_p1,_str_t p2,_u32 sz_p2,_cstr_t div);
_u8 div_str_ex(_str_t str, _str_t p1, _u32 sz_p1, _str_t p2, _u32 sz_p2, _cstr_t div, _s8 start_ex, _s8 stop_ex);
_u32 wildcmp(_str_t string,_str_t wild);
_str_t _itoa(_s32 n, _str_t s, _u8 b);
_str_t _uitoa(_u32 n, _str_t s, _u8 b);
_str_t _ulltoa(_u64 n, _str_t s, _u8 b);

#ifdef __cplusplus
};
#endif // __cplusplus

#endif
