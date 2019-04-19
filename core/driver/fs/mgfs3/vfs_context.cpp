#include "iVFS.h"
#include "vfs.h"
#include "iScheduler.h"
#include "iSystemLog.h"
#include "mgfs3.h"
#include "iRTC.h"
#include "file.h"
#include "inode.h"
#include "bitmap.h"
#include "buffer.h"
#include "dentry.h"

//#define _DEBUG_
#include "debug.h"

#ifdef _DEBUG_
#define	LOG(_fmt_, ...) \
	DBG(_fmt_, __VA_ARGS__)
#else
#define LOG(_fmt_, ...) \
	if(m_p_syslog) \
		m_p_syslog->fwrite(_fmt_, __VA_ARGS__) 
#endif

typedef struct {
	_mgfs_file_handle_t	handle;
	_u64			position;
} _mgfs_handle_data_t;

class cMGFS3_CXT:public iFSContext {
public:
	cMGFS3_CXT() {
		DRIVER_TYPE = DTYPE_FS;
		DRIVER_MODE = DMODE_BLOCK;
		DRIVER_CLASS= DCLASS_FSC;
		str_cpy(DRIVER_IDENT, (_str_t)"MGFS3-CXT", sizeof(DRIVER_IDENT));
		m_io_enable = false;
		m_io_lock = 0;
		m_p_fs_mutex = 0;
		m_p_broadcast = 0;
		m_p_fshandle = 0;
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}

	~cMGFS3_CXT() {}
	
	DECLARE_BASE(cMGFS3_CXT);
	DECLARE_INIT();
	DECLARE_DESTROY();
	DECLARE_DRIVER_INIT();
	DECLARE_DRIVER_UNINIT();
	
	bool format_storage(_u64 sz_dev, _u8 usize=1, _u8 flags=0, 
				_str_t serial=0, _u8 *vbr=0, _u8 *fsbr=0, _u32 sz_fsbr=0);
	void add_storage(FSHANDLE fsh);
	bool add_storage(iVolume *p_vol);
	bool remove_storage(FSHANDLE fsh);
	bool remove_storage(iVolume *p_vol);
	bool verify_storage(FSHANDLE hfs);
	bool verify_storage(iVolume *p_vol);
	void driver_ioctl(_u32 cmd, _driver_io_t *p_dio);
	void info(_fs_context_info_t *p_cxti);
	FSHANDLE root(void);
	void list(FSHANDLE hfile, _fs_list_t *p_cb_list, void *p_udata);

protected:
	iSystemLog 		*m_p_syslog;
	iHeap			*m_p_heap;
	iRTC			*m_p_rtc;
	FSHANDLE		m_hfs;	// storage handle
	_mgfs_context_t		m_cxt;
	FSHANDLE		m_hroot;
	_u64			m_pos;
	iBroadcast		*m_p_broadcast;
	HMUTEX			m_io_lock;
	bool			m_io_enable;
	iMutex			*m_p_fs_mutex;
	iFSHandle		*m_p_fshandle;

	void create_mgfs3_context(void);
	_mgfs_info_t *superblock(void);

	// sector I/O (return number of affected sectors)
	friend _u32 mgfs3_read_sector(void *p_io_cxt, _u32 sector, _u32 count, void *buffer);
	friend _u32 mgfs3_write_sector(void *p_io_cxt, _u32 sector, _u32 count, void *buffer);
	_u32 sector_io(bool write, _u32 sector, _u32 count, void *buffer);
	// memory allocation
	friend void *mgfs3_mem_alloc(void *p_io_cxt, _u32 size);
	friend void mgfs3_mem_free(void *p_io_cxt, void *ptr, _u32 size);
	// synchronization
	friend _h_lock_ mgfs3_lock(void *p_io_cxt, _h_lock_ hlock);
	friend void mgfs3_unlock(void *p_io_cxt, _h_lock_ hlock);
	friend _u32 mgfs3_timestamp(void *p_io_cxt);
	friend void mgfs3_info(void *p_io_cxt, _fs_info_t *p_fsi);
	friend void _mgfs3_cxt_msg_handler(_msg_t *p_msg, void *p_data);
	void create_superblock(_u64 sz_dev, _u8 usize, _u8 flags, _str_t serial);
	void create_meta_group(_u64 sz_dev);
	void install_vbr(_u8 *p_vbr);
	void install_fsbr(_u8 *p_fsbr, _u32 sz_fsbr);
	bool compare_handle(FSHANDLE h1, FSHANDLE h2);
	void msg_handler(_msg_t *p_msg);
	_mgfs_handle_data_t *get_handle_data(FSHANDLE h);
	_h_file_ get_mgfs_handle(FSHANDLE h);
	_u32 mgfs_pmask(FSHANDLE h);
};

void _mgfs3_cxt_msg_handler(_msg_t *p_msg, void *p_data) {
	cMGFS3_CXT *p_obj = (cMGFS3_CXT *)p_data;
	p_obj->msg_handler(p_msg);
}

