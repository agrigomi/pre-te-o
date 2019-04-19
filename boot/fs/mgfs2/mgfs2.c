// mgfs.c
// boot version of MGFS2 file system (read only)

#include "stub.h"
#include "lib.h"
#include "mgfs.h"

extern _boot_stub_t _g_stub;

typedef struct {
 	_u8			dev;	// bios device number
	_mgfs_info_t 		sb;	// mgfs superblock
	_mgfs_inode_record_t 	ii;	// file inode	
	_u8 			*io;	// buffer for i/o operations 
	_u16 			sz_io;	// size of i/o buffer ( min. one block) see 'fs_get_block_size'
}_file_info_t;

// return sizeof superblock in p_sb
static _u16 _get_superblock(_u8 dev,_mgfs_info_t *p_sb, _u8 *io_buffer) {
 	_u16 r = 0;
	_u8 *lb = io_buffer;
	_lba_t lba;
	_u32 i = 0;
	_device_parameters_t dp;
	_u16 sz_sector = 512;
	
	lba._qw = 0;
	
	if(!get_device_parameters(dev,&dp))
		sz_sector = dp.sz_sector;
	
	if(!_g_stub._dev.read(&lba,dev,1,lb)) {
		for(i = 0; i < sz_sector; i++) {
			if(*(lb + i + 2) == MARKER_B0 && *(lb + i + 3) == MARKER_B1) {
				r = sizeof(_mgfs_info_t);
				mem_cpy((_u8 *)p_sb, lb + i, r);
				break;
			}
		}
	}
	
	return r;
}

static _u8 read_cluster(_mgfs_info_t *_sb, _umax cluster, _u8 dev, _u8 *_ptr) {
	_lba_t lba;
	_u8 spc = 0;	// sectors per cluster
	_umax sector = 0; 
	
	spc = _sb->sz_unit;	// get num. of sectors per cluster
	sector = cluster * spc;	// calc sector number
	
	// It shall be fixed in 64 bit version, but now we use 32 bit cluster address 
	lba._lw = sector;
	lba._hw = 0;
	////////////////////////
	
	return _g_stub._dev.read(&lba,dev,spc,_ptr);
}

// read one cluster from file and return 0 if success
static _u8 file_read_cluster(_mgfs_info_t *_sb, _umax cluster, _mgfs_inode_record_t *p_ii, _u8 dev, _u8 *_ptr) {
	_u8 l = p_ii->level;
	_umax *p_cl_num = (_umax *)&(p_ii->location[0]);
	_u32 count = (_sb->sz_addr == 4) ? 8 : 4;
	_u32 cl_sz = _sb->sz_sector * _sb->sz_unit;
	_u32 i,max;
	_umax f = cluster;
	_u8 *ptr;
	
	while(l) {
		max = _pow32(cl_sz/_sb->sz_addr,l); // max pointers per root at current level
		for(i = 0; i < count; i++) {
			if(((i+1) * max) > f) {
				ptr = (_u8 *)p_cl_num;
				ptr += i * _sb->sz_addr;
				p_cl_num = (_umax *)ptr;
				
				if(*p_cl_num != INVALID_CLUSTER_NUMBER) {
					// read address cluster
					read_cluster(_sb,*p_cl_num,dev,_ptr);
					p_cl_num = (_umax *)&_ptr[0];
					
					// count of addresses per cluster
					count = cl_sz / _sb->sz_addr; 
					f -= i * max;
				}
				
				break;
			}
		}
		
		l--;
	}
	
	ptr = (_u8 *)p_cl_num;
	ptr += f * _sb->sz_addr;
	p_cl_num = (_umax *)ptr;
	
	return read_cluster(_sb,*p_cl_num,dev,_ptr);
}

// return 0 if success
static _u8 get_meta(_mgfs_info_t *_sb, _u8 dev, _mgfs_inode_record_t *p_iroot, 
		    _mgfs_inode_record_t *p_iinodes, _u8 *p_io) {
	_u8 r = 1;
	
	if((r = file_read_cluster(_sb, 0, &_sb->meta, dev, p_io)) == 0) {
		_mgfs_inode_record_t *pi = (_mgfs_inode_record_t *)p_io;
		mem_cpy((_u8 *)p_iroot, (_u8 *)(pi + SB_I_ROOT), sizeof(_mgfs_inode_record_t));
		mem_cpy((_u8 *)p_iinodes, (_u8 *)(pi + SB_I_NODES), sizeof(_mgfs_inode_record_t));
	}
	
	return r;
}

