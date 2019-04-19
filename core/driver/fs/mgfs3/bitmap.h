#ifndef __MGFS_BITMAP_H__
#define __MGFS_BITMAP_H__

#include "mgfs3.h"

#define MAX_BITMAP_BITS	248
#define MAX_BITMAP_BYTE	31

typedef struct {
	_u8	used;
	_u8	bitmap[MAX_BITMAP_BYTE];// 31byte=248bit
}__attribute__((packed)) _mgfs_bitmap_t;

bool mgfs_bitmap_set(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u32 bit_number, bool bit_state, _h_lock_ hlock=0); 
_u32 mgfs_bitmap_set_array(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u32 *array, _u32 count, bool bit_state, _h_lock_ hlock=0); 
_u32 mgfs_bitmap_alloc(_mgfs_context_t *p_cxt, _u32 *array, _u32 count, _h_inode_ hinode, _h_lock_ hlock=0);
_u32 mgfs_bitmap_free(_mgfs_context_t *p_cxt, _u32 *array, _u32 count, _h_inode_ hinode, _h_lock_ hlock=0);
// return 0 for success
_u32 mgfs_bitmap_sync(_mgfs_context_t *p_cxt, _u32 *array, _u32 count, _h_inode_ hinode, _h_inode_ hshadow, _h_lock_ hlock=0);
// return the number of free bits
_u32 mgfs_bitmap_free_state(_mgfs_context_t *p_cxt, _h_inode_ hinode, _h_lock_ hlock=0);

#endif

