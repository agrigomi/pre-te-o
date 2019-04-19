#include "iFSIO.h"
#include "str.h"

typedef struct { // buffer record
	_u64	p_buffer;
	_u32	unit_number;
	union {
		_u32 counter;
		struct {
			_u32	_uc	:31; // usage counter
			_u32	_wf	:1;  // write flag
		}__attribute__((packed));
	}__attribute__((packed));
}__attribute__((packed)) _fs_buffer_t;

#define FIRST_MAP_SIZE	256

class cFSBuffer:public iFSBuffer {
protected:
	void init_buffer_map(_fs_context_t *p_cxt, _u32 new_size) {
		_fs_buffer_t *p_old_map = (_fs_buffer_t *)p_cxt->p_buffer_map;
		
		p_cxt->p_buffer_map = fs_mem_alloc(p_cxt, new_size * sizeof(_fs_buffer_t));
		if(p_cxt->p_buffer_map) {
			mem_set((_u8 *)p_cxt->p_buffer_map, 0, new_size * sizeof(_fs_buffer_t));

			_fs_buffer_t *p_map = (_fs_buffer_t *)p_cxt->p_buffer_map;
			for(_u32 i = 0; i < new_size; i++)
				p_map[i].unit_number = INVALID_UNIT_NUMBER;

			if(p_old_map) {
				mem_cpy((_u8 *)p_map, (_u8 *)p_old_map, p_cxt->bmap_sz * sizeof(_fs_buffer_t));
				fs_mem_free(p_cxt, p_old_map, p_cxt->bmap_sz * sizeof(_fs_buffer_t));
			}

			p_cxt->bmap_sz = new_size;
		} else
			p_cxt->p_buffer_map = p_old_map;
	}

	void *fs_mem_alloc(_fs_context_t *p_cxt, _u32 size) {
		void *r = 0;
		if(p_cxt->p_alloc)
			r = p_cxt->p_alloc(p_cxt->p_io_context, size);
		return r;
	}

	void fs_mem_free(_fs_context_t *p_cxt, void *ptr, _u32 size) {
		if(p_cxt->p_free)
			p_cxt->p_free(p_cxt->p_io_context, ptr, size);
	}

	_h_lock_ fs_lock(_fs_context_t *p_cxt, _h_lock_ hlock=0) {
		_h_lock_ r = 0;
		if(p_cxt->p_lock)
			r = p_cxt->p_lock(p_cxt->p_io_context, hlock);
		return r;
	}

	void fs_unlock(_fs_context_t *p_cxt, _h_lock_ hlock) {
		if(p_cxt->p_unlock)
			p_cxt->p_unlock(p_cxt->p_io_context, hlock);
	}

	_u32 fs_unit_read(_fs_context_t *p_cxt, _u32 nunit, void *p_buffer) {
		_u32 r = 0;
		_fs_info_t fsi;
		p_cxt->p_info(p_cxt->p_io_context, &fsi);
		if(p_cxt->p_read && nunit < fsi.nunits) {
			_u32 sector = nunit * fsi.unit_size;
			r = p_cxt->p_read(p_cxt->p_io_context, sector, fsi.unit_size, p_buffer);
		}
		return r;
	}

	_u32 fs_unit_write(_fs_context_t *p_cxt, _u32 nunit, void *p_buffer) {
		_u32 r = 0;
		_fs_info_t fsi;
		p_cxt->p_info(p_cxt->p_io_context, &fsi);
		if(p_cxt->p_write && nunit < fsi.nunits) {
			_u32 sector = nunit * fsi.unit_size;
			r = p_cxt->p_write(p_cxt->p_io_context, sector, fsi.unit_size, p_buffer);
		}
		return r;
	}

public:
	DECLARE_BASE(cFSBuffer);

