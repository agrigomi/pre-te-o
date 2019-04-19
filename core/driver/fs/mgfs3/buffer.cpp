#include "buffer.h"
#include "str.h"
#include "iFSIO.h"
#include "repository.h"

static iFSBuffer *_g_p_fs_buffer_ = 0;

iFSBuffer *get_fsb(void) {
	if(!_g_p_fs_buffer_)
		_g_p_fs_buffer_ = (iFSBuffer *)__g_p_repository__->get_object_by_interface(I_FS_BUFFER, RY_STANDALONE);
	return _g_p_fs_buffer_;
}

_h_buffer_ mgfs_buffer_alloc(_mgfs_context_t *p_cxt, _u32 unit_number, _h_lock_ hlock) {
	_h_buffer_ r = INVALID_BUFFER_HANDLE;
	
	if(!mgfs_get_info(p_cxt))
		return r;
	
	iFSBuffer *p_fsb = get_fsb();
	if(p_fsb)
		r = p_fsb->alloc(&(p_cxt->fs_context), unit_number, hlock);
	
	return r;
}

void *mgfs_buffer_ptr(_mgfs_context_t *p_cxt, _h_buffer_ hb) {
	void *r = 0;
	
	iFSBuffer *p_fsb = get_fsb();
	if(p_fsb)
		r = p_fsb->ptr(&(p_cxt->fs_context), hb);
	
	return r;
}

_u32 mgfs_buffer_unit(_mgfs_context_t *p_cxt, _h_buffer_ hb) {
	_u32 r = INVALID_UNIT_NUMBER;
	
	iFSBuffer *p_fsb = get_fsb();
	if(p_fsb)
		r = p_fsb->unit(&(p_cxt->fs_context), hb);
	
	return r;
}

void mgfs_buffer_free(_mgfs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock) {
	iFSBuffer *p_fsb = get_fsb();
	if(p_fsb)
		p_fsb->free(&(p_cxt->fs_context), hb, hlock);
}

void mgfs_buffer_write(_mgfs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock) {
	iFSBuffer *p_fsb = get_fsb();
	if(p_fsb)
		p_fsb->write(&(p_cxt->fs_context), hb, hlock);
}

void mgfs_buffer_flush(_mgfs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock) {
	iFSBuffer *p_fsb = get_fsb();
	if(p_fsb)
		p_fsb->flush(&(p_cxt->fs_context), hb, hlock);
}

void mgfs_buffer_cleanup(_mgfs_context_t *p_cxt, _h_lock_ hlock) {
	iFSBuffer *p_fsb = get_fsb();
	if(p_fsb)
		p_fsb->cleanup(&(p_cxt->fs_context), hlock);
}