IMPLEMENT_BASE(cMGFS3_CXT, VFS_CONTEXT_NAME, 0x0001);
IMPLEMENT_INIT(cMGFS3_CXT, rbase) {
	REPOSITORY = rbase;
	INIT_MUTEX();
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	m_p_heap = (iHeap *)OBJECT_BY_INTERFACE(I_HEAP, RY_STANDALONE);
	m_p_rtc = (iRTC *)OBJECT_BY_INTERFACE(I_RTC, RY_STANDALONE);
	m_hfs = 0;
	m_hroot = 0;
	m_pos = 0;
	if((m_p_broadcast = (iBroadcast *)OBJECT_BY_INTERFACE(I_BROADCAST, RY_STANDALONE)))
		m_p_broadcast->add_handler(_mgfs3_cxt_msg_handler, this);
	m_p_fs_mutex = (iMutex *)OBJECT_BY_INTERFACE(I_MUTEX, RY_CLONE);
	m_p_fshandle = (iFSHandle *)OBJECT_BY_INTERFACE(I_FS_HANDLE, RY_STANDALONE);
	return true;
}
IMPLEMENT_DESTROY(cMGFS3_CXT) {
	RELEASE_MUTEX();
	RELEASE_OBJECT(m_p_heap);
	RELEASE_OBJECT(m_p_rtc);
	RELEASE_OBJECT(m_p_fs_mutex);
	if(m_p_broadcast)
		m_p_broadcast->remove_handler(_mgfs3_cxt_msg_handler);
	RELEASE_OBJECT(m_p_broadcast);
}
IMPLEMENT_DRIVER_INIT(cMGFS3_CXT, host) {
	_fs_version_ = MGFS_VERSION;
	if(!(DRIVER_HOST = host)) {
		iDriverRoot *p_droot = GET_DRIVER_ROOT();
		if(p_droot) {
			_driver_request_t dr;
			dr.flags = DRF_IDENT|DRF_OBJECT_NAME;
			dr.DRIVER_IDENT = VFS_IDENT;
			dr.DRIVER_NAME = VFS_OBJECT_NAME;
			iFS *p_mgfs = (iFS *)p_droot->driver_request(&dr);
			if(p_mgfs) {
				DRIVER_HOST = p_mgfs;
				m_io_enable = true;
			}
		}
	} else
		m_io_enable = true;
	return m_io_enable;
}
IMPLEMENT_DRIVER_UNINIT(cMGFS3_CXT) {
	if(m_hroot) {
		fclose(m_hroot);
		fclose(m_hfs);
	}
	mgfs_buffer_cleanup(&m_cxt);
}

void cMGFS3_CXT::msg_handler(_msg_t *p_msg) {
	switch(p_msg->msg) {
		case MSG_DRIVER_UNINIT: {
				iDriverBase *p_drv = (iDriverBase *)p_msg->data;
				if(p_drv == this) {
					HMUTEX hlock = lock();
					m_io_enable = false;
					unlock(hlock);
				}
			}
			break;
	}
}

_mgfs_info_t *cMGFS3_CXT::superblock(void) {
	_mgfs_info_t *r = 0;
	if(m_cxt.fs.magic == MGFS_MAGIC && m_cxt.fs.version == MGFS_VERSION) {
		m_cxt.flags &= ~MGFS_DELAY_WRITE;
		r = &m_cxt.fs;
	}
	return r;
}

_str_t _g_mgfs3_fs_name_ = (_str_t)"mgfs3";

_mgfs_handle_data_t *cMGFS3_CXT::get_handle_data(FSHANDLE h) {
	return (_mgfs_handle_data_t *)m_p_fshandle->data(h);
}

_h_file_ cMGFS3_CXT::get_mgfs_handle(FSHANDLE h) {
	_h_file_ r = 0;
	_mgfs_handle_data_t *p_hdata = get_handle_data(h);
	if(p_hdata)
		r = &p_hdata->handle;
	return r;
}

void cMGFS3_CXT::info(_fs_context_info_t *p_cxti) {
	mem_set((_u8 *)p_cxti, 0, sizeof(_fs_context_info_t));
	if(m_hfs)
		p_cxti->nitems = 1; // single storage
	else
		p_cxti->nitems = 0;
	_mgfs_info_t *p_sb = superblock();
	switch(m_p_fshandle->type(m_hfs)) {
		case HTYPE_VOLUME: {
				iVolume *p_vol = (iVolume *)m_p_fshandle->driver(m_hfs);
				if(p_vol) {
					p_cxti->pbsize = p_vol->_vol_block_size_;
					p_cxti->tpbnum = p_vol->_vol_size_;
				}
				if(p_sb) {
					p_cxti->lbsize = p_sb->sz_sector * p_sb->sz_unit;
					p_cxti->pbsize = p_sb->sz_sector;
					p_cxti->tpbnum = (p_sb->sz_fs * p_cxti->lbsize) / p_cxti->pbsize;
					p_cxti->serial = (_str_t)p_sb->serial;
					p_cxti->fsname = _g_mgfs3_fs_name_;
				}
			}
			break;
		case HTYPE_FSCONTEXT:
		case HTYPE_FILE: {
				iFSContext *p_fscxt = (iFSContext *)m_p_fshandle->driver(m_hfs);
				if(p_fscxt == 0 || p_fscxt == this) {
					if(p_sb) {
						p_cxti->lbsize = p_sb->sz_sector * p_sb->sz_unit;
						p_cxti->pbsize = p_sb->sz_sector;
						p_cxti->tpbnum = (p_sb->sz_fs * p_cxti->lbsize) / p_cxti->pbsize;
						p_cxti->serial = (_str_t)p_sb->serial;
						p_cxti->fsname = _g_mgfs3_fs_name_;
					}
				} else
					p_fscxt->info(p_cxti);
			}
			break;
	}
}