	cFSBuffer() {
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	~cFSBuffer() {}
	
	_h_buffer_ alloc(_fs_context_t *p_cxt, _u32 unit_number, _h_lock_ hlock=0) {
		_h_buffer_ r = INVALID_BUFFER_HANDLE;
		_h_lock_ _hlock = fs_lock(p_cxt, hlock);

		if(!p_cxt->p_buffer_map)
			init_buffer_map(p_cxt, FIRST_MAP_SIZE);

		_fs_info_t fsi;
		p_cxt->p_info(p_cxt->p_io_context, &fsi);

	_buffer_alloc_:		
		if(p_cxt->p_buffer_map) {
			_fs_buffer_t *p_map = (_fs_buffer_t *)p_cxt->p_buffer_map;

			_u32 hfree = INVALID_BUFFER_HANDLE;
			_u32 i = 0;
			for(i = 0; i < p_cxt->bmap_sz; i++) {
				if(p_map[i].unit_number == unit_number) {
					if(p_map[i].p_buffer == 0) {
						p_map[i].p_buffer = (_u64)fs_mem_alloc(p_cxt, fsi.unit_size * fsi.sector_size);
						if(p_map[i].p_buffer) {
							if(!fs_unit_read(p_cxt,unit_number, (void *)p_map[i].p_buffer)) 
								goto _buffer_alloc_done_;
						} else 
							goto _buffer_alloc_done_;
					}

					p_map[i]._uc++;
					r = i;
					break;
				}

				if(p_map[i]._uc == 0 && p_map[i]._wf == 0 && hfree == INVALID_BUFFER_HANDLE)
					hfree = i;

				if(p_map[i].unit_number == INVALID_UNIT_NUMBER)
					break;
			}

			if(r == INVALID_BUFFER_HANDLE) {
				// not found
				if(hfree == INVALID_BUFFER_HANDLE) {
					// realloc buffer map
					init_buffer_map(p_cxt, p_cxt->bmap_sz + FIRST_MAP_SIZE);
					goto _buffer_alloc_;
				} else {
					if(p_map[hfree].p_buffer == 0)
						p_map[hfree].p_buffer = (_u64)fs_mem_alloc(p_cxt, 
									fsi.unit_size * fsi.sector_size);
					
					if(p_map[hfree].p_buffer) {
						if(fs_unit_read(p_cxt, unit_number, (void *)p_map[hfree].p_buffer)) {
							p_map[hfree].unit_number = unit_number;
							p_map[hfree]._uc++;
							r = hfree;
						}
					}
				}
			}
		}

	_buffer_alloc_done_:
		fs_unlock(p_cxt, _hlock);
		return r;
	}

	void *ptr(_fs_context_t *p_cxt, _h_buffer_ hb) {
		void *r = 0;
		if(hb < p_cxt->bmap_sz) {
			_fs_buffer_t *p_map = (_fs_buffer_t *)p_cxt->p_buffer_map;
			r = (void *)p_map[hb].p_buffer;
		}
		return r;
	}

	_u32 unit(_fs_context_t *p_cxt, _h_buffer_ hb) {
		_u32 r = INVALID_UNIT_NUMBER;
		if(hb < p_cxt->bmap_sz) {
			_fs_buffer_t *p_map = (_fs_buffer_t *)p_cxt->p_buffer_map;
			r = p_map[hb].unit_number;
		}
		return r;
	}

	void free(_fs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock=0) {
		if(hb < p_cxt->bmap_sz) {
			_h_lock_ _hlock = fs_lock(p_cxt, hlock);
			
			_fs_buffer_t *p_map = (_fs_buffer_t *)p_cxt->p_buffer_map;
			if(p_map[hb]._uc) {
				if(!p_cxt->write_back)
					flush(p_cxt, hb, _hlock);
				p_map[hb]._uc--;
			}
			
			fs_unlock(p_cxt, _hlock);
		}
	}

	void write(_fs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock=0) {
		if(hb < p_cxt->bmap_sz) {
			_h_lock_ _hlock = fs_lock(p_cxt, hlock);
			
			_fs_buffer_t *p_map = (_fs_buffer_t *)p_cxt->p_buffer_map;
			p_map[hb]._wf = 1;
			if(!p_cxt->write_back)
				flush(p_cxt, hb, _hlock);
			
			fs_unlock(p_cxt, _hlock);
		}
	}

	void flush(_fs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock=0) {
		if(hb == INVALID_BUFFER_HANDLE) {
			for(_u32 i = 0; i < p_cxt->bmap_sz; i++) {
				_h_lock_ _hlock = fs_lock(p_cxt, hlock);
				_fs_buffer_t *p_map = (_fs_buffer_t *)p_cxt->p_buffer_map;
				
				if(p_map[i]._wf) {
					if(fs_unit_write(p_cxt, 
							p_map[i].unit_number, 
							(void *)p_map[i].p_buffer))
						p_map[i]._wf = 0;
				}
				
				fs_unlock(p_cxt, _hlock);
			}
		} else {
			if(hb < p_cxt->bmap_sz) {
				_h_lock_ _hlock = fs_lock(p_cxt, hlock);
				_fs_buffer_t *p_map = (_fs_buffer_t *)p_cxt->p_buffer_map;
				if(p_map[hb]._wf) {
					if(fs_unit_write(p_cxt, 
							p_map[hb].unit_number, 
							(void *)p_map[hb].p_buffer))
						p_map[hb]._wf = 0;
				}
				
				fs_unlock(p_cxt, _hlock);
			}
		}
	}

	void cleanup(_fs_context_t *p_cxt, _h_lock_ hlock=0) {
		_h_lock_ _hlock = fs_lock(p_cxt, hlock);
		_fs_buffer_t *p_map = (_fs_buffer_t *)p_cxt->p_buffer_map;
		
		_fs_info_t fsi;
		p_cxt->p_info(p_cxt->p_io_context, &fsi);

		for(_u32 i = 0; i < p_cxt->bmap_sz; i++) {
			if(p_map[i]._wf) {
				if(fs_unit_write(p_cxt, p_map[i].unit_number, 
						(void *)p_map[i].p_buffer))
					p_map[i]._wf = 0;
			}
			
			if(p_map[i].p_buffer)
				fs_mem_free(p_cxt, (void *)p_map[i].p_buffer, fsi.unit_size * fsi.sector_size);
			
			p_map[i].p_buffer = 0;
			p_map[i]._uc = 0;
			p_map[i].unit_number = INVALID_UNIT_NUMBER;
		}
		
		fs_mem_free(p_cxt, p_map, p_cxt->bmap_sz * sizeof(_fs_buffer_t));
		p_cxt->p_buffer_map = 0;
		fs_unlock(p_cxt, _hlock);
	}
};

IMPLEMENT_BASE(cFSBuffer, "cFSBuffer", 0x0001);
