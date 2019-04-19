#include "mgfs3.h"
#include "buffer.h"
#include "bitmap.h"
#include "inode.h"
#include "str.h"

#define MAX_UNIT_ARRAY	32

_u32 _pow32(_u32 base, _u8 pow) {
	_u32 res = 1;
	int i;
	
	for(i = 0; i < pow; i++)
		res = res * base;
	
	return res;
}

bool mgfs_is_valid_inode_handle(_h_inode_ h) {
	bool r = false;
	if(h->hb_inode != INVALID_BUFFER_HANDLE)
		r = true;
	return r;
}

// return count of data unit numbers in "p_u_buffer"
_u32 mgfs_inode_list_dunit(_mgfs_context_t *p_cxt, 
		 _h_inode_ hinode,
		 _u32 first, // first needed block in file
		 _u32 count, // count of units after first
		 _u32 *p_u_buffer, // [out] array of data unit numbers
		 _h_lock_ hlock
		) {
	_u32 r = 0;
	_u32 *p_cl_num;
	_u32 i,c,max;
	_u8 l = hinode->p_inode->level;
	_u32 f = first;
	_u32 cl_sz = p_cxt->fs.sz_sector * p_cxt->fs.sz_unit;
	
	while(r < count) {
		void *p_buffer=0;
		_h_buffer_ hbuffer = INVALID_BUFFER_HANDLE; 
		_u32 page_number = INVALID_UNIT_NUMBER;

		c = MAX_LOCATION_INDEX;
		p_cl_num = (_u32 *)&(hinode->p_inode->location[0]);
		f = first + r;
		l = hinode->p_inode->level;
		
		while(l) {
			// max pointers per root at current level
			max = _pow32(cl_sz/sizeof(_u32),l);
			for(i = 0; i < c; i++) {
				if(((i+1) * max) > f) {
					if(*(p_cl_num + i) != INVALID_UNIT_NUMBER) {
						// read address cluster
						page_number = *(p_cl_num + i);
						if((hbuffer = mgfs_buffer_alloc(p_cxt, 
									page_number, hlock)) != INVALID_BUFFER_HANDLE) {
							p_buffer = mgfs_buffer_ptr(p_cxt, hbuffer);
							p_cl_num = (_u32 *)p_buffer;
							
							// count of addresses per cluster
							c = cl_sz / sizeof(_u32);
							f -= i * max;
						}/* else 
							break;*/
					}
					
					break;
				}
			}
			
			l--;
			if(l && hbuffer != INVALID_BUFFER_HANDLE) {
				mgfs_buffer_free(p_cxt, hbuffer, hlock);
				hbuffer = INVALID_BUFFER_HANDLE;
			}
		}
		
		if(f >= c) {
			if(hbuffer != INVALID_BUFFER_HANDLE) {
				mgfs_buffer_free(p_cxt, hbuffer, hlock);
				hbuffer = INVALID_BUFFER_HANDLE;
			}
			break;
		}
		
		for(; f < c; f++) {
			if(*(p_cl_num + f) != INVALID_UNIT_NUMBER) {
				*(p_u_buffer + r) = *(p_cl_num + f);
				r++;
				if(r >= count)
					break;
			} else {
				if(hbuffer != INVALID_BUFFER_HANDLE)
					mgfs_buffer_free(p_cxt, hbuffer, hlock);
				
				return r;
			}
		}
		
		if(hbuffer != INVALID_BUFFER_HANDLE) {
			mgfs_buffer_free(p_cxt, hbuffer, hlock);
			hbuffer = INVALID_BUFFER_HANDLE;
		}
	}
	
	return r;
}

// return the size of requested data in bytes
_u32 mgfs_inode_calc_data_pos(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u64 inode_offset, _u32 size, // in
			_u32 *p_block_number, _u32 *p_block_offset, _u32 *p_block_count // out
		       ) {
	_u32 r = 0;
	_mgfs_info_t *p_fs = mgfs_get_info(p_cxt);
	_u64 inode_size = mgfs_unit_size(p_cxt) * hinode->p_inode->dunits;
	
	if(p_fs && inode_offset < inode_size) {
		_u32 unit_size = p_fs->sz_sector * p_fs->sz_unit;
		*p_block_number = (_u32)(inode_offset / unit_size);
		*p_block_offset = (_u32)(inode_offset % unit_size);
		
		r = ((inode_offset + size) < inode_size) ? size : (_u32)(inode_size - inode_offset);
		*p_block_count = ((_u32)((*p_block_offset) + r)) / unit_size;
		if(((_u32)((*p_block_offset) + r)) % unit_size)
			*p_block_count = *p_block_count + 1;
	}
	
	return r;
}

