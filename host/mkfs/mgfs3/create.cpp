#include <sys/mman.h>
#include <stdlib.h>
#include "mkfs.h"
#include "clargs.h"
#include "str.h"
#include "inode.h"
#include "bitmap.h"
#include "buffer.h"
#include "dentry.h"

static void create_superblock(void) {
	_u8 unit_size = DEFAULT_UNIT_SIZE;
	_str_t serial;
	_u32 sz_usize;
	_str_t str_usize;
	_u32 sz_serial;
	
	if(clargs_option("-u", &str_usize, &sz_usize))
		unit_size = (_u8)atoi(str_usize);

	_mgfs_context_t *p_cxt = get_context();

	p_cxt->fs.sz = sizeof(_mgfs_info_t);
	p_cxt->fs.magic = MGFS_MAGIC;
	p_cxt->fs.rzv = MGFS_RESERVED_SECTORS;
	p_cxt->fs.sz_addr = sizeof(_u32);
	p_cxt->fs.version = MGFS_VERSION;
	p_cxt->fs.flags = MGFS_USE_BITMAP_SHADOW;
	p_cxt->fs.sz_sector = MGFS_SECTOR_SIZE;
	p_cxt->fs.sz_unit = unit_size;
	mem_set((_u8 *)&p_cxt->fs.meta, 0, sizeof(_mgfs_inode_record_t));
	mem_set((_u8 *)&p_cxt->fs.meta.location, 0xff, sizeof(p_cxt->fs.meta.location));
	p_cxt->fs.mgb_unit = INVALID_UNIT_NUMBER;
	p_cxt->fs.sz_fs = get_device_size() / (MGFS_SECTOR_SIZE * unit_size);
	if(clargs_option("-n", &serial, &sz_serial)) {
		mem_cpy(p_cxt->fs.serial, (_u8 *)serial, sizeof(p_cxt->fs.serial));
	} else {
		mem_cpy(p_cxt->fs.serial, (_u8 *)"MGFS3", sizeof(p_cxt->fs.serial));
	}

	p_cxt->seq_alloc_unit = MGFS_RESERVED_SECTORS / unit_size;
	p_cxt->seq_alloc_unit += (MGFS_RESERVED_SECTORS / unit_size) ? 1 : 0;
}

