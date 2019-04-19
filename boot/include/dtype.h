#ifndef __BOOT_DTYPE__
#define __BOOT_DTYPE__

typedef unsigned char	_u8;
typedef char		_s8;
typedef unsigned short	_u16;
typedef unsigned int	_u32;
typedef int		_s32;
typedef _s8*		_str_t;
typedef _s8		_char_t;
typedef unsigned long long _u64;

typedef union {
	unsigned long long	_qw;	// quad word
	struct {
 		_u32	_lw;	// lo word
		_u32	_hw;	// hi word
	};
} _lba_t;

typedef  _u32	_umax;


#endif