FSHANDLE cMGFS3_CXT::root(void) {
	FSHANDLE r = m_hroot;
	if(!r) {
		_u8 raw_hdata[FS_MAX_HANDLE_DATA];
		mem_set(raw_hdata, 0, sizeof(raw_hdata));
		_mgfs_handle_data_t *p_hdata = (_mgfs_handle_data_t *)raw_hdata;

		if(mgfs_file_open_root(&m_cxt, &p_hdata->handle)) {
			_fs_unit_size_ = mgfs_unit_size(&m_cxt);
			_fs_size_ = m_cxt.fs.sz_fs;

			if((r = m_hroot = m_p_fshandle->create(this, 0, HTYPE_FILE, (_u8 *)p_hdata)))
				m_p_fshandle->update_state(r, m_p_fshandle->state(r) | HSTATE_OPEN);
		}
	}
	return r;
}

void cMGFS3_CXT::create_superblock(_u64 sz_dev, _u8 usize,  _u8 flags, _str_t serial) {
	m_cxt.fs.sz = sizeof(_mgfs_info_t);
	m_cxt.fs.magic = MGFS_MAGIC;
	m_cxt.fs.rzv = MGFS_RESERVED_SECTORS;
	m_cxt.fs.sz_addr = sizeof(_u32);
	m_cxt.fs.version = MGFS_VERSION;
	m_cxt.fs.flags = flags;
	m_cxt.fs.sz_sector = MGFS_SECTOR_SIZE;
	m_cxt.fs.sz_unit = usize;
	mem_set((_u8 *)&m_cxt.fs.meta, 0, sizeof(_mgfs_inode_record_t));
	mem_set((_u8 *)&m_cxt.fs.meta.location, 0xff, sizeof(m_cxt.fs.meta.location));
	m_cxt.fs.mgb_unit = INVALID_UNIT_NUMBER;
	m_cxt.fs.sz_fs = sz_dev / (MGFS_SECTOR_SIZE * usize);
	if(serial)
		str_cpy((_str_t)m_cxt.fs.serial, serial, sizeof(m_cxt.fs.serial));
	else
		str_cpy((_str_t)m_cxt.fs.serial, (_str_t)"MGFS3", sizeof(m_cxt.fs.serial));
	m_cxt.seq_alloc_unit = MGFS_RESERVED_SECTORS / usize;
	m_cxt.seq_alloc_unit += (MGFS_RESERVED_SECTORS / usize) ? 1 : 0;
}

