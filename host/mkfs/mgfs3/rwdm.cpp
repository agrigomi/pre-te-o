#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include "mkfs.h"
#include "clargs.h"
#include "file.h"
#include "inode.h"
#include "dentry.h"
#include "buffer.h"
#include "str.h"

static _str_t get_src(void) {
	_str_t r = 0;
	_u32 sz = 0;
	clargs_option("-S", &r, &sz);
	return r;
}

static _str_t get_dst(void) {
	_str_t r = 0;
	_u32 sz = 0;
	clargs_option("-D", &r, &sz);
	return r;
}

static _str_t get_bs1(void) {
	_str_t r = 0;
	_u32 sz = 0;
	clargs_option("-a", &r, &sz);
	return r;
}

static _str_t get_bs2(void) {
	_str_t r = 0;
	_u32 sz = 0;
	clargs_option("-b", &r, &sz);
	return r;
}

static void export_stage1(_str_t dst_file) {
	_mgfs_context_t *p_cxt = get_context();
	FILE *hf = fopen(dst_file, "w+");
	if(hf) {
		VTRACE("Export Volume Boot Record in '%s'\n", dst_file);
		_h_buffer_ hb = mgfs_buffer_alloc(p_cxt, 0);
		if(hb != INVALID_BUFFER_HANDLE) {
			_u8 *ptr = (_u8 *)mgfs_buffer_ptr(p_cxt, hb);
			fwrite(ptr, MGFS_SECTOR_SIZE, 1, hf);
			mgfs_buffer_free(p_cxt, hb);
		}

		fclose(hf);
	} else
		TRACE("ERROR: Failed to open '%s'\n", dst_file);
}

static void export_stage2(_str_t dst_file) {
	_mgfs_context_t *p_cxt = get_context();
	FILE *hf = fopen(dst_file, "w+");
	if(hf) {
		VTRACE("Export Filesystem Boot Record in '%s'\n", dst_file);

		_u16 nsectors = p_cxt->fs.sz_fsbr / MGFS_SECTOR_SIZE;
		nsectors += (p_cxt->fs.sz_fsbr / MGFS_SECTOR_SIZE) ? 1 : 0;

		_u8 *ptr = (_u8 *)p_cxt->fs_context.p_alloc(0, MGFS_SECTOR_SIZE);
		if(ptr) {
			_u16 sector = FIRST_FSBR_SECTOR;
			_u16 offset = 0;
			while(offset < p_cxt->fs.sz_fsbr) {
				if(p_cxt->fs_context.p_read(0, sector, 1, ptr) == 1) {
					_u32 nb = 0;
					if((p_cxt->fs.sz_fsbr - offset) > MGFS_SECTOR_SIZE)
						nb = MGFS_SECTOR_SIZE;
					else
						nb = p_cxt->fs.sz_fsbr - offset;

					fwrite(ptr, nb, 1, hf);
					offset += nb;
					sector++;
				} else {
					TRACE("ERROR: Failed to read sector %d\n", sector);
					break;
				}
			}

			p_cxt->fs_context.p_free(0, ptr, MGFS_SECTOR_SIZE);
		} else
			TRACE("%s\n", "ERROR: Failed to allocate memory");

		fclose(hf);
	} else
		TRACE("ERROR: Failed to open '%s'\n", dst_file);
}

void export_file(_str_t src, _str_t dst) {
	_mgfs_file_handle_t hdir;
	_str_t fname;

	_mgfs_context_t *p_cxt = get_context();
	FILE *hf = fopen(dst, "w+");
	if(hf) {
		if(path_parser(src, 0, &hdir, &fname)) {
			_mgfs_file_handle_t hfsrc;

			if(mgfs_file_open(p_cxt, fname, &hdir, &hfsrc)) {
				if(!(hfsrc.hinode.p_inode->flags & MGFS_DIR)) {
					_u64 offset = 0;
					_u64 size = mgfs_file_size(&hfsrc);
					_u32 fblock = 0;
					
					VTRACE("Export file '%s' --(%u)--> '%s'\n", src, (_u32)size, dst);

					while(offset < size) {
						_h_buffer_ hb = mgfs_inode_read_block(p_cxt, &hfsrc.hinode, fblock);
						if(hb != INVALID_BUFFER_HANDLE) {
							_u8 *ptr = (_u8 *)mgfs_buffer_ptr(p_cxt, hb);
							_u32 nb = ((size - offset) > mgfs_unit_size(p_cxt)) ? 
									mgfs_unit_size(p_cxt) : 
									(size - offset);

							fwrite(ptr, nb, 1, hf);
							offset += nb;
							fblock++;

							mgfs_buffer_free(p_cxt, hb);
						} else {
							TRACE("ERROR: Failed to read block %d from '%s'\n", fblock, src);
							break;
						}
					}
				} else
					TRACE("ERROR: '%s' is a directory\n", src);
			
				mgfs_file_close(p_cxt, &hfsrc);
			} else
				TRACE("ERROR: Failed to open MGFS3 file '%s'\n", src);

			mgfs_file_close(p_cxt, &hdir);
		} else
			TRACE("ERROR: Incorrect  path to '%s'\n", src);
		
		fclose(hf);
	} else
		TRACE("ERROR: Failed to open '%s'\n", dst);
}