_h_buffer_ mgfs_inode_read_block(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u32 block_number, _h_lock_ hlock) {
	_h_buffer_ r = INVALID_BUFFER_HANDLE;
	_u32 unit_number = INVALID_UNIT_NUMBER;
	
	if(mgfs_inode_list_dunit(p_cxt, hinode, block_number, 1, &unit_number, hlock) == 1)
		r = mgfs_buffer_alloc(p_cxt, unit_number, hlock);
	
	return r;
}

// return 0 for success
_u32 mgfs_inode_meta_open(_mgfs_context_t *p_cxt, _u8 meta_inode_number, // in
			    _h_inode_ hinode, // out
			    _h_lock_ hlock
			   ) {
	_u32 r = INVALID_INODE_NUMBER;
	_u32 block_number, block_offset, block_count;
	_u64 offset = meta_inode_number * sizeof(_mgfs_inode_record_t);
	_mgfs_inode_handle_t htemp;
	
	htemp.p_inode = &(p_cxt->fs.meta);
	htemp.hb_inode = INVALID_BUFFER_HANDLE;
	
	if(mgfs_inode_calc_data_pos(p_cxt, &htemp, offset, sizeof(_mgfs_inode_record_t),
				&block_number, &block_offset, &block_count) == sizeof(_mgfs_inode_record_t)) {
		if((hinode->hb_inode = mgfs_inode_read_block(p_cxt, &htemp, block_number, hlock)) != INVALID_BUFFER_HANDLE) {
			_u8 *p_u8_buffer = (_u8 *)mgfs_buffer_ptr(p_cxt, hinode->hb_inode);
			hinode->p_inode = (_mgfs_inode_record_t *)(p_u8_buffer + block_offset);
			hinode->number = meta_inode_number;
			r = 0;
		}
	}
	
	return r;
}

void mgfs_inode_close(_mgfs_context_t *p_cxt, _h_inode_ hinode, _h_lock_ hlock) {
	if(hinode->hb_inode != INVALID_BUFFER_HANDLE) {
		mgfs_buffer_free(p_cxt, hinode->hb_inode, hlock);
		hinode->hb_inode = INVALID_BUFFER_HANDLE;
	}
}

void mgfs_inode_update(_mgfs_context_t *p_cxt, _h_inode_ hinode, _h_lock_ hlock) {
	if(hinode->hb_inode != INVALID_BUFFER_HANDLE) {
		hinode->p_inode->mo = mgfs_timestamp(p_cxt);
		mgfs_buffer_write(p_cxt, hinode->hb_inode, hlock);
	}
}

// return 0 for success
_u32 mgfs_inode_open(_mgfs_context_t *p_cxt, _u32 inode_number, // in
		     _h_inode_ hinode, // out
		     _h_lock_ hlock
		    ) {
	_u32 r = INVALID_INODE_NUMBER;
	_u32 block_number, block_offset, block_count;
	_u64 offset = inode_number * sizeof(_mgfs_inode_record_t);
	_mgfs_inode_handle_t hmeta;
	
	if(mgfs_inode_meta_open(p_cxt, INODE_FILE_IDX, &hmeta, hlock) == 0) {
		if(mgfs_inode_calc_data_pos(p_cxt, &hmeta, offset, sizeof(_mgfs_inode_record_t),
					&block_number, &block_offset, &block_count) == sizeof(_mgfs_inode_record_t)) {
			if((hinode->hb_inode = mgfs_inode_read_block(p_cxt, &hmeta, 
							block_number, hlock)) != INVALID_BUFFER_HANDLE) {
				_u8 *p_u8_buffer = (_u8 *)mgfs_buffer_ptr(p_cxt, hinode->hb_inode);
				hinode->p_inode = (_mgfs_inode_record_t *)(p_u8_buffer + block_offset);
				hinode->number = inode_number;
				r = 0;
			}
		}
		
		mgfs_inode_close(p_cxt, &hmeta, hlock);
	}
	
	return r;
}