void cMGFS3_CXT::create_meta_group(_u64 sz_dev) {
	bool bshadow = (m_cxt.fs.flags & MGFS_USE_BITMAP_SHADOW) ? true : false;
	if(bshadow)// turn off bitmap shadow
		m_cxt.fs.flags &= ~MGFS_USE_BITMAP_SHADOW;

	m_cxt.flags |= MGFS_SEQUENTIAL_ALLOC;

	// create empty metagroup //
	_mgfs_inode_record_t inode_record;
	mem_set((_u8 *)&inode_record, 0, sizeof(_mgfs_inode_record_t));
	mem_set((_u8 *)&inode_record.location, 0xff, sizeof(inode_record.location));

	_u32 meta_unit = INVALID_UNIT_NUMBER;
	if(mgfs_bitmap_alloc(&m_cxt, &meta_unit, 1, 0) == 1) {
		_h_buffer_ hb_meta = mgfs_buffer_alloc(&m_cxt, meta_unit);
		if(hb_meta != INVALID_BUFFER_HANDLE) {
			_mgfs_inode_record_t *p_inode = (_mgfs_inode_record_t *)mgfs_buffer_ptr(&m_cxt, hb_meta);

			for(_u32 i = 0; i < META_GROUP_MEMBERS; i++) {
				mem_cpy((_u8 *)p_inode, (_u8 *)&inode_record, sizeof(_mgfs_inode_record_t));
				p_inode->cr = mgfs_timestamp(&m_cxt);
				p_inode++;
			}

			m_cxt.fs.meta.cr = mgfs_timestamp(&m_cxt);
			m_cxt.fs.meta.sz = META_GROUP_MEMBERS * sizeof(_mgfs_inode_record_t);
			m_cxt.fs.meta.dunits = 1;
			m_cxt.fs.meta.location[0] = meta_unit;

			mgfs_buffer_write(&m_cxt, hb_meta);
			mgfs_buffer_free(&m_cxt, hb_meta);
		} else
			LOG("MGFS3: failed to alloc meta buffer !\r\n", "");
	} else
		LOG("MGFS3: failed to alloc bitmap !\r\n", "");
	/////////////////////////////////


	// create space bitmap //
	_u64 device_units = sz_dev / mgfs_unit_size(&m_cxt);
	_u64 bitmaps = (device_units / MAX_BITMAP_BITS) + ((device_units % MAX_BITMAP_BITS) ? 1 : 0);
	_u64 sb_file_size = bitmaps * sizeof(_mgfs_bitmap_t);
	_u32 sb_file_units = (sb_file_size / mgfs_unit_size(&m_cxt)) + 
				((sb_file_size % mgfs_unit_size(&m_cxt)) ? 1 : 0);
	
	_mgfs_inode_handle_t hsbinode;
	if(mgfs_inode_meta_open(&m_cxt, SPACE_BITMAP_IDX, &hsbinode) == 0) {
		mgfs_inode_append_block(&m_cxt, &hsbinode, sb_file_units, 0);
		hsbinode.p_inode->sz = sb_file_size;
		mgfs_inode_update(&m_cxt, &hsbinode);
		mgfs_inode_close(&m_cxt, &hsbinode);
	} else
		LOG("MGFS3: failed to open metafile %d\r\n", SPACE_BITMAP_IDX);
	// create space bitmap shadow
	if(bshadow) {
		if(mgfs_inode_meta_open(&m_cxt, SPACE_BITMAP_SHADOW_IDX, &hsbinode) == 0) {
			mgfs_inode_append_block(&m_cxt, &hsbinode, sb_file_units, 0);
			hsbinode.p_inode->sz = sb_file_size;
			mgfs_inode_update(&m_cxt, &hsbinode);
			mgfs_inode_close(&m_cxt, &hsbinode);
		}
		m_cxt.fs.flags |= MGFS_USE_BITMAP_SHADOW;
	}

	// sequential allocator do not needed any more
	m_cxt.flags &= ~MGFS_SEQUENTIAL_ALLOC;

	// reserve the sequentially allocated units
	if(mgfs_inode_meta_open(&m_cxt, SPACE_BITMAP_IDX, &hsbinode) == 0) {
		_u32 unit_map[256];
		_u32 uidx = 0;
		_mgfs_inode_handle_t hbshadow;

		if(bshadow) {
			if(mgfs_inode_meta_open(&m_cxt, SPACE_BITMAP_SHADOW_IDX, &hbshadow) != 0)
				bshadow = false;
		}

		for(_u32 i = 0; i < m_cxt.seq_alloc_unit; i++) {
			unit_map[uidx] = i;
			uidx++;
			if(uidx >= 256) {
				mgfs_bitmap_set_array(&m_cxt, &hsbinode, unit_map, uidx, true);
				if(bshadow)
					mgfs_bitmap_sync(&m_cxt, unit_map, uidx, &hsbinode, &hbshadow, true);
				
				uidx = 0;
			}
		}
		
		mgfs_bitmap_set_array(&m_cxt, &hsbinode, unit_map, uidx, true);
		if(bshadow) {
			mgfs_bitmap_sync(&m_cxt, unit_map, uidx, &hsbinode, &hbshadow, true);
			mgfs_inode_close(&m_cxt, &hbshadow);
		}

		mgfs_inode_close(&m_cxt, &hsbinode);
	} else
		LOG("MGFS3: failed to open metafile %d\r\n", SPACE_BITMAP_IDX);

	// create root directory
	_mgfs_inode_handle_t hroot;
	if((m_cxt.fs.root_inode = mgfs_inode_create(&m_cxt, MGFS_DIR, 0, &hroot)) != INVALID_INODE_NUMBER) {
		_mgfs_dentry_handle_t hdthis, hdparent;
		mgfs_dentry_create(&m_cxt, &hroot, (_str_t)".", 2, m_cxt.fs.root_inode, 0, &hdthis);
		mgfs_dentry_create(&m_cxt, &hroot, (_str_t)"..", 3, m_cxt.fs.root_inode, 0, &hdparent);
		mgfs_dentry_close(&m_cxt, &hdthis);
		mgfs_dentry_close(&m_cxt, &hdparent);
		mgfs_inode_close(&m_cxt, &hroot);
		////////////////////////////////
	} else
		LOG("MGFS3: failed to create root inode !\r\n", "");

	mgfs_update_info(&m_cxt);
}

