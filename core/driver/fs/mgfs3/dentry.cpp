#include "mgfs3.h"
#include "inode.h"
#include "bitmap.h"
#include "buffer.h"
#include "dentry.h"
#include "str.h"

#define INVALID_BLOCK_OFFSET	0xffff

//get name from dentry pointer
#define _DENTRY_NAME_(p_dentry) ((_str_t)((_u8 *)p_dentry + sizeof(_mgfs_dir_record_t)))
#define _DENTRY_LINK_NAME_(p_dentry) ((_str_t)(((_u8 *)p_dentry) + \
					sizeof(_mgfs_dir_record_t) + \
					str_len(_DENTRY_NAME_(p_dentry)) + 1))

typedef struct {
	_u32 block;
	_u16 offset;
}__attribute__((packed)) _dentry_position_t;

typedef struct {
	_dentry_position_t _l;
	_dentry_position_t _p;
	_dentry_position_t _r;
}__attribute__((packed)) _dentry_link_t;

#define _DENTRY_LINK_RECORD_(p_dentry) ((_dentry_link_t *)&p_dentry->reserved[0])

bool mgfs_is_valid_dentry_handle(_h_dentry_ h) {
	bool r = false;
	if(h->hb_dentry != INVALID_BUFFER_HANDLE)
		r = true;
	return r;
}

_str_t mgfs_dentry_name(_h_dentry_ hd) {
	_str_t r = 0;

	if(hd->hb_dentry != INVALID_BUFFER_HANDLE && hd->p_dentry)
		r = _DENTRY_NAME_(hd->p_dentry);

	return r;
}

_str_t mgfs_dentry_link_name(_h_dentry_ hd) {
	_str_t r = 0;

	if(hd->hb_dentry != INVALID_BUFFER_HANDLE && hd->p_dentry)
		r = _DENTRY_LINK_NAME_(hd->p_dentry);

	return r;
}

_u16 mgfs_dentry_name_size(_h_dentry_ hd) {
	return hd->p_dentry->name_size;
}

_u8 mgfs_dentry_flags(_h_dentry_ hd) {
	return hd->p_dentry->flags;
}

static bool dentry_find(_mgfs_context_t *p_cxt, _h_inode_ hdir, _str_t name, _u32 sz_name, // in
			_dentry_position_t *p_last, // allways set with the last found record in btree search
			_dentry_position_t *p_inplace, // out
			_dentry_position_t *p_deleted, // out
			_h_lock_ hlock
		       ) {
	bool r = false;
	_dentry_position_t cdp; // current dentry position
	cdp.block = 0;
	cdp.offset = 0;

	if(mgfs_is_valid_inode_handle(hdir)) {
		while(cdp.block != INVALID_BLOCK_NUMBER) {
			_h_buffer_ hb = mgfs_inode_read_block(p_cxt, hdir, cdp.block, hlock);
			if(hb != INVALID_BUFFER_HANDLE) {
				_u8 *ptr = (_u8 *)mgfs_buffer_ptr(p_cxt, hb);
				_mgfs_dir_record_t *p_dentry = (_mgfs_dir_record_t *)(ptr + cdp.offset);
				_dentry_link_t *p_dlink = _DENTRY_LINK_RECORD_(p_dentry);

				p_last->block = cdp.block;
				p_last->offset = cdp.offset;

				if((p_dentry->flags & DENTRY_DELETED) && p_deleted->block == INVALID_BLOCK_NUMBER) {
					if(p_dentry->record_size >= (sizeof(_mgfs_dir_record_t) + sz_name)) {
						p_deleted->block = cdp.block;
						p_deleted->offset = cdp.offset;
					}
				}

				if((p_dentry->record_size - sizeof(_mgfs_dir_record_t) - p_dentry->name_size) >= 
										(sizeof(_mgfs_dir_record_t) + sz_name) && 
									p_inplace->block == INVALID_BLOCK_NUMBER) {
					p_inplace->block = cdp.block;
					p_inplace->offset = cdp.offset;
				}

				_str_t cname = (_str_t)(ptr + cdp.offset + sizeof(_mgfs_dir_record_t));
				_s32 cmp = str_cmp(name, cname);
				if(cmp == 0) {
					// equal !!!
					if(p_dentry->flags & DENTRY_DELETED) {
						p_deleted->block = cdp.block;
						p_deleted->offset = cdp.offset;
					} else {
						r = true;
						cdp.block = INVALID_BLOCK_NUMBER;
					}
				} else {
					if(cmp < 0) { // left direction
						cdp.block = p_dlink->_l.block;
						cdp.offset = p_dlink->_l.offset;
					} else { // right direction
						cdp.block = p_dlink->_r.block;
						cdp.offset = p_dlink->_r.offset;
					}
				}

				mgfs_buffer_free(p_cxt, hb, hlock);
			} else
				break;
		}
	}

	return r;
}
			