_u32 mgfs_inode_owner(_h_inode_ hinode) {
	_u32 r = 0;
	if(hinode->hb_inode != INVALID_BUFFER_HANDLE)
		r = hinode->p_inode->owner;

	return r;
}

_u32 mgfs_inode_read(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u64 offset, void *p_buffer, _u32 size, _h_lock_ hlock) {
	_u32 r = 0;
	_mgfs_info_t *p_fs = mgfs_get_info(p_cxt);
	
	if(p_fs && mgfs_is_valid_inode_handle(hinode)) {
		_u32 block_number, block_offset, block_count;
		_u32 _r = 0;
		if((_r = mgfs_inode_calc_data_pos(p_cxt, hinode, offset, size, &block_number, &block_offset, &block_count))) {
			_u32 unit_size = p_fs->sz_sector * p_fs->sz_unit;
			while(r < _r) {
				_u32 unit_array[MAX_UNIT_ARRAY];
				_u32 array_count = mgfs_inode_list_dunit(p_cxt, hinode, block_number, 
						(block_count < MAX_UNIT_ARRAY) ? block_count : MAX_UNIT_ARRAY, unit_array, hlock);

				if(!array_count)
					break;

				block_number += array_count;
				block_count  -= array_count;
				
				for(_u32 i = 0; i < array_count; i++) {
					_h_buffer_ hb = mgfs_buffer_alloc(p_cxt, unit_array[i], hlock);
					if(hb != INVALID_BUFFER_HANDLE) {
						_u8 *_buffer = (_u8 *)mgfs_buffer_ptr(p_cxt, hb);
						
						_u32 _b = ((((_u32)(unit_size - block_offset)) > (_r - r)) ? 
								(_r - r):
								(unit_size - block_offset));
						
						mem_cpy((_u8 *)p_buffer + r, _buffer + block_offset, _b);
						mgfs_buffer_free(p_cxt, hb, hlock);
						r += _b;
						block_offset = 0;
					} else
						goto _mgfs_inode_read_done_;
				}
			}
		}
	}
	
_mgfs_inode_read_done_:	
	
	return r;
}