void cMGFS3_CXT::install_vbr(_u8 *p_vbr) {
	sector_io(true, 0, 1, p_vbr);
}
void cMGFS3_CXT::install_fsbr(_u8 *p_fsbr, _u32 sz_fsbr) {
	if(sz_fsbr <= (MAX_FSBR_SECTORS * MGFS_SECTOR_SIZE)) {
		_u32 fsbr_sectors = sz_fsbr / MGFS_SECTOR_SIZE;
		fsbr_sectors += (sz_fsbr % MGFS_SECTOR_SIZE) ? 1 : 0;
		sector_io(true, FIRST_FSBR_SECTOR, fsbr_sectors, p_fsbr);
	}
}
bool cMGFS3_CXT::format_storage(_u64 sz_dev, _u8 usize, _u8 flags, _str_t serial, 
				_u8 *vbr, _u8 *fsbr, _u32 sz_fsbr) {
	bool r = true;

	if(m_hfs) {
		_str_t dev_ident = (_str_t)"???";
		_u32 sz_sector = 512;
		switch(m_p_fshandle->type(m_hfs)) {
			case HTYPE_VOLUME: {
					iVolume *p_vol = (iVolume *)m_p_fshandle->driver(m_hfs);
					if(p_vol) {
						dev_ident = p_vol->DRIVER_IDENT;
						sz_sector = p_vol->_vol_block_size_;
					}

				}
				break;
			case HTYPE_FILE:
			case HTYPE_FSCONTEXT: {
					iFSContext *p_fscxt = (iFSContext *)m_p_fshandle->driver(m_hfs);
					if(p_fscxt) 
						dev_ident = p_fscxt->DRIVER_IDENT;
				}
				break;

		}
		LOG("\t\t\tCreate MGFS3 filesystem on device '%s'\r\n", dev_ident);
		LOG("\t\t\t%u MB, %u sectors\r\n", sz_dev / (1024*1024), sz_dev/sz_sector);
		if(vbr)
			install_vbr(vbr);
		if(fsbr)
			install_fsbr(fsbr, sz_fsbr);
		create_superblock(sz_dev, usize, flags, serial);
		create_meta_group(sz_dev);
	}

	return r;
}
// sector I/O (return number of affected sectors)
_u32 mgfs3_read_sector(void *p_io_cxt, _u32 sector, _u32 count, void *buffer) {
	cMGFS3_CXT *p = (cMGFS3_CXT *)p_io_cxt;
	return p->sector_io(false, sector, count, buffer);
}
_u32 mgfs3_write_sector(void *p_io_cxt, _u32 sector, _u32 count, void *buffer) {
	cMGFS3_CXT *p = (cMGFS3_CXT *)p_io_cxt;
	return p->sector_io(true, sector, count, buffer);
}
_u32 cMGFS3_CXT::sector_io(bool write, _u32 sector, _u32 count, void *buffer) {
	_u32 r = 0;
	_u64 offset = (_u64)(sector * MGFS_SECTOR_SIZE);
	_u32 size = (count * MGFS_SECTOR_SIZE);

	switch(m_p_fshandle->type(m_hfs)) {
		case HTYPE_VOLUME: {
				iVolume *p_vol = (iVolume *)m_p_fshandle->driver(m_hfs);
				if(p_vol) {
					if(write)
						p_vol->write(offset, (_u8 *)buffer, size, &r);
					else
						p_vol->read(offset, (_u8 *)buffer, size, &r);
				}
			}
			break;
		case HTYPE_FSCONTEXT: {
				iFSContext *p_fscxt = (iFSContext *)m_p_fshandle->driver(m_hfs);
				if(p_fscxt) {
					if(write)
						p_fscxt->write(offset, (_u8 *)buffer, size, &r);
					else
						p_fscxt->read(offset, (_u8 *)buffer, size, &r);
				}
			}
			break;
		case HTYPE_FILE: {
				iFSContext *p_fscxt = (iFSContext *)m_p_fshandle->driver(m_hfs);
				if(p_fscxt) {
					if(write)
						p_fscxt->write(offset, (_u8 *)buffer, size, &r, m_hfs);
					else
						p_fscxt->read(offset, (_u8 *)buffer, size, &r, m_hfs);
				}
			}
			break;
	}

	return r;
}
// memory allocation
void *mgfs3_mem_alloc(void *p_io_cxt, _u32 size) {
	cMGFS3_CXT *p = (cMGFS3_CXT *)p_io_cxt;
	return p->m_p_heap->alloc(size);
}
void mgfs3_mem_free(void *p_io_cxt, void *ptr, _u32 size) {
	cMGFS3_CXT *p = (cMGFS3_CXT *)p_io_cxt;
	p->m_p_heap->free(ptr, size);
}
// synchronization
_h_lock_ mgfs3_lock(void *p_io_cxt, _h_lock_ hlock) {
	cMGFS3_CXT *p = (cMGFS3_CXT *)p_io_cxt;
	_h_lock_ r = 0;
	if(p->m_p_fs_mutex)
		r = (_h_lock_)acquire_mutex(p->m_p_fs_mutex, (HMUTEX)hlock);
	return r;
}
void mgfs3_unlock(void *p_io_cxt, _h_lock_ hlock) {
	cMGFS3_CXT *p = (cMGFS3_CXT *)p_io_cxt;
	if(p->m_p_fs_mutex)
		p->m_p_fs_mutex->unlock((HMUTEX)hlock);
}
_u32 mgfs3_timestamp(void *p_io_cxt) {
	_u32 r = 0;
	cMGFS3_CXT *p = (cMGFS3_CXT *)p_io_cxt;
	if(p->m_p_rtc)
		r = p->m_p_rtc->timestamp();
	return r;
}
void mgfs3_info(void *p_io_cxt, _fs_info_t *p_fsi) {
	cMGFS3_CXT *p = (cMGFS3_CXT *)p_io_cxt;
	
	p_fsi->sector_size = p->m_cxt.fs.sz_sector;
	p_fsi->unit_size = p->m_cxt.fs.sz_unit;
	p_fsi->nunits = p->m_cxt.fs.sz_fs;
	p_fsi->fs_name = (_str_t)"mgfs3";
	p_fsi->fs_version = p->m_cxt.fs.version;
}