static bool dentry_by_position(_mgfs_context_t *p_cxt, _h_inode_ hdir, _dentry_position_t *p_pos, // in
				_h_dentry_ hdentry, // out
				_h_lock_ hlock
				) {
	bool r = false;

	if(mgfs_is_valid_inode_handle(hdir)) {
		if((hdentry->hb_dentry = mgfs_inode_read_block(p_cxt, hdir, p_pos->block, hlock)) != INVALID_BUFFER_HANDLE) {
			_u8 *ptr = (_u8 *)mgfs_buffer_ptr(p_cxt, hdentry->hb_dentry);
			hdentry->p_dentry = (_mgfs_dir_record_t *)(ptr + p_pos->offset);
			r = true;
		}
	}

	return r;
}

bool mgfs_dentry_open(_mgfs_context_t *p_cxt, _h_inode_ hdir, _str_t name, // in
		      _h_dentry_ hd, // out
		      _h_lock_ hlock
		     ) {
	bool r = false;

	if(mgfs_is_valid_inode_handle(hdir)) {
		_dentry_position_t dp_last;
		_dentry_position_t dp_inplace;
		_dentry_position_t dp_deleted;

		dp_last.block = dp_inplace.block = dp_deleted.block = INVALID_BLOCK_NUMBER;

		if((r = dentry_find(p_cxt, hdir, name, str_len(name)+1, &dp_last, &dp_inplace, &dp_deleted, hlock))) 
			r = dentry_by_position(p_cxt, hdir, &dp_last, hd, hlock);
	}
	return r;
}

static _s32 dentry_compare(_h_dentry_ hd1, _h_dentry_ hd2) {
	return str_cmp(_DENTRY_NAME_(hd1->p_dentry), _DENTRY_NAME_(hd2->p_dentry));
}

