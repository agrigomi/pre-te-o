#ifndef __MGTYPE__
#define __MGTYPE__

#define __W64
//#define DEBUG

#ifdef __W64
typedef unsigned long long	_umax;
#else
typedef unsigned int		_umax;
#endif
typedef unsigned long long	_u64;
typedef signed long long	_s64;
typedef unsigned int		_u32;
typedef signed int		_s32;
typedef unsigned short		_u16;
typedef short			_s16;
typedef unsigned char		_u8;
typedef char			_s8;
typedef _s8*			_str_t;
typedef _s8			_char_t;
typedef const char * 		_cstr_t;
typedef unsigned long		_ulong;
typedef signed long		_slong;

typedef union {
	_u64	_qw;	// quad word
	struct {
 		_u32	_ldw;	// lo dword
		_u32	_hdw;	// hi dword
	};
}__attribute__((packed)) _lba_t, _u64_t;


#define __cdecl extern "C"

#endif
