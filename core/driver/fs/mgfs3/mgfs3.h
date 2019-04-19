// Meta Group File System (MGFS) v3

#ifndef __MGFS3_H__
#define __MGFS3_H__

#include "mgtype.h"
#include "iFSIO.h"

#define MGFS_MAGIC		0xf3db
#define MGFS_VERSION		0x0301
#define MGFS_RESERVED_SECTORS	47
#define MGFS_SB_SECTOR		1
#define MGFS_SB_OFFSET		0x100
#define MGFS_SECTOR_SIZE	0x200
#define MAX_FSBR_SECTORS	46
#define FIRST_FSBR_SECTOR	1

#define MAX_SECTORS_PER_UNIT	8
#define MAX_INODE_LEVEL		3
#define MAX_VOLUME_SERIAL	16

// inode flags common 
#define MGFS_ALL_EXEC		(0x01 << 0)
#define MGFS_ALL_WRITE		(0x01 << 1)
#define MGFS_ALL_READ		(0x01 << 2)
#define MGFS_GROUP_EXEC		(0x01 << 3)
#define MGFS_GROUP_WRITE	(0x01 << 4)
#define MGFS_GROUP_READ		(0x01 << 5)
#define MGFS_OWNER_EXEC		(0x01 << 6)
#define MGFS_OWNER_WRITE	(0x01 << 7)
#define MGFS_OWNER_READ		(0x01 << 8)
#define MGFS_DIR		(0x01 << 9)
#define MGFS_HIDDEN		(0x01 << 10)
// inode flags private
#define MGFS_DELETED		(0x01 << 15)

#define MAX_LOCATION_INDEX	8

#define __ERR			0xffffffff

#define INVALID_SECTOR_NUMBER	__ERR
#define INVALID_INODE_NUMBER	__ERR
#define INVALID_BLOCK_NUMBER	__ERR

#define MGFS_NOT_ENUGH_SPACE	__ERR-1

#define META_GROUP_MEMBERS	6

// meta group inode records
#define SPACE_BITMAP_IDX	0
#define INODE_FILE_IDX		1
#define INODE_BITMAP_IDX	2
#define SPACE_BITMAP_SHADOW_IDX	3
#define INODE_BITMAP_SHADOW_IDX	4
#define TRANSACTION_LOG_IDX	5


typedef struct { // inode record
	_u16	flags;
	_u32	cr; 	// creation timestamp
	_u32	mo; 	// modification timestamp
	_u16	lc; 	// link counter
	_u32	dunits; // number of data units
	_u8	level; 	// location level
	_u32	location[MAX_LOCATION_INDEX]; // root unit array
	_u64	sz; 	// file size in bytes
	_u32	owner;	// owner ID
	_u8	reserved[3];
}__attribute__((packed)) _mgfs_inode_record_t;

// directory entry flags
#define DENTRY_DELETED	(1<<0)
#define DENTRY_LINK	(1<<1)

typedef struct { // directory entry record
	_u16  	name_size;	// sizeof file name in bytes including zero termination
	_u16 	record_size;  	// record size including name
	_u8  	flags;
	_u32 	inode_number;
	_u8  	reserved[23];
}__attribute__((packed)) _mgfs_dir_record_t;

#define MGFS_USE_BITMAP_SHADOW	(1<<0)
#define MGFS_USE_INODE_SHADOW	(1<<1)

typedef struct { // superblock
	_u8			sz; 		// size of this structure
	_u16			magic; 		// 'mgfs' magic
	_u8			rzv; 		// number of reserved sectors at begin of device
	_u8			sz_addr; 	// address size in bytes
	_u16			version; 	// file system version
	_u8			flags; 		// filesystem behavior flags
	_u16			sz_sector; 	// sector size in bytes
	_u8			sz_unit; 	// size of unit in sectors
	_mgfs_inode_record_t	meta; 		// master inode container (Meta Group)
	_u32			root_inode;	// inode number for root directory
	_u16			sz_fsbr;	// size of filesystem boot record
	_u32			mgb_unit; 	// meta group backup unit
	_u32			sz_fs; 		// amount of all units in file system
	_u8			serial[MAX_VOLUME_SERIAL]; // volume serial number
}__attribute__((packed)) _mgfs_info_t;


typedef struct {
	_mgfs_inode_record_t 	*p_inode;
	_h_buffer_		hb_inode;
	_u32			number;
}_mgfs_inode_handle_t;

typedef _mgfs_inode_handle_t*	_h_inode_;

typedef struct {
	_mgfs_dir_record_t 	*p_dentry;
	_h_buffer_		hb_dentry;
}_mgfs_dentry_handle_t;

typedef _mgfs_dentry_handle_t*	_h_dentry_;

typedef struct {
	_mgfs_inode_handle_t	hinode;
	_mgfs_dentry_handle_t 	hdentry;
}_mgfs_file_handle_t;

typedef _mgfs_file_handle_t*	_h_file_;

#define MGFS_SEQUENTIAL_ALLOC	(1<<0)
#define MGFS_DELAY_WRITE	(1<<1)

typedef struct {
	_fs_context_t		fs_context;
	_mgfs_info_t		fs;
	_u8			flags; // filesystem runtime behavior flags
	_u32			last_error;
	_u32			seq_alloc_unit;
}_mgfs_context_t;

_mgfs_info_t *mgfs_get_info(_mgfs_context_t *p_cxt);
void mgfs_update_info(_mgfs_context_t *p_cxt);
_u32 mgfs_unit_read(_mgfs_context_t *p_cxt, _u32 nunit, void *p_buffer);
_u32 mgfs_unit_write(_mgfs_context_t *p_cxt, _u32 nunit, void *p_buffer);
void *mgfs_mem_alloc(_mgfs_context_t *p_cxt, _u32 sz);
void mgfs_mem_free(_mgfs_context_t *p_cxt, void *ptr, _u32 size);
_h_lock_ mgfs_lock(_mgfs_context_t *p_cxt, _h_lock_ hlock);
void mgfs_unlock(_mgfs_context_t *p_cxt, _h_lock_ hlock);
_u32 mgfs_timestamp(_mgfs_context_t *p_cxt);
_u32 mgfs_unit_size(_mgfs_context_t *p_cxt);
_u8  mgfs_flags(_mgfs_context_t *p_cxt);

#endif