static bool dentry_record_attach(_mgfs_context_t *p_cxt, _h_inode_ hdir, _dentry_position_t *p_rec_pos, 
				_dentry_position_t *p_rec_parent, 
				_h_lock_ hlock
				) {
	bool r = false;
	_mgfs_dentry_handle_t hdrec;

	if(dentry_by_position(p_cxt, hdir, p_rec_pos, &hdrec, hlock)) {
		_dentry_position_t cdp;// current dentry position
		cdp.block = (p_rec_parent) ? p_rec_parent->block : 0;
		cdp.offset = (p_rec_parent) ? p_rec_parent->offset : 0;
		_mgfs_dentry_handle_t hcdr; // handle of current dentry record
		_s32 cmp = 0;
		_dentry_link_t *p_dlink = 0;
		_u32 block = cdp.block;

		while(block != INVALID_BLOCK_NUMBER) {
			hcdr.p_dentry = 0;
			hcdr.hb_dentry = INVALID_BUFFER_HANDLE;
			p_dlink = 0;

			if(dentry_by_position(p_cxt, hdir, &cdp, &hcdr, hlock)) {
				p_dlink = _DENTRY_LINK_RECORD_(hcdr.p_dentry);

				cmp = dentry_compare(&hdrec, &hcdr);
				if(cmp == 0) {
					// equal
					block = cdp.block = INVALID_BLOCK_NUMBER;
					p_dlink = 0;
				} else {
					if(cmp < 0) {
						// left direction
						if((block = p_dlink->_l.block) != INVALID_BLOCK_NUMBER) {
							cdp.block = p_dlink->_l.block;
							cdp.offset = p_dlink->_l.offset;
						} else
							break;
					} else {
						// right direction
						if((block = p_dlink->_r.block) != INVALID_BLOCK_NUMBER) {
							cdp.block = p_dlink->_r.block;
							cdp.offset = p_dlink->_r.offset;
						} else
							break;
					}
				}

				mgfs_dentry_close(p_cxt, &hcdr, hlock);
			} else
				break;
		}

		if(hcdr.hb_dentry != INVALID_BUFFER_HANDLE && p_dlink != 0 && cmp != 0) {
			// update target record
			_dentry_link_t *p_ndlink = _DENTRY_LINK_RECORD_(hdrec.p_dentry);
			p_ndlink->_p.block = cdp.block;
			p_ndlink->_p.offset = cdp.offset;
			mgfs_buffer_write(p_cxt, hdrec.hb_dentry, hlock);

			// update parent record
			if(cmp < 0) {
				p_dlink->_l.block = p_rec_pos->block;
				p_dlink->_l.offset = p_rec_pos->offset;
			} else {
				p_dlink->_r.block = p_rec_pos->block;
				p_dlink->_r.offset = p_rec_pos->offset;
			}

			mgfs_buffer_write(p_cxt, hcdr.hb_dentry, hlock);
			mgfs_dentry_close(p_cxt, &hcdr, hlock);

			r = true;
		}

		mgfs_dentry_close(p_cxt, &hdrec, hlock);
	}
	
	return r;
}

static bool dentry_record_detach(_mgfs_context_t *p_cxt, _h_inode_ hdir, _dentry_position_t *p_rec_pos, // in
				_h_dentry_ hdrec, _h_dentry_ hdparent, // out
				_h_lock_ hlock) {
	bool r = false;

	if(dentry_by_position(p_cxt, hdir, p_rec_pos, hdrec, hlock)) {
		_dentry_link_t *p_dlink = _DENTRY_LINK_RECORD_(hdrec->p_dentry);
		_dentry_position_t dp_rec_parent;
		dp_rec_parent.block = p_dlink->_p.block;
		dp_rec_parent.offset = p_dlink->_p.offset;

		if(dentry_by_position(p_cxt, hdir, &dp_rec_parent, hdparent, hlock)) {
			_dentry_link_t *p_link = _DENTRY_LINK_RECORD_(hdparent->p_dentry);
			if(p_link->_l.block == p_rec_pos->block && p_link->_l.offset == p_rec_pos->offset) {
				p_link->_l.block = INVALID_BLOCK_NUMBER;
				p_link->_l.offset = INVALID_BLOCK_OFFSET;
				r = true;
			}

			if(p_link->_r.block == p_rec_pos->block && p_link->_r.offset == p_rec_pos->offset) {
				p_link->_r.block = INVALID_BLOCK_NUMBER;
				p_link->_r.offset = INVALID_BLOCK_OFFSET;
				r = true;
			}

			if(r)
				mgfs_buffer_write(p_cxt, hdparent->hb_dentry, hlock);
		}
	}

	return r;
}