// return the number of new appended blocks
static _u32 inode_append_block(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u8 level, _u32 base_unit,
				_h_inode_ hbitmap, _u32 *array, _u32 count, _h_inode_ hbshadow, _h_lock_ hlock) {
	_u32 r = 0;
	_u8 _level = level;
	_u32 _base_unit = base_unit;
	_u32 *p_unit_number = 0;
	_u32 addr_count = 0;
	_h_buffer_ hb_addr = INVALID_BUFFER_HANDLE;

	while(r < count) {
		if(_level == hinode->p_inode->level) { // enumerate the 'inode' locations
			addr_count = MAX_LOCATION_INDEX;
			p_unit_number = &(hinode->p_inode->location[0]);
		} else { // enumerate address unit
			addr_count = mgfs_unit_size(p_cxt) / sizeof(_u32);
			if((hb_addr = mgfs_buffer_alloc(p_cxt, _base_unit)) != INVALID_BUFFER_HANDLE)
				p_unit_number = (_u32 *)mgfs_buffer_ptr(p_cxt, hb_addr);
			else
				break;
		}

		_u32 i = 0; // searching for last valid position
		while(*(p_unit_number + i) != INVALID_UNIT_NUMBER && i < addr_count)
			i++;

		if(i < addr_count) {
			if(_level) {
				if(i > 0) {
					//	   |
					//	XXX+--
					// looking for free positions in one level down
					r += inode_append_block(p_cxt, hinode, _level - 1, *(p_unit_number + i - 1),
								hbitmap, array + r, count - r, hbshadow, hlock);
					if(r >= count)
						break;
				}

				// allocate address unit
				_u32 new_addr_unit = INVALID_UNIT_NUMBER;
				if(mgfs_bitmap_alloc(p_cxt, &new_addr_unit, 1, hbitmap, hlock) == 1) {
					_h_buffer_ hb_new_addr = mgfs_buffer_alloc(p_cxt, new_addr_unit, hlock);
					if(hb_new_addr != INVALID_BUFFER_HANDLE) {
						// assign the new address unit
						*(p_unit_number + i) = new_addr_unit;

						_u8 *p_new = (_u8 *)mgfs_buffer_ptr(p_cxt, hb_new_addr);
						mem_set(p_new, 0xff, mgfs_unit_size(p_cxt));
						mgfs_buffer_write(p_cxt, hb_new_addr, hlock);
						mgfs_buffer_free(p_cxt, hb_new_addr, hlock);
					} else {
						mgfs_bitmap_free(p_cxt, &new_addr_unit, 1, hbitmap, hlock);
						break;
					}

					if(hbshadow)
						mgfs_bitmap_sync(p_cxt, &new_addr_unit, 1, hbitmap, hbshadow, hlock);
				} else
					break;
			} else {
				// level 0 (contain numbers of data units)
				while(i < addr_count) {
					// assign data unit
					*(p_unit_number + i) = *(array + r);
					r++;
					hinode->p_inode->dunits++;
					if(r == count)
						break;

					i++;
				}

				if(_level != hinode->p_inode->level)
					// update the current unit
					mgfs_buffer_write(p_cxt, hb_addr, hlock);
			}
		} else {
			if(_level == hinode->p_inode->level && _level <= MAX_INODE_LEVEL) {	// switch to next level
				// allocate address unit
				_u32 new_addr_unit = INVALID_UNIT_NUMBER;
				if(mgfs_bitmap_alloc(p_cxt, &new_addr_unit, 1, hbitmap, hlock) == 1) {
					_h_buffer_ hb_new_addr = mgfs_buffer_alloc(p_cxt, new_addr_unit, hlock);
					if(hb_new_addr != INVALID_BUFFER_HANDLE) {
						_u8 *p_new = (_u8 *)mgfs_buffer_ptr(p_cxt, hb_new_addr);
						mem_set(p_new, 0xff, mgfs_unit_size(p_cxt));
						// export addresses from 'inode' to the new address unit
						p_unit_number = (_u32 *)p_new;
						for(i = 0; i < MAX_LOCATION_INDEX; i++)
							*(p_unit_number + i) = hinode->p_inode->location[i];

						mgfs_buffer_write(p_cxt, hb_new_addr, hlock);
						hinode->p_inode->level++;
						mem_set((_u8 *)hinode->p_inode->location, 0xff, sizeof(hinode->p_inode->location));
						hinode->p_inode->location[0] = new_addr_unit;
						_base_unit = new_addr_unit;
						mgfs_buffer_free(p_cxt, hb_new_addr, hlock);
					} else {
						mgfs_bitmap_free(p_cxt, &new_addr_unit, 1, hbitmap, hlock);
						break;
					}
					
					if(hbshadow)
						mgfs_bitmap_sync(p_cxt, &new_addr_unit, 1, hbitmap, hbshadow, hlock);
				} else
					break;
			} else
				break;
		}
	}

	if(hb_addr != INVALID_BUFFER_HANDLE)
		mgfs_buffer_free(p_cxt, hb_addr, hlock);

	return r;
}

// return the number of new appended blocks
static _u32 inode_append_block(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u32 nblocks, _h_lock_ hlock) {
	_u32 r = 0;
	_u32 unit_array[MAX_UNIT_ARRAY];
	_mgfs_inode_handle_t hbitmap;
	_mgfs_inode_handle_t hbshadow;
	bool bshadow = false;

	if(mgfs_inode_meta_open(p_cxt, SPACE_BITMAP_IDX, &hbitmap, hlock) == 0) {
		if(mgfs_flags(p_cxt) & MGFS_USE_BITMAP_SHADOW) {
			if(mgfs_inode_meta_open(p_cxt, SPACE_BITMAP_SHADOW_IDX, &hbshadow, hlock) == 0)
				// use bitmap shadow
				bshadow = true;
		}

		while(r < nblocks) {
			_u32 array_count = mgfs_bitmap_alloc(p_cxt, unit_array, 
							(nblocks < MAX_UNIT_ARRAY) ? nblocks : MAX_UNIT_ARRAY,
							&hbitmap, hlock);
			if(array_count) {
				_u32 _r = inode_append_block(p_cxt, hinode, 
							hinode->p_inode->level, hinode->p_inode->location[0],
							&hbitmap, unit_array, array_count, (bshadow) ? &hbshadow : 0, hlock);
				if(bshadow) // sync the space bitmap
					mgfs_bitmap_sync(p_cxt, unit_array, _r, &hbitmap, &hbshadow, hlock);
				
				if(_r)
					r += _r;
				else
					break;
			} else
				break;
		}

		if(bshadow)
			mgfs_inode_close(p_cxt, &hbshadow, hlock);
		
		mgfs_inode_close(p_cxt, &hbitmap, hlock);
	}

	return r;
}