static _mgfs_dir_record_t *search_dentry(_mgfs_info_t *_sb, _u8 dev, _str_t name, 
					 _u32 sz_name, _mgfs_inode_record_t *p_inode, _u8 *p_io) {
	_mgfs_dir_record_t *r = 0;
	_mgfs_dir_record_t *_r = (_mgfs_dir_record_t *)p_io;
	_umax cluster = 0;
	_u32 count = (_sb->sz_sector * _sb->sz_unit) / sizeof(_mgfs_dir_record_t);
	
	while(file_read_cluster(_sb,cluster,p_inode,dev,p_io) == 0) {
		_u32 i = 0;
		for(i = 0; i < count; i++) {
			_u8 *p_name = (((_u8 *)(_r + i)) + _sb->sz_addr);
			if((_r + i)->inode_num != INVALID_INODE_NUMBER && str_len((_str_t)p_name) == sz_name) {
				if(mem_cmp(p_name,(_u8 *)name,sz_name) == 0) {
					r = (_r + i);
					goto _search_dentry_done;
				}
			}
		}
		
		cluster++;
	}

_search_dentry_done:	
	return r;
}

// return 0 if success
static _mgfs_dir_record_t * get_dentry(_mgfs_info_t *_sb, _u8 dev, _str_t path, 
					_mgfs_inode_record_t *p_iroot, _mgfs_inode_record_t *p_iinodes, _u8 *p_io) {
	_mgfs_dir_record_t *r = 0;
	_u16 sz_path = str_len(path) + 1;
	_u16 i = 0, j = 0;
	_mgfs_dir_record_t *p_dentry=0;
	_mgfs_inode_record_t inode;
	
	mem_cpy((_u8 *)&inode, (_u8 *)p_iroot, sizeof(_mgfs_inode_record_t));
	
	for(i = 0; i < sz_path; i++) {
		if(*(path + i) == '/' || *(path + i) == 0 || *(path + i) == ';' || *(path + i) < 0x20) {
			if(i - j) {
				// searching for directory entry
				p_dentry = search_dentry(_sb, dev, (path + j), i - j, &inode, p_io);
				if(p_dentry) {
					if(*(path + i) == '/') {
						// calculate the inode position 
						_umax cluster = (p_dentry->inode_num * sizeof(_mgfs_inode_record_t)) / (_sb->sz_sector * _sb->sz_unit);
						_umax offset  = (p_dentry->inode_num * sizeof(_mgfs_inode_record_t)) % (_sb->sz_sector * _sb->sz_unit);
						
						if(file_read_cluster(_sb,cluster,p_iinodes,dev,p_io) == 0) {
							mem_cpy((_u8 *)&inode, (p_io + offset), sizeof(_mgfs_inode_record_t));
						} else {
							p_dentry = 0;
							break;
						}
					}
				} else {
					_g_stub._display.text_out("Can't find entry '",18);
					_g_stub._display.text_out(path+j,i-j);
					_g_stub._display.text_out("'\x0d\x0a",3);
					break;
				}	
			}
			
			j = i + 1;
		}
	}
	
	r = p_dentry;
	
	return r;
}

static _u32 get_inode(_mgfs_info_t *_sb, _u8 dev, _str_t path, 
		      _mgfs_inode_record_t *p_ii, _u8 *_ptr) {
	_u32 r = 0;
	_mgfs_inode_record_t iroot;
	_mgfs_inode_record_t iinodes;
	
	if(get_meta(_sb, dev, &iroot, &iinodes, _ptr) == 0) {
		_mgfs_dir_record_t *dr;
		
		if((dr = get_dentry(_sb, dev, path, &iroot, &iinodes, _ptr))) {
			// calculate the inode position 
			_umax cluster = (dr->inode_num * sizeof(_mgfs_inode_record_t)) / (_sb->sz_sector * _sb->sz_unit);
			_umax offset  = (dr->inode_num * sizeof(_mgfs_inode_record_t)) % (_sb->sz_sector * _sb->sz_unit);
			
			if(file_read_cluster(_sb,cluster,&iinodes,dev,_ptr) == 0) {
				mem_cpy((_u8 *)p_ii, (_ptr + offset), sizeof(_mgfs_inode_record_t));
				r = sizeof(_mgfs_inode_record_t);
			}
		}
	}
	
	return r;
}


	// public filesystem interface