void cMGFS3_CXT::create_mgfs3_context(void) {
	m_cxt.fs_context.p_io_context = this;
	m_cxt.fs_context.write_back = false;
	m_cxt.fs_context.p_read = mgfs3_read_sector;
	m_cxt.fs_context.p_write = mgfs3_write_sector;
	m_cxt.fs_context.p_alloc = mgfs3_mem_alloc;
	m_cxt.fs_context.p_free = mgfs3_mem_free;
	m_cxt.fs_context.p_lock = mgfs3_lock;
	m_cxt.fs_context.p_unlock = mgfs3_unlock;
	m_cxt.fs_context.p_timestamp = mgfs3_timestamp;
	m_cxt.fs_context.p_info = mgfs3_info;
}

void cMGFS3_CXT::add_storage(FSHANDLE fsh) {
	if(!m_hfs) {
		m_hfs = fsh;
		create_mgfs3_context();
	}
}

bool cMGFS3_CXT::add_storage(iVolume *p_vol) {
	bool r = false;
	if(!m_hfs) {
		FSHANDLE h = m_p_fshandle->create(p_vol, 0, HTYPE_VOLUME);
		if(h) {
			m_p_fshandle->update_state(h, HSTATE_OPEN);
			add_storage(h);
			r = true;
		}
	}
	return r;
}

bool cMGFS3_CXT::compare_handle(FSHANDLE h1, FSHANDLE h2) {
	return m_p_fshandle->compare(h1, h2);
}

bool cMGFS3_CXT::remove_storage(FSHANDLE fsh) {
	bool r = false;

	if(compare_handle(fsh, m_hfs)) {
		fclose(m_hfs);
		m_p_fshandle->remove(m_hfs);
		m_hfs = 0;
		r = true;
	}

	return r;
}

bool cMGFS3_CXT::remove_storage(iVolume *p_vol) {
	bool r = false;
	iDriverBase *p_drv = m_p_fshandle->driver(m_hfs);
	if(p_drv) {
		if(p_drv->DRIVER_TYPE == DTYPE_VOL && p_drv == p_vol) {
			m_p_fshandle->remove(m_hfs);
			m_hfs = 0;
			r = true;
		}
	}
	return r;
}

bool cMGFS3_CXT::verify_storage(FSHANDLE hfs) {
	return compare_handle(hfs, m_hfs);
}

bool cMGFS3_CXT::verify_storage(iVolume *p_vol) {
	bool r = false;
	iDriverBase *p_drv = m_p_fshandle->driver(m_hfs);
	if(p_drv) {
		if(p_drv->DRIVER_TYPE == DTYPE_VOL) {
			if(p_drv == p_vol)
				r = true;
		}
	}
	return r;
}

typedef struct {
	_fs_list_t 	*p_cb_list;
	void		*p_udata;
}_list_data_t;

#define FSP_TO_MGFS_MASK	0x000007ff

void _mgfs3_dentry_list_(_u16 inode_flags, _u8 dentry_flags, 
			_str_t name, 
			_str_t link_path, 
			_u32 ct, _u32 mt, _u32 owner,
			_u64 size, void *p_udata) {
	_fs_file_info_t fi;
	_list_data_t *p_ld = (_list_data_t *)p_udata;

	if(!(dentry_flags & DENTRY_DELETED) && !(inode_flags & (MGFS_DELETED|MGFS_HIDDEN))) {
		fi.fname = name;
		fi.lname = link_path;
		fi.fsize = size;
		fi.ctime = ct;
		fi.mtime = mt;
		fi.pmask = inode_flags & FSP_TO_MGFS_MASK;
		if(dentry_flags & DENTRY_LINK)
			fi.pmask |= FSP_SLINK;
		fi.usrid = owner; // ??? should be transformed to user info ID ???
		p_ld->p_cb_list(&fi, p_ld->p_udata);
	}
}

void cMGFS3_CXT::list(FSHANDLE hdir, _fs_list_t *p_cb_list, void *p_udata) {
	_list_data_t ld;
	ld.p_cb_list = p_cb_list;
	ld.p_udata = p_udata;

	if(hdir && (m_p_fshandle->state(hdir) & HSTATE_OPEN)) {
		_mgfs_handle_data_t *p_hdata = (_mgfs_handle_data_t *)m_p_fshandle->data(hdir);
		if(p_hdata)
			mgfs_dentry_list(&m_cxt, &(p_hdata->handle.hinode), _mgfs3_dentry_list_, &ld);
	}
}

_u32 cMGFS3_CXT::mgfs_pmask(FSHANDLE h) {
	_u32 r = 0;
	_h_file_ hf = get_mgfs_handle(h);
	if(hf) {
		r = hf->hinode.p_inode->flags;
		if(hf->hdentry.p_dentry) {
			if(hf->hdentry.p_dentry->flags & DENTRY_LINK)
				r |= FSP_SLINK;
		}
	}
	return r;
}

