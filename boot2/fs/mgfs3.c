#include "../boot.h"
#include "mgfs3.h"
#include "lib.h"

#define SECTOR_SIZE	0x200
#define FSSB_SECTOR	1
#define FSSB_OFFSET	0x100

extern _u8 *_fs_super_block_;
_mgfs_info_t *_g_p_mgfs3_ = 0;

_u8 fs_superblock_sector(void) {
	return FSSB_SECTOR;
}

_u8 fs_superblock_offset(void) {
	return (_u8)FSSB_OFFSET;
}

_u16 sector_size(void) {
	_u16 r = SECTOR_SIZE;

	if(_g_p_mgfs3_)
		r = _g_p_mgfs3_->sz_sector;

	return r;
}

_u16 __attribute__((optimize("O0"))) get_unit_size(void) {
	return _g_p_mgfs3_->sz_sector * _g_p_mgfs3_->sz_unit;
}

void fs_init(void) {
	_g_p_mgfs3_ = (_mgfs_info_t *)&_fs_super_block_;
}

// return 0 for success
static _u8 __attribute__((optimize("O0"))) read_dev_unit(_u32 unit, _u32 dst) {
	_u8 r = 0xff;
	_u32 count = _g_p_mgfs3_->sz_unit;
	_u32 sector = unit * count;
	
	if(read_sector(sector, count, dst) == count)
		r = 0;
	
	return r;
}

