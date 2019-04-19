#include "mgfs3.h"
#include "str.h"

_mgfs_info_t *mgfs_get_info(_mgfs_context_t *p_cxt) {
	_mgfs_info_t *r = 0;
	
	if(p_cxt->fs.magic == MGFS_MAGIC)
		r = &(p_cxt->fs);
	else {
		_fs_context_t *p_fscxt = &(p_cxt->fs_context);
		p_fscxt->bmap_sz = 0;
		p_fscxt->p_buffer_map = 0;
		p_fscxt->write_back = false;
		
		if(p_fscxt->p_alloc && p_fscxt->p_read && p_fscxt->p_free) {
			void *p_sector = p_fscxt->p_alloc(p_fscxt->p_io_context, MGFS_SECTOR_SIZE);
			if(p_sector) {
				if(p_fscxt->p_read(p_fscxt->p_io_context, MGFS_SB_SECTOR, 1, p_sector)) {
					mem_cpy((_u8 *)&(p_cxt->fs), (_u8 *)p_sector + MGFS_SB_OFFSET, sizeof(_mgfs_info_t));
					if(p_cxt->flags & MGFS_DELAY_WRITE)
						p_fscxt->write_back = true;
					r = &(p_cxt->fs);
				}
				
				p_fscxt->p_free(p_fscxt->p_io_context, p_sector, MGFS_SECTOR_SIZE);
			}
		}
	}
	
	return r;
}

void mgfs_update_info(_mgfs_context_t *p_cxt) {
	_fs_context_t *p_fscxt = &(p_cxt->fs_context);

	if(p_fscxt->p_alloc && p_fscxt->p_read && p_fscxt->p_write && p_fscxt->p_free) {
		void *p_sector = p_fscxt->p_alloc(p_fscxt->p_io_context, MGFS_SECTOR_SIZE);
		if(p_sector) {
			if(p_fscxt->p_read(p_fscxt->p_io_context, MGFS_SB_SECTOR, 1, p_sector)) {
				mem_cpy((_u8 *)p_sector + MGFS_SB_OFFSET, (_u8 *)&(p_cxt->fs), sizeof(_mgfs_info_t));
				p_fscxt->p_write(p_fscxt->p_io_context, MGFS_SB_SECTOR, 1, p_sector);
			}
			
			p_fscxt->p_free(p_fscxt->p_io_context, p_sector, MGFS_SECTOR_SIZE);
		}
	}
}

_u32 mgfs_unit_read(_mgfs_context_t *p_cxt, _u32 nunit, void *p_buffer) {
	_u32 r = 0;
	_fs_context_t *p_fscxt = &(p_cxt->fs_context);
	
	if(p_fscxt->p_read && p_cxt->fs.magic == MGFS_MAGIC && nunit < p_cxt->fs.sz_fs) {
		_u32 sector = nunit * p_cxt->fs.sz_unit;
		r = p_fscxt->p_read(p_fscxt->p_io_context, sector, p_cxt->fs.sz_unit, p_buffer);
	}
	return r;
}

_u32 mgfs_unit_write(_mgfs_context_t *p_cxt, _u32 nunit, void *p_buffer) {
	_u32 r = 0;
	_fs_context_t *p_fscxt = &(p_cxt->fs_context);

	if(p_fscxt->p_write && p_cxt->fs.magic == MGFS_MAGIC && nunit < p_cxt->fs.sz_fs) {
		_u32 sector = nunit * p_cxt->fs.sz_unit;
		r = p_fscxt->p_write(p_fscxt->p_io_context, sector, p_cxt->fs.sz_unit, p_buffer);
	}
	return r;
}

void *mgfs_mem_alloc(_mgfs_context_t *p_cxt, _u32 sz) {
	void *r = 0;
	_fs_context_t *p_fscxt = &(p_cxt->fs_context);
	
	if(p_fscxt->p_alloc)
		r = p_fscxt->p_alloc(p_fscxt->p_io_context, sz);
	
	return r;
}

void mgfs_mem_free(_mgfs_context_t *p_cxt, void *ptr, _u32 size) {
	_fs_context_t *p_fscxt = &(p_cxt->fs_context);
	if(p_fscxt->p_free)
		p_fscxt->p_free(p_fscxt->p_io_context, ptr, size);
}

_h_lock_ mgfs_lock(_mgfs_context_t *p_cxt, _h_lock_ hlock) {
	_h_lock_ r = 0;
	_fs_context_t *p_fscxt = &(p_cxt->fs_context);
	
	if(p_fscxt->p_lock)
		r = p_fscxt->p_lock(p_fscxt->p_io_context, hlock);
	
	return r;
}

void mgfs_unlock(_mgfs_context_t *p_cxt, _h_lock_ hlock) {
	_fs_context_t *p_fscxt = &(p_cxt->fs_context);
	if(p_fscxt->p_unlock)
		p_fscxt->p_unlock(p_fscxt->p_io_context, hlock);
}

_u32 mgfs_timestamp(_mgfs_context_t *p_cxt) {
	_u32 r = 0;
	_fs_context_t *p_fscxt = &(p_cxt->fs_context);

	if(p_fscxt->p_timestamp)
		r = p_fscxt->p_timestamp(p_fscxt->p_io_context);

	return r;
}

_u32 mgfs_unit_size(_mgfs_context_t *p_cxt) {
	_u32 r = 0;
	_mgfs_info_t *p_fs = mgfs_get_info(p_cxt);
	if(p_fs)
		r = p_fs->sz_sector * p_fs->sz_unit;

	return r;
}

_u8 mgfs_flags(_mgfs_context_t *p_cxt) {
	_u8 r = 0;
	_mgfs_info_t *p_fs = mgfs_get_info(p_cxt);
	if(p_fs)
		r = p_fs->flags;

	return r;
}