// append initialized blocks to inode, and return the number of new appended blocks
_u32 mgfs_inode_append_block(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u32 nblocks, _u8 pattern, _h_lock_ hlock) {
	_u32 r = 0;

	_u32 unit_array[MAX_UNIT_ARRAY];
	_mgfs_inode_handle_t hbitmap;
	_mgfs_inode_handle_t hbshadow;
	bool bshadow = false;

	if(mgfs_inode_meta_open(p_cxt, SPACE_BITMAP_IDX, &hbitmap, hlock) == 0) {
		if(mgfs_flags(p_cxt) & MGFS_USE_BITMAP_SHADOW) {
			if(mgfs_inode_meta_open(p_cxt, SPACE_BITMAP_SHADOW_IDX, &hbshadow, hlock) == 0)
				// use bitmap shadow
				bshadow = true;
		}

		while(r < nblocks) {
			_u32 array_count = mgfs_bitmap_alloc(p_cxt, unit_array, 
							(nblocks < MAX_UNIT_ARRAY) ? nblocks : MAX_UNIT_ARRAY,
							&hbitmap, hlock);
			if(array_count) {
				for(_u32 i = 0; i < array_count; i++) {
					_h_buffer_ hbunit = mgfs_buffer_alloc(p_cxt, unit_array[i], hlock);
					if(hbunit != INVALID_BUFFER_HANDLE) {
						_u8 *ptr = (_u8 *)mgfs_buffer_ptr(p_cxt, hbunit);
						mem_set(ptr, pattern, mgfs_unit_size(p_cxt));
						mgfs_buffer_write(p_cxt, hbunit, hlock);
						mgfs_buffer_free(p_cxt, hbunit, hlock);
					}
				}

				_u32 _r = inode_append_block(p_cxt, hinode, 
							hinode->p_inode->level, hinode->p_inode->location[0],
							&hbitmap, unit_array, array_count, (bshadow) ? &hbshadow : 0, hlock);
				if(bshadow) // sync the space bitmap
					mgfs_bitmap_sync(p_cxt, unit_array, _r, &hbitmap, &hbshadow, hlock);
				
				if(_r)
					r += _r;
				else
					break;
			} else
				break;
		}

		if(bshadow)
			mgfs_inode_close(p_cxt, &hbshadow, hlock);
		
		mgfs_inode_close(p_cxt, &hbitmap, hlock);
	}

	return r;
}

_u32 mgfs_inode_write(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u64 offset, void *p_buffer, _u32 size, _h_lock_ hlock) {
	_u32 r = 0;
	_mgfs_info_t *p_fs = mgfs_get_info(p_cxt);
	
	if(p_fs && mgfs_is_valid_inode_handle(hinode)) {		
		_u32 unit_size = p_fs->sz_sector * p_fs->sz_unit;
		// calculate the absolute inode size in bytes
		_u64 inode_size = hinode->p_inode->dunits * unit_size;
		
		if((offset + size) > inode_size) {
			_u32 add_blocks = (_u32)( (((offset + size) - inode_size) % unit_size) ?
						  ((((offset + size) - inode_size) / unit_size) + 1):
						  (((offset + size) - inode_size) / unit_size)
						);
			
			// add blocks to inode
			inode_append_block(p_cxt, hinode, add_blocks, hlock);
			inode_size += hinode->p_inode->dunits * unit_size;
		}

		if((offset + size) <= inode_size) {
			_u32 block_number, block_offset, block_count;
			_u32 _r = 0;
			if((_r = mgfs_inode_calc_data_pos(p_cxt, hinode, offset, size, 
								&block_number, &block_offset, &block_count))) {
				while(r < _r) {
					_u32 unit_array[MAX_UNIT_ARRAY];
					_u32 array_count = mgfs_inode_list_dunit(p_cxt, hinode, block_number, 
							(block_count < MAX_UNIT_ARRAY) ? block_count : MAX_UNIT_ARRAY, 
							unit_array, hlock);

					if(!array_count)
						break;

					block_number += array_count;
					block_count  -= array_count;
					
					for(_u32 i = 0; i < array_count; i++) {
						_h_buffer_ hb = mgfs_buffer_alloc(p_cxt, unit_array[i], hlock);
						if(hb != INVALID_BUFFER_HANDLE) {
							_u8 *_buffer = (_u8 *)mgfs_buffer_ptr(p_cxt, hb);
							
							_u32 _b = ((((_u32)(unit_size - block_offset)) > (_r - r)) ? 
									(_r - r):
									(unit_size - block_offset));
							
							mem_cpy(_buffer + block_offset, (_u8 *)p_buffer + r, _b);
							r += _b;
							block_offset = 0;
							mgfs_buffer_write(p_cxt, hb, hlock);
							mgfs_buffer_free(p_cxt, hb, hlock);
						} else
							goto _mgfs_inode_write_done_;
					}
				}
			}
		}
	}

_mgfs_inode_write_done_:
	// update inode
	if((offset + r) > hinode->p_inode->sz)
		hinode->p_inode->sz = (offset + r);
	
	mgfs_inode_update(p_cxt, hinode, hlock);

	return r;
}