bool mgfs_dentry_create(_mgfs_context_t *p_cxt, _h_inode_ hdir, _str_t name, _u32 sz_name, // in
			_u32 inode_number, _u8 flags, // in
			_h_dentry_ hd, // out
			_h_lock_ hlock
		       ) {
	bool r = false;

	_dentry_position_t dp_last;
	_dentry_position_t dp_inplace;
	_dentry_position_t dp_deleted;

	dp_last.block = dp_inplace.block = dp_deleted.block = INVALID_BLOCK_NUMBER;
	dp_last.offset = dp_inplace.offset = dp_deleted.offset = INVALID_BLOCK_OFFSET;

	if(dentry_find(p_cxt, hdir, name, sz_name, &dp_last, &dp_inplace, &dp_deleted, hlock) == false) {
		if(dp_deleted.block != INVALID_BLOCK_NUMBER) {
			// replace record
			_mgfs_dentry_handle_t hdparent;
			hd->hb_dentry = hdparent.hb_dentry = INVALID_BUFFER_HANDLE;

			// detach deleted record
			if(dentry_record_detach(p_cxt, hdir, &dp_deleted, hd, &hdparent, hlock)) {
				_dentry_link_t *p_dlink = _DENTRY_LINK_RECORD_(hd->p_dentry);
				if(p_dlink->_l.block != INVALID_BLOCK_NUMBER && p_dlink->_l.offset != INVALID_BLOCK_OFFSET)
					dentry_record_attach(p_cxt, hdir, &p_dlink->_l, &p_dlink->_p, hlock);
				if(p_dlink->_r.block != INVALID_BLOCK_NUMBER && p_dlink->_r.offset != INVALID_BLOCK_OFFSET)
					dentry_record_attach(p_cxt, hdir, &p_dlink->_r, &p_dlink->_p, hlock);
			
				///// replace ////
				hd->p_dentry->name_size = sz_name;
				hd->p_dentry->flags = flags;
				hd->p_dentry->inode_number = inode_number;
				p_dlink->_l.block = p_dlink->_r.block = p_dlink->_p.block = INVALID_BLOCK_NUMBER;
				p_dlink->_l.offset = p_dlink->_r.offset = p_dlink->_p.offset = INVALID_BLOCK_OFFSET;
				mem_cpy((_u8 *)_DENTRY_NAME_(hd->p_dentry), (_u8 *)name, sz_name);
				//////////////////

				// attach
				r = dentry_record_attach(p_cxt, hdir, &dp_deleted, &dp_last, hlock);
			}
			
			mgfs_dentry_close(p_cxt, &hdparent, hlock);
		} else {
			if(dp_inplace.block != INVALID_BLOCK_NUMBER) {
				// shrinking record to fits a new one inside 
				_mgfs_dentry_handle_t hdinplace; // handle of record to shrink
				if(dentry_by_position(p_cxt, hdir, &dp_inplace, &hdinplace, hlock)) {
					_u16 nrec_sz = sizeof(_mgfs_dir_record_t) + sz_name;
					// shrinc inplace record
					hdinplace.p_dentry->record_size -= nrec_sz;

					_dentry_position_t dp_new;
					dp_new.block = dp_inplace.block;
					dp_new.offset = dp_inplace.offset + hdinplace.p_dentry->record_size;
					if(dentry_by_position(p_cxt, hdir, &dp_new, hd, hlock)) {
						hd->p_dentry->name_size = sz_name;
						hd->p_dentry->record_size = nrec_sz;
						hd->p_dentry->flags = flags;
						hd->p_dentry->inode_number = inode_number;
						_dentry_link_t *p_dlink = _DENTRY_LINK_RECORD_(hd->p_dentry);
						p_dlink->_l.block = p_dlink->_p.block = p_dlink->_r.block = INVALID_BLOCK_NUMBER;
						p_dlink->_l.offset = p_dlink->_p.offset = p_dlink->_r.offset = INVALID_BLOCK_OFFSET;
						mem_cpy((_u8 *)_DENTRY_NAME_(hd->p_dentry), (_u8 *)name, sz_name);

						// attach
						r = dentry_record_attach(p_cxt, hdir, &dp_new, &dp_last, hlock);
					}

					mgfs_buffer_write(p_cxt, hdinplace.hb_dentry, hlock);
					mgfs_dentry_close(p_cxt, &hdinplace, hlock);
				}
			} else {
				// add new record to the end of file
				_u32 nblock = hdir->p_inode->dunits;
				if(mgfs_inode_append_block(p_cxt, hdir, 1, 0, hlock) == 1 && hdir->p_inode->dunits == nblock + 1) {
					_dentry_position_t dp_new;
					dp_new.block = nblock;
					dp_new.offset = 0;
					bool atach = false;

					if(hdir->p_inode->sz != 0)
						// first record
						atach = true;

					hdir->p_inode->sz += mgfs_unit_size(p_cxt);
					mgfs_inode_update(p_cxt, hdir, hlock);

					if(dentry_by_position(p_cxt, hdir, &dp_new, hd, hlock)) {
						hd->p_dentry->name_size = sz_name;
						hd->p_dentry->record_size = mgfs_unit_size(p_cxt);
						hd->p_dentry->flags = flags;
						hd->p_dentry->inode_number = inode_number;
						_dentry_link_t *p_dlink = _DENTRY_LINK_RECORD_(hd->p_dentry);
						p_dlink->_l.block = p_dlink->_p.block = p_dlink->_r.block = INVALID_BLOCK_NUMBER;
						p_dlink->_l.offset = p_dlink->_p.offset = p_dlink->_r.offset = INVALID_BLOCK_OFFSET;
						mem_cpy((_u8 *)_DENTRY_NAME_(hd->p_dentry), (_u8 *)name, sz_name);

						if(atach)
							// attach
							r = dentry_record_attach(p_cxt, hdir, &dp_new, &dp_last, hlock);
						else
							r = true;
					}
				}
			}
		}
	} else
		p_cxt->last_error = MGFS_DENTRY_EXISTS;

	return r;
}

