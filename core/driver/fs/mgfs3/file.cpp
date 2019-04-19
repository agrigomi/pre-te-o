#include "file.h"
#include "inode.h"
#include "dentry.h"
#include "str.h"

bool mgfs_file_open_root(_mgfs_context_t *p_cxt, // in
				_h_file_ hroot, // out
				_h_lock_ hlock
			) {
	bool r = false;
	
	hroot->hdentry.hb_dentry = INVALID_BUFFER_HANDLE;
	hroot->hdentry.p_dentry = 0;

	if(mgfs_inode_open(p_cxt, p_cxt->fs.root_inode, &hroot->hinode, hlock) == 0)
		r = true;

	return r;
}

bool mgfs_file_open(_mgfs_context_t *p_cxt, _str_t name, _h_file_ hdir, // in
			_h_file_ hfile, // out
			_h_lock_ hlock
		   ) {
	bool r = false;

	if(hdir && hdir->hinode.p_inode && (hdir->hinode.p_inode->flags & MGFS_DIR)) {
		if(mgfs_dentry_open(p_cxt, &hdir->hinode, name, &hfile->hdentry, hlock)) {
			if(hfile->hdentry.p_dentry->flags & DENTRY_LINK) {
				hfile->hinode.number = INVALID_INODE_NUMBER;
				hfile->hinode.p_inode = 0;
				hfile->hinode.hb_inode = INVALID_BUFFER_HANDLE;
				r = true;
			} else {
				if(mgfs_inode_open(p_cxt, hfile->hdentry.p_dentry->inode_number, &hfile->hinode, hlock) == 0)
					r = true;
			}
		}
	}

	return r;
}

void mgfs_file_close(_mgfs_context_t *p_cxt, _h_file_ hfile, _h_lock_ hlock) {
	if(hfile->hinode.hb_inode != INVALID_BUFFER_HANDLE && hfile->hinode.p_inode)
		mgfs_inode_close(p_cxt, &hfile->hinode, hlock);
	if(hfile->hdentry.hb_dentry != INVALID_BUFFER_HANDLE && hfile->hdentry.p_dentry)
		mgfs_dentry_close(p_cxt, &hfile->hdentry, hlock);
}

bool mgfs_file_create(_mgfs_context_t *p_cxt, _str_t name, _u16 flags, _u32 owner, _h_file_ hdir, // in
			_h_file_ hfile, // out
			_h_lock_ hlock
		     ) {
	bool r = false;

	if(hdir->hinode.p_inode->flags & MGFS_DIR) {
		_u32 inode_number = mgfs_inode_create(p_cxt, flags, owner, &hfile->hinode, hlock);
		if(inode_number != INVALID_INODE_NUMBER) {
			r = mgfs_dentry_create(p_cxt, &hdir->hinode, name, str_len(name) + 1, 
						inode_number, 0, &hfile->hdentry, hlock);

			if(r && (flags & MGFS_DIR)) {
				// create '.' and '..'
				_u32 parent_inode_number = hdir->hinode.number;
				_mgfs_dentry_handle_t hdthis, hdparent;
				mgfs_dentry_create(p_cxt, &hfile->hinode, (_str_t)".", 2, 
							inode_number, 0, &hdthis, hlock);
				mgfs_dentry_create(p_cxt, &hfile->hinode, (_str_t)"..", 3, 
							parent_inode_number, 0, &hdparent, hlock);

				mgfs_dentry_close(p_cxt, &hdthis, hlock);
				mgfs_dentry_close(p_cxt, &hdparent, hlock);
			}
		}
	}

	return r;
}

bool mgfs_file_delete(_mgfs_context_t *p_cxt, _str_t name, _h_file_ hdir, _h_lock_ hlock) {
	bool r = false;

	if(hdir->hinode.p_inode->flags & MGFS_DIR) {
		_mgfs_file_handle_t hfile;
		if(mgfs_file_open(p_cxt, name, hdir, &hfile, hlock)) {
			bool del = true;

			if(hfile.hinode.p_inode->flags & MGFS_DIR) {
				// check for empty directory
				//???
			}

			mgfs_file_close(p_cxt, &hfile, hlock);
			if(del) {
				if(hfile.hinode.p_inode) 
					mgfs_inode_delete(p_cxt, &hfile.hinode, hlock);
				r = mgfs_dentry_delete(p_cxt, &hdir->hinode, name, hlock);
			}
		}
	}

	return r;
}