// read file information structure in 'p_info_buffer' and return num of bytes in p_info_buffer
_u16 fs_get_file_info(_u8 dev,			// bios device number
		      _str_t p_file_name,		// full path to file
		      _u8 *p_info_buffer,	// file information buffer needed by other file operations
		      _u16 sz_buffer,		// size of info buffer ( this function return 0 if 'sz_buffer' is to small )
		      _u8 *p_io_buffer,		// buffer for i/o operations 
		      _u16 sz_io_buffer		// size of i/o buffer ( min. one block) see 'fs_get_block_size'
		      ) {
 	_u16 r = 0;
 	_file_info_t *p_fi = (_file_info_t *)p_info_buffer;
	
	if(sz_buffer >= sizeof(_file_info_t)) {
		if(_get_superblock(dev,&p_fi->sb,p_io_buffer) == sizeof(_mgfs_info_t)) {
		 	if(sz_io_buffer >= p_fi->sb.sz_sector * p_fi->sb.sz_unit) {
	 			if(get_inode(&p_fi->sb,dev,p_file_name,&p_fi->ii,p_io_buffer)) {
					p_fi->dev = dev;
					p_fi->io = p_io_buffer;
					p_fi->sz_io = sz_io_buffer;
					r = sizeof(_file_info_t);
				}
			}
		}
	}
 
 	return r;
}

// return file system block size
_u16 fs_get_block_size(_u8 *p_file_info) {
 	_u16 r = 0;
 	_file_info_t *p_fi = (_file_info_t *)p_file_info;
	
	// check for valid MGFS superblock
	if(p_fi->sb.magic == MGFS_MAGIC)
	 	r = p_fi->sb.sz_unit * p_fi->sb.sz_sector;

	return r;
}

// return the size of file
_u32 fs_get_file_size(_u8 *p_file_info) {
 	_u32 r = 0;
	_file_info_t *p_fi = (_file_info_t *)p_file_info;
	
	if(p_fi->sb.magic == MGFS_MAGIC)
		r = (_u32)p_fi->ii.sz;

	return r;
}

_u8 fs_get_type(void) {
	return BOOT_MGFS;
}

// read one blokc from file content & return num. of readed bytes
_u32 fs_read_file_block(_u8 *p_file_info, 	// file information buffer
			_u32 block_num, 	// block number in file
			_u32 count,		// num of file blocks to read
			_u8 *p_buffer, 
			_u32 sz_buffer
			) {
 	_u32 r = 0;
	_u32 i = 0;
	_file_info_t *p_fi = (_file_info_t *)p_file_info;
	
	if(p_fi->sb.magic == MGFS_MAGIC) {
	 	if((count * (p_fi->sb.sz_sector * p_fi->sb.sz_unit)) <= sz_buffer) {
		 	for(i = 0; i < count; i++) {
				 if(file_read_cluster(&p_fi->sb, block_num + i, &p_fi->ii,p_fi->dev,
				  			(p_buffer + (i * (p_fi->sb.sz_sector * p_fi->sb.sz_unit)))) == 0) {
				  	r += (p_fi->sb.sz_sector * p_fi->sb.sz_unit);
				 }
			}
		}
	}
 
	return r;
}

// read 'size' bytes from file 'offset' and return num. of actual readed bytes
// 	this function using i/o buffer 'p_file_info->io'
_u32 fs_read_file(_u8 *p_file_info,		// file information buffer
		  _umax offset,			// offset at begin of file in bytes
		  _u32 size,			// num of bytes to read
		  _u8 *p_buffer,		// destination buffer
		  _u32 sz_buffer		// size of destination buffer
		  ) {

	_u32 r = 0;
	_file_info_t *p_fi = (_file_info_t *)p_file_info;
	// size of cluster
	_u16 cl_sz = p_fi->sb.sz_unit * p_fi->sb.sz_sector;
	// first requested cluster
	_umax cl_num = offset / cl_sz;
	// offset in first cluster
	_u32 fcl_offset = (_u32)(offset % cl_sz);
	_u32 _b;
	_umax _len;
	_umax cl_count = 0;
	
	if(offset < p_fi->ii.sz) {
		_len = (_u32)(((offset + size) < p_fi->ii.sz) ? size : (p_fi->ii.sz - offset));
		cl_count = _len / cl_sz; // count of affected clusters
		 
		if(_len % cl_sz)
			cl_count++;

		while(r < _len) {
			_b = (_u32)((((_umax)(cl_sz - fcl_offset)) > (_len - r)) ? 
					(_len - r):
					(cl_sz - fcl_offset));
					
			if(_b < cl_sz) {
				if(file_read_cluster(&p_fi->sb, cl_num, &p_fi->ii, p_fi->dev, p_fi->io) == 0)
					mem_cpy(p_buffer + r, p_fi->io + fcl_offset, _b);
				else
					goto _read_done;	
			} else {
				if(file_read_cluster(&p_fi->sb, cl_num, &p_fi->ii, p_fi->dev, p_buffer + r))
					goto _read_done;
			}
			
			r += _b;
			if(r >= sz_buffer)
				break;
			fcl_offset = 0;
			cl_num++;
			cl_count--;
		}
	}
	
_read_done:	
	return r;
}