static bool dentry_record_delete(_mgfs_context_t *p_cxt, _h_inode_ hdir, _dentry_position_t *p_rec_pos, _h_lock_ hlock) {
	bool r = false;

	_mgfs_dentry_handle_t hdrec;
	if(dentry_by_position(p_cxt, hdir, p_rec_pos, &hdrec, hlock)) {
		if(p_rec_pos->offset == 0) {
			hdrec.p_dentry->flags |= DENTRY_DELETED;
			mgfs_buffer_write(p_cxt, hdrec.hb_dentry, hlock);
			r = true;
		} else {
			// enumerating records in block, until the prev. record is found
			_dentry_position_t cdp; // current dentry position
			cdp.block = p_rec_pos->block;
			cdp.offset = 0;
			_u32 block_sz = mgfs_unit_size(p_cxt);
			_u16 offset = 0; // prev. record offset
			while(cdp.offset != p_rec_pos->offset && cdp.offset < (_u16)block_sz) {
				_u8 *ptr = (_u8 *)mgfs_buffer_ptr(p_cxt, hdrec.hb_dentry);
				_mgfs_dir_record_t *p_dr = (_mgfs_dir_record_t *)(ptr + cdp.offset);
				offset = cdp.offset;
				cdp.offset += p_dr->record_size;
			}

			if(cdp.offset < (_u16)block_sz && cdp.offset == p_rec_pos->offset) {
				_mgfs_dentry_handle_t hdtmp, hdparent;
				hdtmp.hb_dentry = hdparent.hb_dentry = INVALID_BUFFER_HANDLE;

				if(dentry_record_detach(p_cxt, hdir, p_rec_pos, &hdtmp, &hdparent, hlock)) {
					_dentry_link_t *p_dlink = _DENTRY_LINK_RECORD_(hdtmp.p_dentry);
					
					if(p_dlink->_l.block != INVALID_BLOCK_NUMBER && p_dlink->_l.offset != INVALID_BLOCK_OFFSET)
						dentry_record_attach(p_cxt, hdir, &p_dlink->_l, &p_dlink->_p, hlock);
					if(p_dlink->_r.block != INVALID_BLOCK_NUMBER && p_dlink->_r.offset != INVALID_BLOCK_OFFSET)
						dentry_record_attach(p_cxt, hdir, &p_dlink->_r, &p_dlink->_p, hlock);


					_u8 *ptr = (_u8 *)mgfs_buffer_ptr(p_cxt, hdrec.hb_dentry);
					_mgfs_dir_record_t *p_dr_prev = (_mgfs_dir_record_t *)(ptr + offset);
					p_dr_prev->record_size += hdrec.p_dentry->record_size;
					ptr = (_u8 *)hdrec.p_dentry;
					mem_set(ptr, 0, hdrec.p_dentry->record_size);
					mgfs_buffer_write(p_cxt, hdrec.hb_dentry, hlock);
					r = true;
				}
				
				mgfs_dentry_close(p_cxt, &hdtmp, hlock);
				mgfs_dentry_close(p_cxt, &hdparent, hlock);
			}
		}

		mgfs_dentry_close(p_cxt, &hdrec, hlock);
	}

	return r;
}