// return inode number
static _u32 inode_alloc(_mgfs_context_t *p_cxt, _h_lock_ hlock) {
	_u32 r = INVALID_INODE_NUMBER;
	_mgfs_inode_handle_t h_ibitmap;	// inode bitmap
	_mgfs_inode_handle_t h_isbitmap;// inode bitmap shadow
	bool use_shadow = false;

	if(mgfs_inode_meta_open(p_cxt, INODE_BITMAP_IDX, &h_ibitmap, hlock) == 0) {
		if(mgfs_flags(p_cxt) & MGFS_USE_BITMAP_SHADOW) {
			if(mgfs_inode_meta_open(p_cxt, INODE_BITMAP_SHADOW_IDX, &h_isbitmap, hlock) == 0)
				// use inode bitmap shadow
				use_shadow = true;
		}

		if(!mgfs_bitmap_free_state(p_cxt, &h_ibitmap, hlock)) {
			// extend inode bitmap
			_h_lock_ _lock = mgfs_lock(p_cxt, hlock);

			if(mgfs_inode_append_block(p_cxt, &h_ibitmap, 1, 0, _lock) == 1) {
				h_ibitmap.p_inode->sz += mgfs_unit_size(p_cxt);
				mgfs_inode_update(p_cxt, &h_ibitmap, _lock);
				if(use_shadow) {
					if(mgfs_inode_append_block(p_cxt, &h_isbitmap, 1, 0, _lock)) {
						h_isbitmap.p_inode->sz += mgfs_unit_size(p_cxt);
						mgfs_inode_update(p_cxt, &h_isbitmap, _lock);
					}
				}
			}

			mgfs_unlock(p_cxt, _lock);
		}

		_u32 new_inode = INVALID_INODE_NUMBER;
		if(mgfs_bitmap_alloc(p_cxt, &new_inode, 1, &h_ibitmap, hlock) == 1)
			r = new_inode;

		if(use_shadow) {
			if(r != INVALID_INODE_NUMBER)
				mgfs_bitmap_sync(p_cxt, &new_inode, 1, &h_ibitmap, &h_isbitmap, hlock);
			mgfs_inode_close(p_cxt, &h_isbitmap, hlock);
		}

		mgfs_inode_close(p_cxt, &h_ibitmap, hlock);
	}

	return r;
}

// return 0 for success
static _u32 inode_free(_mgfs_context_t *p_cxt, _u32 inode_number, _h_lock_ hlock) {
	_u32 r = INVALID_INODE_NUMBER;
	_mgfs_inode_handle_t h_ibitmap;	// inode bitmap
	_mgfs_inode_handle_t h_isbitmap;// inode bitmap shadow
	bool use_shadow = false;

	if(mgfs_inode_meta_open(p_cxt, INODE_BITMAP_IDX, &h_ibitmap, hlock) == 0) {
		if(mgfs_flags(p_cxt) & MGFS_USE_BITMAP_SHADOW) {
			if(mgfs_inode_meta_open(p_cxt, INODE_BITMAP_SHADOW_IDX, &h_isbitmap, hlock) == 0)
				use_shadow = true;
		}

		if(mgfs_bitmap_free(p_cxt, &inode_number, 1, &h_ibitmap, hlock) == 1)
			r = 0;

		if(use_shadow) {
			mgfs_bitmap_sync(p_cxt, &inode_number, 1, &h_ibitmap, &h_isbitmap, hlock);
			mgfs_inode_close(p_cxt, &h_isbitmap, hlock);
		}

		mgfs_inode_close(p_cxt, &h_ibitmap, hlock);
	}

	return r;
}

