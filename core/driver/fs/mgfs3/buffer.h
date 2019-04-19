#ifndef __MGFS_BUFFER_H__
#define __MGFS_BUFFER_H__

#include "mgfs3.h"

_h_buffer_ mgfs_buffer_alloc(_mgfs_context_t *p_cxt, _u32 unit_number, _h_lock_ hlock=0);
void *mgfs_buffer_ptr(_mgfs_context_t *p_cxt, _h_buffer_ hb);
_u32 mgfs_buffer_unit(_mgfs_context_t *p_cxt, _h_buffer_ hb);
void mgfs_buffer_free(_mgfs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock=0);
void mgfs_buffer_write(_mgfs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock=0);
void mgfs_buffer_flush(_mgfs_context_t *p_cxt, _h_buffer_ hb=INVALID_BUFFER_HANDLE, _h_lock_ hlock=0);
void mgfs_buffer_cleanup(_mgfs_context_t *p_cxt, _h_lock_ hlock=0);

#endif