bool mgfs_dentry_rename(_mgfs_context_t *p_cxt, _h_inode_ hdir, _str_t name, _str_t new_name, _h_lock_ hlock) {
	bool r = false;

	_dentry_position_t dp_last;
	_dentry_position_t dp_inplace;
	_dentry_position_t dp_deleted;

	dp_last.block = dp_inplace.block = dp_deleted.block = INVALID_BLOCK_NUMBER;
	dp_last.offset = dp_inplace.offset = dp_deleted.offset = INVALID_BLOCK_OFFSET;

	if(dentry_find(p_cxt, hdir, new_name, str_len(new_name), &dp_last, &dp_inplace, &dp_deleted, hlock) == false) {
		if(dentry_find(p_cxt, hdir, name, str_len(name), &dp_last, &dp_inplace, &dp_deleted, hlock)) {
			_mgfs_dentry_handle_t hdtmp;
			hdtmp.hb_dentry = INVALID_BUFFER_HANDLE;
			if(dentry_by_position(p_cxt, hdir, &dp_last, &hdtmp, hlock)) {
				_str_t _name = new_name;
				_u32 inode_number = hdtmp.p_dentry->inode_number;
				_u8 flags = hdtmp.p_dentry->flags;
				_u16 name_size = str_len(new_name) + 1;
				bool free_name = false;
				if(hdtmp.p_dentry->flags & DENTRY_LINK) {
					name_size = str_len(new_name) + str_len(_DENTRY_LINK_NAME_(hdtmp.p_dentry)) + 2;
					_name = (_str_t)mgfs_mem_alloc(p_cxt, name_size);
					if(_name) {
						mem_cpy((_u8 *)_name, (_u8 *)new_name, str_len(new_name+1));
						mem_cpy((_u8 *)(_name + str_len(new_name) + 1), 
							(_u8 *)(_DENTRY_LINK_NAME_(hdtmp.p_dentry)),
							str_len(_DENTRY_LINK_NAME_(hdtmp.p_dentry)) + 1);

						free_name = true;
					}
				}

				if(dentry_record_delete(p_cxt, hdir, &dp_last, hlock)) {
					_mgfs_dentry_handle_t hdnew;
					hdnew.hb_dentry = INVALID_BUFFER_HANDLE;
					r = mgfs_dentry_create(p_cxt, hdir, _name, name_size, inode_number, flags, &hdnew, hlock);
					mgfs_dentry_close(p_cxt, &hdnew, hlock);
				}

				if(free_name)
					mgfs_mem_free(p_cxt, _name, name_size);

				mgfs_dentry_close(p_cxt, &hdtmp, hlock);
			}
		}
	} else
		p_cxt->last_error = MGFS_DENTRY_EXISTS;

	return r;
}