bool mgfs_file_move(_mgfs_context_t *p_cxt, _h_file_ hsrcdir, _str_t srcname, _h_file_ hdstdir, _str_t dstname, _h_lock_ hlock) {
	bool r = false;

	_mgfs_file_handle_t hfsrc;
	if(mgfs_file_open(p_cxt, srcname, hsrcdir, &hfsrc, hlock)) {
		_mgfs_file_handle_t hfdst;
		if(mgfs_file_open(p_cxt, dstname, hdstdir, &hfdst, hlock)) {
			// check for existing destination 
			if(hfdst.hinode.p_inode->flags & MGFS_DIR) { // the destination is a directory (allowed)
				// create new dentry in 'hfdst' with name located in source dentry
				_mgfs_dentry_handle_t hdnew;
				_str_t newfname = mgfs_dentry_name(&hfsrc.hdentry);
				_u16 newfname_sz = mgfs_dentry_name_size(&hfsrc.hdentry);
	
				if(mgfs_dentry_create(p_cxt, &hfdst.hinode, newfname, newfname_sz, hfsrc.hinode.number,
							hfsrc.hdentry.p_dentry->flags, &hdnew, hlock)) {
					r = mgfs_dentry_delete(p_cxt, &hsrcdir->hinode, srcname, hlock);
					mgfs_dentry_close(p_cxt, &hdnew, hlock);
				}
			} else
				p_cxt->last_error = MGFS_DENTRY_EXISTS;

			mgfs_file_close(p_cxt, &hfdst, hlock);
		} else {
			// create new dentry in 'hdstdir' with name 'dstname'
			_mgfs_dentry_handle_t hdnew;
			if(mgfs_dentry_flags(&hfsrc.hdentry) & DENTRY_LINK) {
				// create link dentry
				_u32 newfname_sz = str_len(dstname)+1 + str_len(mgfs_dentry_link_name(&hfsrc.hdentry))+1;
				_str_t newfname = (_str_t)mgfs_mem_alloc(p_cxt, newfname_sz);
				if(newfname) {
					mem_cpy((_u8 *)newfname, (_u8 *)dstname, str_len(dstname)+1);
					mem_cpy((_u8 *)newfname + str_len(dstname)+1, 
						(_u8 *)mgfs_dentry_link_name(&hfsrc.hdentry),
						str_len(mgfs_dentry_link_name(&hfsrc.hdentry))+1);

					if(mgfs_dentry_create(p_cxt, &hdstdir->hinode, newfname, newfname_sz, hfsrc.hinode.number,
								hfsrc.hdentry.p_dentry->flags, &hdnew, hlock)) {
						r = mgfs_dentry_delete(p_cxt, &hsrcdir->hinode, srcname, hlock);
						mgfs_dentry_close(p_cxt, &hdnew, hlock);
					}
					
					mgfs_mem_free(p_cxt, newfname, newfname_sz);
				}
			} else {
				// create regular dentry
				if(mgfs_dentry_create(p_cxt, &hdstdir->hinode, dstname, str_len(dstname)+1, hfsrc.hinode.number,
							hfsrc.hdentry.p_dentry->flags, &hdnew, hlock)) {
					r = mgfs_dentry_delete(p_cxt, &hsrcdir->hinode, srcname, hlock);
					mgfs_dentry_close(p_cxt, &hdnew, hlock);
				}
			}
		}

		mgfs_file_close(p_cxt, &hfsrc, hlock);
	}

	return r;
}

bool mgfs_file_create_hard_link(_mgfs_context_t *p_cxt, _h_file_ hsrcdir, _str_t name,
				_h_file_ hdstdir, _str_t link_name, _h_lock_ hlock) {
	bool r = false;

	_mgfs_file_handle_t hfile;
	if(mgfs_file_open(p_cxt, name, hsrcdir, &hfile, hlock)) {
		_mgfs_dentry_handle_t hdentry;
		hdentry.hb_dentry = INVALID_BUFFER_HANDLE;
		r = mgfs_dentry_create(p_cxt, &hdstdir->hinode, link_name, str_len(link_name)+1, 
					hfile.hdentry.p_dentry->inode_number, 0, &hdentry, hlock);
		if(r) {
			hfile.hinode.p_inode->lc++;
			mgfs_inode_update(p_cxt, &hfile.hinode, hlock);
			mgfs_dentry_close(p_cxt, &hdentry, hlock);
		}

		mgfs_file_close(p_cxt, &hfile, hlock);
	}

	return r;
}

bool mgfs_file_create_soft_link(_mgfs_context_t *p_cxt, _str_t path_name,
				_h_file_ hdstdir, _str_t link_name, _h_lock_ hlock) {
	bool r = false;
	_u16 name_size = str_len(link_name) + str_len(path_name) + 2;
	_u8 *ptr_name = (_u8 *)mgfs_mem_alloc(p_cxt, name_size);
	if(ptr_name) {
		mem_cpy(ptr_name, (_u8 *)link_name, str_len(link_name) + 1);
		mem_cpy(ptr_name + str_len(link_name) + 1, (_u8 *)path_name, str_len(path_name) + 1);
		_mgfs_dentry_handle_t hdlink;
		r = mgfs_dentry_create(p_cxt, &hdstdir->hinode, (_str_t)ptr_name, name_size, INVALID_INODE_NUMBER, DENTRY_LINK,
					&hdlink, hlock);
		if(r)
			mgfs_dentry_close(p_cxt, &hdlink, hlock);

		mgfs_mem_free(p_cxt, ptr_name, name_size);
	}

	return r;
}

_u32 mgfs_file_read(_mgfs_context_t *p_cxt, _h_file_ hfile, _u64 offset, void *buffer, _u32 nbytes, _h_lock_ hlock) {
	return mgfs_inode_read(p_cxt, &hfile->hinode, offset, buffer, nbytes, hlock);
}

_u32 mgfs_file_write(_mgfs_context_t *p_cxt, _h_file_ hfile, _u64 offset, void *buffer, _u32 nbytes, _h_lock_ hlock) {
	return mgfs_inode_write(p_cxt, &hfile->hinode, offset, buffer, nbytes, hlock);
}

_u64 mgfs_file_size(_h_file_ hfile) {
	_u64 r = 0;

	if(hfile->hinode.p_inode)
		r = hfile->hinode.p_inode->sz;

	return r;
}

