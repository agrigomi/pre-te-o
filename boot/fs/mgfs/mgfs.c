// mgfs.c
// boot version of MGFS file system (read only)

#include "../../boot.h"
#include "../../../mgfs/mgfs.h"
#include "stub.h"

extern _boot_stub_t _g_stub;

typedef struct {
 	_u8		dev;	// bios device number
	_mgfs_dev_info 	sb;	// mgfs superblock
	_mgfs_inode_info ii;	// file inode	
	_u8 *io;		// buffer for i/o operations 
	_u16 sz_io;		// size of i/o buffer ( min. one block) see 'fs_get_block_size'
}_file_info_t;

// return sizeof superblock in p_sb
static _u16 _get_superblock(_u8 dev,_mgfs_dev_info *p_sb, _u8 *io_buffer) {
 	_u16 r = 0;
	_u8 *lb = io_buffer;
	_u64 lba;
	_u32 i = 0;
	
	lba._qw = 0;
	
	if(!_g_stub._dev.read(lba,dev,1,lb)) {
	 	for(i = 0; i < MGFS_SZ_SECTOR; i++) {
		 	if(*(lb + i + 2) == MARKER_B0 && *(lb + i + 3) == MARKER_B1) {
				r = sizeof(_mgfs_dev_info);
			 	mem_cpy((_u8 *)p_sb, lb + i, r);
			 	break;
			}
		}
	}
	
	return r;
}

static _u8 read_cluster(_mgfs_dev_info *_sb, _umax cluster, _u8 dev, _u8 *_ptr) {
	_u64 lba;
	_u8 spc = 0;	// sectors per cluster
	_umax sector = 0; 
	
	spc = _sb->sz_cluster;	// get num. of sectors per cluster
	sector = cluster * spc;	// calc sector number
	
	// It shall be fixed in 64 bit version, but now we use 32 bit cluster address 
	lba._lw = sector;
	lba._hw = 0;
	////////////////////////
	
	return _g_stub._dev.read(lba,dev,spc,_ptr);
}