bool mgfs_dentry_delete(_mgfs_context_t *p_cxt, _h_inode_ hdir, _str_t name, _h_lock_ hlock) {
	bool r = false;

	_dentry_position_t dp_last;
	_dentry_position_t dp_inplace;
	_dentry_position_t dp_deleted;

	dp_last.block = dp_inplace.block = dp_deleted.block = INVALID_BLOCK_NUMBER;
	dp_last.offset = dp_inplace.offset = dp_deleted.offset = INVALID_BLOCK_OFFSET;

	if(dentry_find(p_cxt, hdir, name, str_len(name), &dp_last, &dp_inplace, &dp_deleted, hlock))
		r = dentry_record_delete(p_cxt, hdir, &dp_last, hlock);

	return r;
}

void mgfs_dentry_close(_mgfs_context_t *p_cxt, _h_dentry_ hd, _h_lock_ hlock) {
	mgfs_buffer_free(p_cxt, hd->hb_dentry, hlock);
	hd->hb_dentry = INVALID_BUFFER_HANDLE;
}

static void dentry_list(_mgfs_context_t *p_cxt, _h_inode_ hd, _dentry_position_t *p_dpos, 
			_mgfs_dentry_list_t *p_callback, void *p_udata, _h_lock_ hlock=0) {
	_mgfs_dentry_handle_t hdentry;
	if(dentry_by_position(p_cxt, hd, p_dpos, &hdentry, hlock)) {
		_mgfs_inode_handle_t hinode;
		hinode.hb_inode = INVALID_BUFFER_HANDLE;
		hinode.p_inode = 0;
		_u16 iflags=0;
		_u8 dflags=0;
		_u64 isize = 0;
		_str_t link_path = 0;
		_u32 mo=0, cr=0; // creation and modification timestamp
		_u32 owner=0;

		if(hdentry.p_dentry->inode_number != INVALID_INODE_NUMBER)
			mgfs_inode_open(p_cxt, hdentry.p_dentry->inode_number, &hinode, hlock);

		if(hinode.p_inode && hinode.hb_inode != INVALID_BUFFER_HANDLE) {
			iflags = hinode.p_inode->flags;
			isize = hinode.p_inode->sz;
			mo = hinode.p_inode->mo;
			cr = hinode.p_inode->cr;
			owner = hinode.p_inode->owner;
			mgfs_inode_close(p_cxt, &hinode, hlock);
		}

		dflags = hdentry.p_dentry->flags;
		if(dflags & DENTRY_LINK)
			link_path = _DENTRY_LINK_NAME_(hdentry.p_dentry);

		_dentry_link_t *p_dlink = _DENTRY_LINK_RECORD_(hdentry.p_dentry);
		if(p_dlink->_l.block != INVALID_BLOCK_NUMBER && p_dlink->_l.offset != INVALID_BLOCK_OFFSET)
			dentry_list(p_cxt, hd, &p_dlink->_l, p_callback, p_udata, hlock);
		
		if(!(iflags & (MGFS_DELETED|MGFS_HIDDEN)))
			p_callback(iflags, dflags, _DENTRY_NAME_(hdentry.p_dentry), 
					link_path, cr, mo, owner, isize, p_udata);
		
		if(p_dlink->_r.block != INVALID_BLOCK_NUMBER && p_dlink->_r.offset != INVALID_BLOCK_OFFSET)
			dentry_list(p_cxt, hd, &p_dlink->_r, p_callback, p_udata, hlock);

		mgfs_dentry_close(p_cxt, &hdentry, hlock);
	}
}

void mgfs_dentry_list(_mgfs_context_t *p_cxt, _h_inode_ hdir, 
			_mgfs_dentry_list_t *p_callback, void *p_udata, _h_lock_ hlock) {
	if(mgfs_is_valid_inode_handle(hdir)) {
		if(hdir->p_inode->flags & MGFS_DIR) {
			_dentry_position_t dpos;
			dpos.block = 0;
			dpos.offset = 0;

			dentry_list(p_cxt, hdir, &dpos, p_callback, p_udata, hlock);
		}
	}
}