static _u32 __attribute__((optimize("O0"))) inode_list_dunit(_mgfs_inode_record_t *p_inode, 
				_u32 first,
				_u32 count,
				_u32 *p_u_buffer,
				void *p_io_buffer
			    ) {
	_u32 r = 0;

	_u32 *p_cl_num;
	_u32 i,c,max;
	_u8 l = p_inode->level;
	_u32 f = first;
	_u32 cl_sz = get_unit_size();

	while(r < count) {
		_u32 page_number = INVALID_UNIT_NUMBER;

		c = MAX_LOCATION_INDEX;
		p_cl_num = (_u32 *)&(p_inode->location[0]);
		f = first + r;
		l = p_inode->level;

		while(l) {
			// max pointers per root at current level
			max = _pow32(cl_sz/sizeof(_u32),l);
			for(i = 0; i < c; i++) {
				if(((i+1) * max) > f) {
					if(*(p_cl_num + i) != INVALID_UNIT_NUMBER) {
						// read address cluster
						page_number = *(p_cl_num + i);
						if(read_dev_unit(page_number, (_u32)p_io_buffer) == 0) {
							p_cl_num = (_u32 *)p_io_buffer;

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
		}
		
		if(f >= c)
			break;
		
		for(; f < c; f++) {
			if(*(p_cl_num + f) != INVALID_UNIT_NUMBER) {
				*(p_u_buffer + r) = *(p_cl_num + f);
				r++;
				if(r >= count)
					break;
			} else
				return r;
		}
	}

	return r;
}

static _u8 __attribute__((optimize("O0"))) read_file_block(_mgfs_inode_record_t *p_inode, _u32 block, void *dst) {
	_u8 r = FSERR;
	_u32 unit = INVALID_UNIT_NUMBER;

	if(inode_list_dunit(p_inode, block, 1, &unit, dst) == 1) {
		r = read_dev_unit(unit, (_u32)dst);
	}

	return r;
}

static _u8 __attribute__((optimize("O0"))) get_metafile_info(_u8 meta_idx, void *file_info) {
	_u8 r = FSERR;
	_u16 unit_size = get_unit_size();
	void *p_io_buffer = alloc_io_buffer(unit_size);

	if(p_io_buffer) {
		if(read_file_block(&_g_p_mgfs3_->meta, 0, p_io_buffer) == 0) {
			_u32 offset = meta_idx * sizeof(_mgfs_inode_record_t);
			mem_cpy((_u8 *)file_info, (_u8 *)((_u8 *)p_io_buffer + offset), sizeof(_mgfs_inode_record_t));
			r = 0;
		}

		free_io_buffer(p_io_buffer);
	}

	return r;
}

static _u8 __attribute__((optimize("O0"))) get_inode(_u32 inode_num, _mgfs_inode_record_t *p_inode) {
	_u8 r = FSERR;
	_mgfs_inode_record_t meta_inode;
	
	if(get_metafile_info(INODE_FILE_IDX, &meta_inode) == 0) {
		_u16 unit_size = get_unit_size();
		_u32 offset = inode_num * sizeof(_mgfs_inode_record_t);
		_u32 block = offset / unit_size;
		_u32 block_offset = offset % unit_size;

		_u8 *p_io = alloc_io_buffer(unit_size);
		if(p_io) {
			if(read_file_block(&meta_inode, block, p_io) == 0) {
				mem_cpy((_u8 *)p_inode, p_io + block_offset, sizeof(_mgfs_inode_record_t));
				r = 0;
			}

			free_io_buffer(p_io);
		}
	}

	return r;
}

static _u8 __attribute__((optimize("O0"))) get_root_inode(_mgfs_inode_record_t *p_inode) {
	return get_inode(_g_p_mgfs3_->root_inode, p_inode);
}

static _mgfs_dir_record_t * __attribute__((optimize("O0"))) find_dentry(_str_t fname, _u16 sz_fname, _mgfs_inode_record_t *p_idir) {
	_mgfs_dir_record_t *r = 0;
	_u16 unit_size = get_unit_size();
	void *p_io = alloc_io_buffer(unit_size);

	if(p_io) {
		_u32 block = 0;
		
		while(block < p_idir->dunits) {
			if(read_file_block(p_idir, block, p_io) == 0) {
				_u8 *p = (_u8 *)p_io;

				while(p < ((_u8 *)p_io + unit_size)) {
					_mgfs_dir_record_t *p_dentry = (_mgfs_dir_record_t *)p;

					if(mem_cmp((_u8 *)(p_dentry + 1), (_u8 *)fname, sz_fname) == 0) {
						r = p_dentry;
						block = p_idir->dunits;
						break;
					}

					p += p_dentry->record_size;
				}
			}

			block++;
		}

		free_io_buffer(p_io);
	}

	return r;
}

static _u32 __attribute__((optimize("O0"))) get_dentry_inode_number(_str_t fpath, _mgfs_inode_record_t *p_iroot) {
	_u32 r = INVALID_INODE_NUMBER;
	_u16 sz_path = str_len(fpath) + 1;
	_mgfs_inode_record_t inode;
	_u16 i = 0, j = 0;
	_u32 inode_number = r;

	mem_cpy((_u8 *)&inode, (_u8 *)p_iroot, sizeof(_mgfs_inode_record_t));

	for(i = 0; i < sz_path; i++) {
		if(*(fpath + i) == '/' || *(fpath + i) == 0 || *(fpath + i) < 0x20) {
			if(i - j) {
				_mgfs_dir_record_t *p_dentry = find_dentry(fpath + j, i - j, &inode);
				if(p_dentry) {
					inode_number = p_dentry->inode_number;
					if(get_inode(inode_number, &inode) != 0)
						break;
				} else
					break;
			}

			j = i + 1;
		}
	}

	if(i == sz_path)
		r = inode_number;

	return r;
}

_u8 __attribute__((optimize("O0"))) fs_get_file_info(_str_t fpath, void *finfo) {
	_u8 r = FSERR;

	_mgfs_inode_record_t root_inode;
	if(get_root_inode(&root_inode) == 0) {
		_u32 inode_number = get_dentry_inode_number(fpath, &root_inode);

		if(inode_number != INVALID_INODE_NUMBER)
			r = get_inode(inode_number, (_mgfs_inode_record_t *)finfo);
	}

	return r;
}

_u32 __attribute__((optimize("O0"))) fs_get_file_blocks(void *finfo) {
	_mgfs_inode_record_t *p_inode = (_mgfs_inode_record_t *)finfo;
	return p_inode->dunits;
}

_u32 __attribute__((optimize("O0"))) fs_get_file_size(void *finfo) {
	_mgfs_inode_record_t *p_inode = (_mgfs_inode_record_t *)finfo;
	return (_u32)p_inode->sz;
}

_u8 __attribute__((optimize("O0"))) fs_read_file_block(void *finfo, _u32 block, _u32 dst_address) {
	_u8 r = FSERR;
	_u32 unit = INVALID_UNIT_NUMBER;
	void *p_io_buffer = alloc_io_buffer(get_unit_size());
	
	if(p_io_buffer) {
		if(inode_list_dunit((_mgfs_inode_record_t *)finfo, block, 1,&unit, p_io_buffer) == 1) {
			r = read_dev_unit(unit, dst_address);
		}

		free_io_buffer(p_io_buffer);
	}

	return r;	
}

// return number of file blocks
_u32 __attribute__((optimize("O0"))) fs_read_file(void *finfo, _u32 dst_addr) {
	_u32 unit_array[MAX_FILE_BLOCKS];
	_mgfs_inode_record_t *p_inode = (_mgfs_inode_record_t *)finfo;
	_u32 unit_count = 0;
	_u16 unit_size = get_unit_size();
	void *p_io_buffer = alloc_io_buffer(unit_size);

	mem_set((_u8 *)unit_array, 0, sizeof(unit_array));

	if(p_io_buffer) {
		_u32 addr = dst_addr;
		_u32 block_idx = 0;

		unit_count = inode_list_dunit(p_inode, 0, MAX_FILE_BLOCKS, unit_array, p_io_buffer);
		while(block_idx < unit_count) {
			if(read_dev_unit(unit_array[block_idx], addr) == 0) {
				addr += unit_size;
				block_idx++;
				print_char('.');
			} else
				break;
		}

		free_io_buffer(p_io_buffer);
	}

	return unit_count;
}

void fs_get_serial(_u8 *bserial, _u32 sz_bserial) {
	mem_cpy(bserial, _g_p_mgfs3_->serial, sz_bserial);
}