static void create_meta_group(void) {
	_mgfs_context_t *p_cxt = get_context();
	bool bshadow = (p_cxt->fs.flags & MGFS_USE_BITMAP_SHADOW) ? true : false;
	if(bshadow)// turn of bitmap shadow
		p_cxt->fs.flags &= ~MGFS_USE_BITMAP_SHADOW;

	p_cxt->flags |= MGFS_SEQUENTIAL_ALLOC;

	// create empty metagroup //
	_mgfs_inode_record_t inode_record;
	mem_set((_u8 *)&inode_record, 0, sizeof(_mgfs_inode_record_t));
	mem_set((_u8 *)&inode_record.location, 0xff, sizeof(inode_record.location));

	_u32 meta_unit = INVALID_UNIT_NUMBER;
	if(mgfs_bitmap_alloc(p_cxt, &meta_unit, 1, 0) == 1) {
		_h_buffer_ hb_meta = mgfs_buffer_alloc(p_cxt, meta_unit);
		if(hb_meta != INVALID_BUFFER_HANDLE) {
			_mgfs_inode_record_t *p_inode = (_mgfs_inode_record_t *)mgfs_buffer_ptr(p_cxt, hb_meta);

			for(_u32 i = 0; i < META_GROUP_MEMBERS; i++) {
				mem_cpy((_u8 *)p_inode, (_u8 *)&inode_record, sizeof(_mgfs_inode_record_t));
				p_inode->cr = p_cxt->fs_context.p_timestamp(0);
				p_inode++;
			}

			p_cxt->fs.meta.cr = mgfs_timestamp(p_cxt);
			p_cxt->fs.meta.sz = META_GROUP_MEMBERS * sizeof(_mgfs_inode_record_t);
			p_cxt->fs.meta.dunits = 1;
			p_cxt->fs.meta.location[0] = meta_unit;

			mgfs_buffer_write(p_cxt, hb_meta);
			mgfs_buffer_free(p_cxt, hb_meta);
		}
	}
	/////////////////////////////////


	// create space bitmap //
	_u64 device_units = get_device_size() / mgfs_unit_size(p_cxt);
	_u64 bitmaps = (device_units / MAX_BITMAP_BITS) + ((device_units % MAX_BITMAP_BITS) ? 1 : 0);
	_u64 sb_file_size = bitmaps * sizeof(_mgfs_bitmap_t);
	_u32 sb_file_units = (sb_file_size / mgfs_unit_size(p_cxt)) + ((sb_file_size % mgfs_unit_size(p_cxt)) ? 1 : 0);
	
	_mgfs_inode_handle_t hsbinode;
	if(mgfs_inode_meta_open(p_cxt, SPACE_BITMAP_IDX, &hsbinode) == 0) {
		VTRACE("Create space bitmap (%u units)\n", sb_file_units);
		mgfs_inode_append_block(p_cxt, &hsbinode, sb_file_units, 0);
		hsbinode.p_inode->sz = sb_file_size;
		mgfs_inode_update(p_cxt, &hsbinode);
		mgfs_inode_close(p_cxt, &hsbinode);
	} else {
		TRACE("%s\n", "ERROR: Failed to create space bitmap");
	}

	// create space bitmap shadow
	if(bshadow) {
		if(mgfs_inode_meta_open(p_cxt, SPACE_BITMAP_SHADOW_IDX, &hsbinode) == 0) {
			VTRACE("Create shadow of space bitmap (%u units)\n", sb_file_units);
			mgfs_inode_append_block(p_cxt, &hsbinode, sb_file_units, 0);
			hsbinode.p_inode->sz = sb_file_size;
			mgfs_inode_update(p_cxt, &hsbinode);
			mgfs_inode_close(p_cxt, &hsbinode);
		} else {
			TRACE("%s\n", "ERROR: Failed to create shadow of space bitmap");
		}

		p_cxt->fs.flags |= MGFS_USE_BITMAP_SHADOW;
	}

	// sequential allocator do not needed any more
	p_cxt->flags &= ~MGFS_SEQUENTIAL_ALLOC;

	// reserve the sequentially allocated units
	if(mgfs_inode_meta_open(p_cxt, SPACE_BITMAP_IDX, &hsbinode) == 0) {
		_u32 unit_map[256];
		_u32 uidx = 0;
		_mgfs_inode_handle_t hbshadow;

		if(bshadow) {
			if(mgfs_inode_meta_open(p_cxt, SPACE_BITMAP_SHADOW_IDX, &hbshadow) != 0)
				bshadow = false;
		}

		for(_u32 i = 0; i < p_cxt->seq_alloc_unit; i++) {
			unit_map[uidx] = i;
			uidx++;
			if(uidx >= 256) {
				mgfs_bitmap_set_array(p_cxt, &hsbinode, unit_map, uidx, true);
				if(bshadow)
					mgfs_bitmap_sync(p_cxt, unit_map, uidx, &hsbinode, &hbshadow, true);
				
				uidx = 0;
			}
		}
		
		mgfs_bitmap_set_array(p_cxt, &hsbinode, unit_map, uidx, true);
		if(bshadow) {
			mgfs_bitmap_sync(p_cxt, unit_map, uidx, &hsbinode, &hbshadow, true);
			mgfs_inode_close(p_cxt, &hbshadow);
		}

		mgfs_inode_close(p_cxt, &hsbinode);
	}

	// create root directory
	_mgfs_inode_handle_t hroot;
	if((p_cxt->fs.root_inode = mgfs_inode_create(p_cxt, MGFS_DIR, 0, &hroot)) != INVALID_INODE_NUMBER) {
		VTRACE("root directory created with inode %u\n", p_cxt->fs.root_inode);
		_mgfs_dentry_handle_t hdthis, hdparent;
		mgfs_dentry_create(p_cxt, &hroot, (_str_t)".", 2, p_cxt->fs.root_inode, 0, &hdthis);
		mgfs_dentry_create(p_cxt, &hroot, (_str_t)"..", 3, p_cxt->fs.root_inode, 0, &hdparent);
		mgfs_dentry_close(p_cxt, &hdthis);
		mgfs_dentry_close(p_cxt, &hdparent);
		mgfs_inode_close(p_cxt, &hroot);
	} else {
		TRACE("ERROR: %s\n", "failed to create root directory");
	}
	////////////////////////////////

	mgfs_update_info(p_cxt);
}

void install_vbr(void) {
	_str_t vbr_name;
	_u32   sz_vbr_name;

	if(clargs_option("-a", &vbr_name, &sz_vbr_name)) {
		_u64 sz_vbr=0;
		_u8 *p_vbr = map_file(vbr_name, &sz_vbr);
		if(p_vbr) {
			if(sz_vbr == MGFS_SECTOR_SIZE) {
				VTRACE("Install VBR from '%s'\n", vbr_name);
				get_fs_context()->p_write(0, 0, 1, p_vbr);
			} else
				TRACE("ERROR: Invalid VBR size (%u)\n", (_u32)sz_vbr);
			
			munmap(p_vbr, sz_vbr);
		} else
			TRACE("ERROR: Failed to open VBR file '%s'\n", vbr_name);
	}
}

void install_fsbr(void) {
	_str_t fsbr_name;
	_u32   sz_fsbr_name;

	if(clargs_option("-b", &fsbr_name, &sz_fsbr_name)) {
		_u64 sz_fsbr=0;
		_u8 *p_fsbr = map_file(fsbr_name, &sz_fsbr);
		if(p_fsbr) {
			if(sz_fsbr <= (MAX_FSBR_SECTORS * MGFS_SECTOR_SIZE)) {
				VTRACE("Install FSBR from '%s'\n", fsbr_name);
				_u32 fsbr_sectors = sz_fsbr / MGFS_SECTOR_SIZE;
				fsbr_sectors += (sz_fsbr % MGFS_SECTOR_SIZE) ? 1 : 0;
				get_fs_context()->p_write(0, FIRST_FSBR_SECTOR, fsbr_sectors, p_fsbr);
				get_context()->fs.sz_fsbr = sz_fsbr;
			} else
				TRACE("ERROR: Invalid FSBR size (%u)\n", (_u32)sz_fsbr);
			
			munmap(p_fsbr, sz_fsbr);
		} else
			TRACE("ERROR: Failed to open FSBR file '%s'\n", fsbr_name);
	}
}

void _create_fs_(void) {
	install_vbr();
	install_fsbr();
	create_superblock();
	create_meta_group();
	parse_file_map();
}