// return inode number for success, else INVALID_INODE_NUMBER
_u32 mgfs_inode_create(_mgfs_context_t *p_cxt, _u16 flags,  _u32 owner_id, // in
		       _h_inode_ hinode, // out
		       _h_lock_ hlock
		      ) {
	_u32 r = INVALID_INODE_NUMBER;

	if((r = inode_alloc(p_cxt, hlock)) != INVALID_INODE_NUMBER) {
		_mgfs_inode_handle_t hif_meta;	// inode container
		_u64 inode_offset = r * sizeof(_mgfs_inode_record_t);
		
		if(mgfs_inode_meta_open(p_cxt, INODE_FILE_IDX, &hif_meta, hlock) == 0) {
			_mgfs_inode_record_t ir;

			mem_set((_u8 *)&ir, 0, sizeof(_mgfs_inode_record_t));
			ir.flags = flags;
			ir.cr = mgfs_timestamp(p_cxt);
			ir.owner = owner_id;

			for(_u8 i = 0; i < MAX_LOCATION_INDEX; i++)
				ir.location[i] = INVALID_UNIT_NUMBER;
			
			_h_lock_ _lock = mgfs_lock(p_cxt, hlock);

			if(mgfs_inode_write(p_cxt, &hif_meta, inode_offset, &ir, sizeof(_mgfs_inode_record_t), _lock) !=
										sizeof(_mgfs_inode_record_t)) {
				inode_free(p_cxt, r, _lock);
				r = INVALID_INODE_NUMBER;
			} else {
				if(mgfs_inode_open(p_cxt, r, hinode, _lock) != 0) {
					inode_free(p_cxt, r, _lock);
					r = INVALID_INODE_NUMBER;
				}
			}

			mgfs_unlock(p_cxt, _lock);
			mgfs_inode_close(p_cxt, &hif_meta, hlock);
		}
	}

	return r;
}

static _u32 inode_remove_block(_mgfs_context_t *p_cxt, _h_inode_ hinode, _h_inode_ hbitmap, _h_inode_ hsbitmap,
				_u8 level, _u32 base_unit, _u32 nblocks, bool *empty, _h_lock_ hlock=0) {
	_u32 r = 0;
	_u32 *p_unit_number = 0;
	_u32 unit_size = mgfs_unit_size(p_cxt);
	_h_buffer_ hbase = INVALID_BUFFER_HANDLE;
	_u32 addr_count = 0;

	if(level == hinode->p_inode->level) {
		p_unit_number = &hinode->p_inode->location[0];
		addr_count = MAX_LOCATION_INDEX;
	} else {
		if((hbase = mgfs_buffer_alloc(p_cxt, base_unit, hlock)) != INVALID_BUFFER_HANDLE) {
			p_unit_number = (_u32 *)mgfs_buffer_ptr(p_cxt, hbase);
			addr_count = unit_size / sizeof(_u32);
		}
	}

	_u32 addr_idx = addr_count;
	while(addr_idx && r < nblocks && p_unit_number) {
		addr_idx--;
		
		_u32 _base_unit = *(p_unit_number + addr_idx);
		if(_base_unit != INVALID_UNIT_NUMBER) {
			if(level) {
				bool _empty = false;
				_u32 _r = inode_remove_block(p_cxt, hinode, hbitmap, hsbitmap, level - 1,
							_base_unit, nblocks - r, &_empty , hlock);
				hinode->p_inode->dunits -= _r;
				r += _r;

				if(_empty) {
					mgfs_bitmap_free(p_cxt, &_base_unit, 1, hbitmap, hlock);
					if(hsbitmap)
						mgfs_bitmap_sync(p_cxt, &_base_unit, 1, hbitmap, hsbitmap, hlock);

					*(p_unit_number + addr_idx) = INVALID_INODE_NUMBER;
				}
			} else {// remove data blocks
				_u32 _count = 0;
				_u32 _r = r;
				while(addr_idx && _r < nblocks) {
					if(*(p_unit_number + addr_idx) != INVALID_UNIT_NUMBER) {
						_count++;
						_r++;
					}
					
					addr_idx--;
				}
		
				_u32 *_base = (p_unit_number + addr_idx);
				_r = mgfs_bitmap_free(p_cxt, _base, _count+1, hbitmap, hlock);
				hinode->p_inode->dunits -= _r;
				if(hsbitmap)
					mgfs_bitmap_sync(p_cxt, _base, _r, hbitmap, hsbitmap, hlock);
				
				r += _r;
				for(_u32 i = 0; i < _count+1; i++)
					*(p_unit_number + i) = INVALID_UNIT_NUMBER;
			}
		}
	}

	if(addr_idx == 0)
		*empty = true;

	if(hbase != INVALID_BUFFER_HANDLE) {
		if(r)
			mgfs_buffer_write(p_cxt, hbase, hlock);
		mgfs_buffer_free(p_cxt, hbase, hlock);
	}

	return r;
}