void _read_file_(void) {
	_str_t src = get_src();
	_str_t dst = get_dst();
	_str_t bs1 = get_bs1();
	_str_t bs2 = get_bs2();

	if(bs1)
		export_stage1(bs1);
	if(bs2)
		export_stage2(bs2);
	if(!src)
		src = clargs_parameter(2);
	if(!dst)
		dst = clargs_parameter(3);

	if(src && dst)
		export_file(src, dst);
}

static void import_file(_mgfs_context_t *p_cxt, _u8 *p_src, _u64 sz, _h_file_ hdst) {
	if(!(hdst->hinode.p_inode->flags & MGFS_DIR)) 
		mgfs_file_write(p_cxt, hdst, 0, p_src, sz);
	else
		TRACE("ERROR: '%s' is a directory\n", mgfs_dentry_name(&hdst->hdentry));
}

void import_file(_str_t src, _str_t dst) {
	_mgfs_context_t *p_cxt = get_context();
	_mgfs_file_handle_t hdir;
	_str_t fname;
	_u8 *p_src = 0;
	_u64 sz_src = 0;

	if((p_src = map_file(src, &sz_src))) {
		if(path_parser(dst, 0, &hdir, &fname)) {
			_mgfs_file_handle_t hfile;

			VTRACE("Import file '%s' --(%u)--> '%s'\n", src, (_u32)sz_src, dst);

			if(mgfs_file_open(p_cxt, fname, &hdir, &hfile)) {
				import_file(p_cxt, p_src, sz_src, &hfile);
				mgfs_file_close(p_cxt, &hfile);
			} else {
				if(mgfs_file_create(p_cxt, fname, 0, 0, &hdir, &hfile)) {
					import_file(p_cxt, p_src, sz_src, &hfile);
					mgfs_file_close(p_cxt, &hfile);
				} else
					TRACE("ERROR: failed to create destination file '%s'\n", dst);
			}

			mgfs_file_close(p_cxt, &hdir);
		} else
			TRACE("ERROR: Incorrect path '%s'\n", dst);

		munmap(p_src, sz_src);
	} else
		TRACE("ERROR: Failed to open source file '%s'\n", src);
}

void _write_file_(void) {
	_str_t src = get_src();
	_str_t dst = get_dst();
	_str_t bs1 = get_bs1();
	_str_t bs2 = get_bs2();
	
	if(bs1)
		install_vbr();
	if(bs2) {
		install_fsbr();
		mgfs_update_info(get_context());
	}

	if(!src)
		src = clargs_parameter(2);
	if(!dst)
		dst = clargs_parameter(3);

	if(src && dst)
		import_file(src, dst);

	parse_file_map();
}

void _delete_file_(void) {
	_mgfs_context_t *p_cxt = get_context();
	_mgfs_file_handle_t hdir;
	_str_t fname;
	_str_t dst = get_dst();

	if(!dst)
		dst = clargs_parameter(2);

	if(dst) {
		if(path_parser(dst, 0, &hdir, &fname)) {
			_mgfs_file_handle_t hfile;
			if(mgfs_file_open(p_cxt, fname, &hdir, &hfile)) {
				if(hfile.hdentry.p_dentry->flags & DENTRY_LINK) {
					if(!mgfs_file_delete(p_cxt, fname, &hdir)) {
						TRACE("ERROR: Can't delete file '%s'\n", dst);
					} else {
						VTRACE("Delete '%s'\n", dst);
					}
				} else {
					if(!(hfile.hinode.p_inode->flags & MGFS_DIR)) {
						mgfs_file_close(p_cxt, &hfile);
						if(!mgfs_file_delete(p_cxt, fname, &hdir)) {
							TRACE("ERROR: Can't delete file '%s'\n", dst);
						} else {
							VTRACE("Delete '%s'\n", dst);
						}
					} else {
						TRACE("ERROR: '%s' is a directory\n", fname);
						mgfs_file_close(p_cxt, &hfile);
					}
				}
			}

			mgfs_file_close(p_cxt, &hdir);
		} else
			TRACE("ERROR: Incorrect path '%s'\n", dst);
	}
}

void _move_file_(void) {
	_mgfs_file_handle_t hsrcdir;
	_mgfs_file_handle_t hdstdir;
	_str_t fsrcname, fdstname;
	_str_t dst = get_dst();
	_str_t src = get_src();

	if(!src)
		src = clargs_parameter(2);
	if(!dst)
		dst = clargs_parameter(3);

	if(path_parser(src, 0, &hsrcdir, &fsrcname)) {
		if(path_parser(dst, 0, &hdstdir, &fdstname)) {
			if(mgfs_file_move(get_context(), &hsrcdir, fsrcname, &hdstdir, fdstname)) {
				VTRACE("Move '%s' --> '%s'\n", src, dst);
			} else
				TRACE("ERROR: Failed to move file '%s'\n", src);

			mgfs_file_close(get_context(), &hdstdir);
		} else
			TRACE("ERROR: Incorrect path '%s'\n", dst);	

		mgfs_file_close(get_context(), &hsrcdir);
	} else
		TRACE("ERROR: Incorrect path '%s'\n", src);
}

