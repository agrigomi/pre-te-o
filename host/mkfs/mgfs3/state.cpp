#include "mkfs.h"
#include "dentry.h"
#include "file.h"
#include "str.h"
#include "inode.h"
#include "bitmap.h"

static void _dentry_list_(_u16 inode_flags, _u8 dentry_flags, _str_t name, _str_t link_path, 
			_u32 ct, _u32 mt, _u32 owner, _u64 size, void *p_udata) {
	if(dentry_flags & DENTRY_LINK) {
		TRACE("\t\t%s --> %s\n", name, link_path);
	} else {
		TRACE("%lu\t\t%s", (_ulong)size, name);
		if(inode_flags & MGFS_DIR)
			TRACE("%c",'/');
		TRACE("%c",'\n');
	}
}

void _list_(void) {
	_str_t dst;
	_u32 ldst;

	if(!clargs_option("-D", &dst, &ldst))
		dst = clargs_parameter(2);

	if(dst) {
		_mgfs_file_handle_t hdir;
		_str_t fname = 0;
		if(path_parser(dst, 0, &hdir, &fname)) {
			_mgfs_file_handle_t hfile;
			if(str_len(fname)) {
				if(mgfs_file_open(get_context(), fname, &hdir, &hfile)) {
					mgfs_dentry_list(get_context(), &hfile.hinode, _dentry_list_, 0);
					mgfs_file_close(get_context(), &hfile);
				} else
					TRACE("ERROR: Failed to open '%s'\n", dst);
			} else 
				mgfs_dentry_list(get_context(), &hdir.hinode, _dentry_list_);

			mgfs_file_close(get_context(), &hdir);
		} else
			TRACE("ERROR: Incorrect path '%s'\n", dst);
	}
}

void _status_(void) {
	_mgfs_inode_handle_t hsbitmap, hsbshadow;

	_u64 bytes = get_context()->fs.sz_sector * get_context()->fs.sz_unit;

	if(mgfs_inode_meta_open(get_context(), SPACE_BITMAP_IDX, &hsbitmap) == 0) {
		_u32 units = mgfs_bitmap_free_state(get_context(), &hsbitmap);
		TRACE("Primar bitmap free space in units: %u (%lu bytes)\n",units, (long)(units * bytes));
		mgfs_inode_close(get_context(), &hsbitmap);
	}
	
	if(mgfs_inode_meta_open(get_context(), SPACE_BITMAP_SHADOW_IDX, &hsbshadow) == 0) {
		_u32 units = mgfs_bitmap_free_state(get_context(), &hsbitmap);
		TRACE("Shadow bitmap free space in units: %u (%lu bytes)\n",units, (long)(units * bytes));
		mgfs_inode_close(get_context(), &hsbshadow);
	}
}