void cMGFS3_CXT::driver_ioctl(_u32 cmd, _driver_io_t *p_dio) {
	p_dio->error = FS_RESULT_ERR;
	p_dio->result = 0;
	m_io_lock = lock(m_io_lock);

	if(!m_io_enable) {
		unlock(m_io_lock);
		return;
	}

	FSHANDLE h = p_dio->handle;
	_u8 hstate = m_p_fshandle->state(h);
	if(h && (hstate & HSTATE_OPEN)) {
		while(hstate & HSTATE_IO_PENDING) {
			sleep_systhread(1);
			hstate = m_p_fshandle->state(h);
		}

		if(!(hstate & HSTATE_OPEN)) {
			p_dio->error = FS_RESULT_INVALID_HANDLE;
			return;
		}

		iFSContext *p_fscxt = (iFSContext *)m_p_fshandle->driver(h);
		if(p_fscxt) {
			switch(m_p_fshandle->type(h)) {
				case HTYPE_FSCONTEXT:
				case HTYPE_FILE: {
						if(p_fscxt != this) {
							DBG("0x%h: redirect to 0x%h\r\n", this, p_fscxt);
							p_fscxt->driver_ioctl(cmd, p_dio);
							return;
						}
					}
					break;
				case HTYPE_VOLUME:
				case HTYPE_DEVICE:
					return;
			}
		} else 
			return;
		
		_u8 raw_hdata[FS_MAX_HANDLE_DATA];
		mem_set(raw_hdata, 0, sizeof(raw_hdata));
		m_p_fshandle->update_state(h, m_p_fshandle->state(h)|HSTATE_IO_PENDING);

		switch(cmd) {
			case DIOCTL_FOPEN: {
					FSHANDLE _h = m_p_fshandle->create(this, p_dio->user_id, HTYPE_FILE, raw_hdata);
					if(_h) {
						p_dio->_handle = 0;
						_h_file_ hdir = get_mgfs_handle(h);
						_h_file_ hfout = get_mgfs_handle(_h);
						if(hdir && hfout) {
							if(mgfs_file_open(&m_cxt, p_dio->name, hdir, hfout)) {
								m_p_fshandle->update_state(_h, HSTATE_OPEN);
								p_dio->_handle = _h;
								p_dio->error = FS_RESULT_OK;
							} else {
								DBG("MGFS3_CXT: failrd to opene '%s'\r\n", p_dio->name);
								m_p_fshandle->remove(_h);
							}
						} else
							m_p_fshandle->remove(_h);
					}
				}
				break;
			case DIOCTL_FCLOSE: {
					if(h != &m_hroot) {
						_h_file_ hf = get_mgfs_handle(h);
						if(hf) {
							mgfs_file_close(&m_cxt, hf);
							m_p_fshandle->remove(h);
							p_dio->error = FS_RESULT_OK;
						}
					}
				}
				break;
			case DIOCTL_FCREATE: {
					FSHANDLE _h = m_p_fshandle->create(this, p_dio->user_id, HTYPE_FILE, raw_hdata);
					if(_h) {
						p_dio->_handle = 0;
						_h_file_ hdir = get_mgfs_handle(h);
						_h_file_ hfout = get_mgfs_handle(_h);
						_u32 pmask = p_dio->pmask & FSP_TO_MGFS_MASK;
						if(mgfs_file_create(&m_cxt, p_dio->name, pmask, p_dio->user_id, hdir, hfout)) {
							m_p_fshandle->update_state(_h, HSTATE_OPEN);
							p_dio->_handle = _h;
							p_dio->error = FS_RESULT_OK;
						} else {
							DBG("MGFS3_CXT: failed to create file '%s'\r\n", p_dio->name);
							m_p_fshandle->remove(_h);
						}
					}
				}
				break;
			case DIOCTL_FDELETE: {
					_h_file_ hdir = get_mgfs_handle(h);
					if(mgfs_file_delete(&m_cxt, p_dio->name, hdir))
						p_dio->error = FS_RESULT_OK;
				}
				break;
			case DIOCTL_FLINK: {
					FSHANDLE _h = p_dio->_handle;
					if(_h && (m_p_fshandle->state(_h) & HSTATE_OPEN)) {
						if(m_p_fshandle->driver(_h) == this) {
							_h_file_ hdir = get_mgfs_handle(h);
							_h_file_ _hdir = get_mgfs_handle(_h);
							if(mgfs_file_create_hard_link(&m_cxt, hdir, 
									p_dio->name, _hdir, p_dio->_name))
								p_dio->error = FS_RESULT_OK;
						}
					}
				}
				break;
			case DIOCTL_FSLINK: {
					_h_file_ hdir = get_mgfs_handle(h);
					if(mgfs_file_create_soft_link(&m_cxt, p_dio->_name, 
									hdir, p_dio->name))
						p_dio->error = FS_RESULT_OK;
				}
				break;
			case DIOCTL_FMOVE: {
					FSHANDLE _h = p_dio->_handle;
					if(_h && (m_p_fshandle->state(_h) & HSTATE_OPEN)) {
						if(m_p_fshandle->driver(_h) == this) {
							_h_file_ hdir = get_mgfs_handle(h);
							_h_file_ _hdir = get_mgfs_handle(_h);
							if(mgfs_file_move(&m_cxt, hdir, 
									p_dio->name, _hdir, p_dio->_name))
								p_dio->error = FS_RESULT_OK;
						}
					}
				}
				break;
			case DIOCTL_SREAD: {
					if(!(mgfs_pmask(h) & (FSP_DIR|FSP_SLINK))) {
						_mgfs_handle_data_t *p_hdata = get_handle_data(h);
						_h_file_ hf = &p_hdata->handle;
						p_dio->result = mgfs_file_read(&m_cxt, hf, p_hdata->position, 
									p_dio->buffer, p_dio->size);
						p_hdata->position += p_dio->result;
						p_dio->error = FS_RESULT_OK;
					}
				}
				break;
			case DIOCTL_SWRITE: {
					if(!(mgfs_pmask(h) & (FSP_DIR|FSP_SLINK))) {
						_mgfs_handle_data_t *p_hdata = get_handle_data(h);
						_h_file_ hf = &p_hdata->handle;
						p_dio->result = mgfs_file_write(&m_cxt, hf, 
									p_hdata->position, p_dio->buffer, p_dio->size);
						p_hdata->position += p_dio->result;
						p_dio->error = FS_RESULT_OK;
					}
				}
				break;
			case DIOCTL_BREAD: {
					if(!(mgfs_pmask(h) & (FSP_DIR|FSP_SLINK))) {
						_mgfs_handle_data_t *p_hdata = get_handle_data(h);
						_h_file_ hf = &p_hdata->handle;
						_u64 offset = p_dio->lba * MGFS_SECTOR_SIZE;
						_u32 count = p_dio->count * MGFS_SECTOR_SIZE;

						if((p_dio->result = mgfs_file_read(&m_cxt, hf, offset, 
									p_dio->buffer, count))) {
							p_hdata->position = offset + p_dio->result;
							p_dio->error = FS_RESULT_OK;
						}
					}
				}
				break;
			case DIOCTL_BWRITE: {
					if(!(mgfs_pmask(h) & (FSP_DIR|FSP_SLINK))) {
						_mgfs_handle_data_t *p_hdata = get_handle_data(h);
						_h_file_ hf = &p_hdata->handle;
						_u64 offset = p_dio->lba * MGFS_SECTOR_SIZE;
						_u32 count = p_dio->count * MGFS_SECTOR_SIZE;

						if((p_dio->result = mgfs_file_write(&m_cxt, hf, offset, 
									p_dio->buffer, count))) {
							p_hdata->position = offset + p_dio->result;
							p_dio->error = FS_RESULT_OK;
						}
					}
				}
				break;
			case DIOCTL_FREAD: {
					if(!(mgfs_pmask(h) & (FSP_DIR|FSP_SLINK))) {
						_mgfs_handle_data_t *p_hdata = get_handle_data(h);
						_h_file_ hf = &p_hdata->handle;
						if((p_dio->result = mgfs_file_read(&m_cxt, hf, p_dio->offset, 
									p_dio->buffer, p_dio->size))) {
							p_hdata->position = p_dio->offset + p_dio->result;
							p_dio->error = FS_RESULT_OK;
						}
					}
				}
				break;
			case DIOCTL_FWRITE: {
					if(!(mgfs_pmask(h) & (FSP_DIR|FSP_SLINK))) {
						_mgfs_handle_data_t *p_hdata = get_handle_data(h);
						_h_file_ hf = &p_hdata->handle;
						if((p_dio->result = mgfs_file_write(&m_cxt, hf, p_dio->offset, 
									p_dio->buffer, p_dio->size))) {
							p_hdata->position = p_dio->offset + p_dio->result;
							p_dio->error = FS_RESULT_OK;
						}
					}
				}
				break;
			case DIOCTL_FSEEK: {
					if(!(mgfs_pmask(h) & (FSP_DIR|FSP_SLINK))) {
						_mgfs_handle_data_t *p_hdata = get_handle_data(h);
						_h_file_ hf = &p_hdata->handle;
						_u64 fsize = mgfs_file_size(hf);
						p_hdata->position = (p_dio->offset < fsize)?p_dio->offset:fsize;
						p_dio->error = FS_RESULT_OK;
					}
				}
				break;
			case DIOCTL_FPMASK: 
				p_dio->result = p_dio->pmask = mgfs_pmask(h);
				p_dio->error = FS_RESULT_OK;
				break;
			case DIOCTL_FSIZE: {
					_h_file_ hf = get_mgfs_handle(h);
					if(hf) {
						p_dio->result64 = mgfs_file_size(hf);
						p_dio->error = FS_RESULT_OK;
					}
				}
				break;
			case DIOCTL_FINFO: {
					_fs_file_info_t *p_fi = (_fs_file_info_t *)p_dio->p_data;
					_h_file_ hf = get_mgfs_handle(h);
					if(mgfs_is_valid_inode_handle(&hf->hinode)) {
						if(m_p_fshandle->type(h) == HTYPE_FILE) {
							p_fi->fname = mgfs_dentry_name(&(hf->hdentry));
							p_fi->lname = mgfs_dentry_link_name(&(hf->hdentry));
							p_fi->fsize = mgfs_file_size(hf);
							if(hf->hinode.p_inode) {
								p_fi->ctime = hf->hinode.p_inode->cr;
								p_fi->mtime = hf->hinode.p_inode->mo;
								p_fi->usrid = hf->hinode.p_inode->owner; //??? transform ???
							} else {
								p_fi->ctime = p_fi->mtime = p_fi->usrid = 0;
							}
							p_fi->pmask = mgfs_pmask(h);
							p_dio->error = FS_RESULT_OK;
						}
					}
				}
				break;
		}
		m_p_fshandle->update_state(h, m_p_fshandle->state(h) & ~HSTATE_IO_PENDING);
	} else
		p_dio->error = FS_RESULT_INVALID_HANDLE;
	unlock(m_io_lock);
}