// return 0 for success
_u32 mgfs_inode_delete(_mgfs_context_t *p_cxt, _h_inode_ hinode, _h_lock_ hlock) {
	_u32 r = __ERR;
	_mgfs_inode_handle_t hbitmap;
	_mgfs_inode_handle_t hbshadow;
	bool bshadow = false;

	if(hinode->p_inode->lc) {
		hinode->p_inode->lc--;
		mgfs_inode_update(p_cxt, hinode, hlock);
	} else {
		if(mgfs_inode_meta_open(p_cxt, SPACE_BITMAP_IDX, &hbitmap, hlock) == 0) {
			if(mgfs_flags(p_cxt) & MGFS_USE_BITMAP_SHADOW) {
				if(mgfs_inode_meta_open(p_cxt, SPACE_BITMAP_SHADOW_IDX, &hbshadow, hlock) == 0)
					// use bitmap shadow
					bshadow = true;
			}

			bool  empty = false;
			_u32 dunits = hinode->p_inode->dunits;
			_u32 removed = inode_remove_block(p_cxt, hinode, &hbitmap, 
							(bshadow)?&hbshadow:0, hinode->p_inode->level,
							hinode->p_inode->location[0], dunits, &empty, hlock);

			if(empty && dunits == removed) {
				if(inode_free(p_cxt, hinode->number, hlock) == 0) {
					hinode->p_inode->flags |= MGFS_DELETED;
					r = 0;
				}
			}

			hinode->p_inode->mo = mgfs_timestamp(p_cxt);
			hinode->p_inode->sz = hinode->p_inode->dunits * mgfs_unit_size(p_cxt);
			mgfs_inode_update(p_cxt, hinode, hlock);

			if(bshadow)
				mgfs_inode_close(p_cxt, &hbshadow, hlock);

			mgfs_inode_close(p_cxt, &hbitmap, hlock);
		}
	}

	return r;
}

// truncate inode to size passed in 'new_size' parameter and return the new inode size
_u64 mgfs_inode_truncate(_mgfs_context_t *p_cxt, _h_inode_ hinode, _u64 new_size, _h_lock_ hlock) {
	_u64 r = hinode->p_inode->sz;
	_mgfs_inode_handle_t hbitmap;
	_mgfs_inode_handle_t hbshadow;
	bool bshadow = false;

	if(new_size < r) {
		if(mgfs_inode_meta_open(p_cxt, INODE_BITMAP_IDX, &hbitmap, hlock) == 0) {
			if(mgfs_flags(p_cxt) & MGFS_USE_BITMAP_SHADOW) {
				if(mgfs_inode_meta_open(p_cxt, SPACE_BITMAP_SHADOW_IDX, &hbshadow, hlock) == 0)
					// use bitmap shadow
					bshadow = true;
			}

			_u32 bcount = (r - new_size) / mgfs_unit_size(p_cxt);
			bcount -= ((r - new_size) % mgfs_unit_size(p_cxt)) ? 1 : 0;
			bool empty = false;
			_u32 removed = inode_remove_block(p_cxt, hinode, &hbitmap, 
						(bshadow)?&hbshadow:0, hinode->p_inode->level,
						hinode->p_inode->location[0], bcount, &empty, hlock);
			if(removed == bcount) 
				r = hinode->p_inode->sz = new_size;
			else {
				r -= removed * mgfs_unit_size(p_cxt);
				hinode->p_inode->sz = r;
			}
			
			if(bshadow)
				mgfs_inode_close(p_cxt, &hbshadow, hlock);

			mgfs_inode_close(p_cxt, &hbitmap, hlock);
			mgfs_inode_update(p_cxt, hinode, hlock);
		}
	}

	return r;
}