// read one cluster from file and return 0 if success
static _u8 file_read_cluster(_mgfs_dev_info *_sb, _umax cluster, _mgfs_inode_info *p_ii, _u8 dev, _u8 *_ptr) {
	_u8 l = p_ii->level;
	_umax *p_cl_num = (_umax *)&p_ii->location[0];
	_u32 count = (_sb->sz_addr == 4) ? 8 : 4;
	_u32 cl_sz = MGFS_SZ_SECTOR * _sb->sz_cluster;
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

/*
static _u32 read_file(_mgfs_dev_info *_sb, _mgfs_inode_info *p_ii, _u8 dev, _u8 *_ptr) {
	_u32 res = 0;
	_u32 cl = (p_ii->sz / (_sb->sz_cluster * MGFS_SZ_SECTOR));
	_u8 *p = _ptr;
	_umax cluster = 0;
	
	if((p_ii->sz % (_sb->sz_cluster * MGFS_SZ_SECTOR)))
		cl++;
		
	while(cl) {
		if(file_read_cluster(_sb, cluster, p_ii, dev, p) == 0) {
			p += (MGFS_SZ_SECTOR * _sb->sz_cluster);
			cl--;
			cluster++;
			res += (MGFS_SZ_SECTOR * _sb->sz_cluster);
		} else
			break;
	}
	
	if(res > p_ii->sz)
		res = p_ii->sz;
	
	return res;
}
*/

static void calc_inode_position(_mgfs_dev_info *_sb, _umax inode, _umax *cluster, _umax *offset) {
	// calculate the inode position 
	*cluster = (inode * sizeof(_mgfs_inode_info)) / (MGFS_SZ_SECTOR * _sb->sz_cluster);
	*offset = (inode * sizeof(_mgfs_inode_info)) % (MGFS_SZ_SECTOR * _sb->sz_cluster);
}

// searching for inode passed by 'path' parameter 
// this function needed by i/o buffer (_ptr) 
//output:
// the 'p_ii' record will be filled if inode exists.
// return count of readed bytes (inode size = 64)
static _u32 get_inode(_mgfs_dev_info *_sb, _u8 dev, _str_t path, _mgfs_inode_info *p_ii, _u8 *_ptr) {
	_u32 res = 0;
	_mgfs_inode_info ii;
	_mgfs_dir_record dy;
	_u16 i,j,k;
	_umax inode_cluster = INVALID_CLUSTER_NUMBER;
	_u32 cluster_offset = 0;
	_umax inode = _sb->i_root;
	_umax ccl = 0;
	_u16 sz_path = str_len(path) + 1;
	
	j = 0;
	for(i = 0; i < sz_path; i++) {
		if(*(path + i) == '/' || *(path + i) == 0 || *(path + i) == ';') {
			if(i - j) {
				calc_inode_position(_sb,inode,&inode_cluster,&cluster_offset);
				
				// loading cluster from master inode file
				if(file_read_cluster(_sb,inode_cluster,&_sb->im,dev,_ptr) == 0) {
					// copy inode because the destination buffer is reusable
					mem_cpy((_u8*)&ii, (_u8*)(_ptr + cluster_offset), sizeof(_mgfs_inode_info));
					if(ii.permissions & MGFS_TYPE_DIR) {
						// calculate count of clusters by current directory
						ccl = ii.sz / (MGFS_SZ_SECTOR * _sb->sz_cluster);
						if(ii.sz % (MGFS_SZ_SECTOR * _sb->sz_cluster))
							ccl++;
						
						// searching for directory record
						inode = INVALID_INODE_NUMBER;
						inode_cluster = 0;
						while(inode_cluster < ccl) {
							// try to read one directory cluster
							if(file_read_cluster(_sb,inode_cluster,&ii,dev,_ptr) == 0) {
								// enumerating the records in current cluster
								for(k = 0; k < (_u16)(MGFS_SZ_SECTOR * _sb->sz_cluster); k += sizeof(_mgfs_dir_record)) {
									// copy directory entry because the destination buffer is reusable
									mem_cpy((_u8*)&dy, _ptr+k,sizeof(_mgfs_dir_record));
									if(dy.inode_num != INVALID_CLUSTER_NUMBER && 
											mem_cmp((((_u8*)&dy)+_sb->sz_addr),(_u8 *)(path + j), 
												str_len((_str_t)(((_u8 *)&dy)+_sb->sz_addr))) == 0) {
										// entry found		
										inode = dy.inode_num; // get entry inode
										inode_cluster = ccl - 1; // break the parent loop
										break;
									}
								}
								
								inode_cluster++;
							} else
								return res;
						}
						
						if(inode == INVALID_INODE_NUMBER)
							return res;
					} else 
						break;
				} else
					return res;
			}

			j = i + 1;
		}
	}
	
	calc_inode_position(_sb,inode,&inode_cluster,&cluster_offset);
	
	if(inode != _sb->i_root && inode != INVALID_INODE_NUMBER) {
		// loading cluster from master inode file
		if(file_read_cluster(_sb,inode_cluster,&_sb->im,dev,_ptr) == 0) {
			// copy inode record because the destination buffer is reusable
			mem_cpy((_u8*)p_ii, (_u8*)(_ptr + cluster_offset), sizeof(_mgfs_inode_info));
			res = sizeof(_mgfs_inode_info);
		}
	}

	return res;		
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
	
	if(sz_buffer >= sizeof(_file_info_t) && sz_io_buffer >= MGFS_SZ_SECTOR) {
		if(_get_superblock(dev,&p_fi->sb,p_io_buffer) == sizeof(_mgfs_dev_info)) {
		 	if(sz_io_buffer >= MGFS_SZ_SECTOR * p_fi->sb.sz_cluster) {
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
	 	r = p_fi->sb.sz_cluster * MGFS_SZ_SECTOR;

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
	 	if((count * (MGFS_SZ_SECTOR * p_fi->sb.sz_cluster)) <= sz_buffer) {
		 	for(i = 0; i < count; i++) {
				 if(file_read_cluster(&p_fi->sb, block_num + i, &p_fi->ii,p_fi->dev,
				  			(p_buffer + (i * (MGFS_SZ_SECTOR * p_fi->sb.sz_cluster)))) == 0) {
				  	r += (MGFS_SZ_SECTOR * p_fi->sb.sz_cluster);
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
	_u16 cl_sz = p_fi->sb.sz_cluster * MGFS_SZ_SECTOR;
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

