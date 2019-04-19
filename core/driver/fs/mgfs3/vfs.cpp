#include "iVFS.h"
#include "vfs.h"
#include "mgfs3.h"
#include "iSystemLog.h"

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

class cMGFS3_VFS: public iFS {
public:
	cMGFS3_VFS() {
		m_p_syslog = 0;
		DRIVER_TYPE = DTYPE_FS;
		DRIVER_MODE = DMODE_BLOCK;
		DRIVER_CLASS= DCLASS_VFS;
		str_cpy(DRIVER_IDENT, (_str_t)VFS_IDENT, sizeof(DRIVER_IDENT));
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}

	DECLARE_BASE(cMGFS3_VFS);
	DECLARE_INIT();
	DECLARE_DESTROY();
	DECLARE_DRIVER_INIT();
	DECLARE_DRIVER_UNINIT();

	~cMGFS3_VFS() {}

	bool ident(FSHANDLE hfs);
	bool ident_volume(iVolume *p_vol);
	bool ident_context(iFSContext *p_cxt);
	bool format(FSHANDLE hstorage, 
			_u64 sz_dev, 
			_u8 usize=1, 
			_u8 flags=0, 
			_str_t serial=0, 
			_u8 *vbr=0, 
			_u8 *fsbr=0, 
			_u32 sz_fsbr=0
		);
	iFSContext *create_context(void);
	void destroy_context(iFSContext *p_cxt);
	iFSContext *get_context(_u32 cidx);
	iFSContext *get_context(FSHANDLE hstorage);
	iFSContext *get_context(iVolume *p_vol);

protected:
	iSystemLog *m_p_syslog;

	bool ident_file(iFSContext *p_cxt, FSHANDLE hfile);
	bool ident_superblock(_u8 *p_buffer);
};

IMPLEMENT_BASE(cMGFS3_VFS, VFS_OBJECT_NAME, 0x0001);
IMPLEMENT_INIT(cMGFS3_VFS, rbase) {
	REPOSITORY = rbase;
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);

	REGISTER_DRIVER();
	return true;
}
IMPLEMENT_DESTROY(cMGFS3_VFS) {
}
IMPLEMENT_DRIVER_INIT(cMGFS3_VFS, host) {
	bool r = false;

	if(!(DRIVER_HOST = host)) {
		_driver_request_t dr;
		dr.flags = DRF_INTERFACE;
		dr.DRIVER_INTERFACE = I_FS_HOST;

		iDriverRoot *p_droot = GET_DRIVER_ROOT();
		if(p_droot) {
			iFSHost *p_fs_host = (iFSHost *)p_droot->driver_request(&dr);
			if(p_fs_host) {
				DRIVER_HOST = p_fs_host;
				r = true;
			}
			RELEASE_OBJECT(p_droot);
		}
	} else
		r = true;

	return r;
}
IMPLEMENT_DRIVER_UNINIT(cMGFS3_VFS) {
}

bool cMGFS3_VFS::format(FSHANDLE hstorage, 
			_u64 sz_dev, 
			_u8 usize, 
			_u8 flags, 
			_str_t serial, 
			_u8 *vbr, 
			_u8 *fsbr, 
			_u32 sz_fsbr
		) {
	bool r = false;
	iFSContext *p_cxt = create_context();
	if(p_cxt) {
		p_cxt->add_storage(hstorage);
		r = p_cxt->format_storage(sz_dev, usize, flags, serial, vbr, fsbr, sz_fsbr);
		destroy_context(p_cxt);
	}
	return r;
}

bool cMGFS3_VFS::ident_superblock(_u8 *p_buffer) {
	bool r = false;
	_mgfs_info_t *p_sb = (_mgfs_info_t *)(p_buffer + MGFS_SB_OFFSET);
	if(p_sb->magic == MGFS_MAGIC && p_sb->version == MGFS_VERSION)
		r = true;
#ifdef _DEBUG_
	else {
		_u8 *p = (_u8 *)p_sb;
		_u32 sz = sizeof(_mgfs_info_t);
		for(_u32 i = 0; i < sz; i++)
			DBG("%x ",p[i]);
	}
#endif
	return r;
}

bool cMGFS3_VFS::ident_volume(iVolume *p_vol) {
	bool r = false;
	if(p_vol) {
		_u8 io_buffer[1024];
		_u16 bsize = p_vol->_vol_block_size_;
		mem_set(io_buffer, 0, sizeof(io_buffer));
		_u32 nb=0;
		p_vol->read(bsize * MGFS_SB_SECTOR, io_buffer, 
			(bsize < sizeof(io_buffer))?bsize:sizeof(io_buffer), &nb);
		if((r = ident_superblock(io_buffer))) {
			_mgfs_info_t *p_sb = (_mgfs_info_t *)(io_buffer + MGFS_SB_OFFSET);
			str_cpy((_str_t)p_vol->_vol_ident_, (_str_t)p_sb->serial, sizeof(p_vol->_vol_ident_));
		}
	}
	return r;
}
bool cMGFS3_VFS::ident_context(iFSContext *p_cxt) {
	bool r = false;
	if(str_cmp((_str_t)VFS_CONTEXT_NAME, p_cxt->DRIVER_IDENT) == 0)
		r = true;
	return r;
}
bool cMGFS3_VFS::ident_file(iFSContext *p_cxt, FSHANDLE hfile) {
	bool r = false;
	if(hfile && p_cxt) {
		_u8 io_buffer[1024];
		_fs_context_info_t cxti;
		p_cxt->info(&cxti);
		mem_set(io_buffer, 0, sizeof(io_buffer));
		_u32 nb=0;
		p_cxt->read(cxti.pbsize * MGFS_SB_SECTOR, io_buffer, 
			(cxti.pbsize < sizeof(io_buffer))?cxti.pbsize:sizeof(io_buffer), &nb, hfile);
		r = ident_superblock(io_buffer);
	}
	return r;
}

bool cMGFS3_VFS::ident(FSHANDLE hfs) {
	bool r = false;
	iFSHandle *p_fsh = (iFSHandle *)OBJECT_BY_INTERFACE(I_FS_HANDLE, RY_STANDALONE);
	if(p_fsh) {
		switch(p_fsh->type(hfs)) {
			case HTYPE_VOLUME:
				r = ident_volume((iVolume *)p_fsh->driver(hfs));
				break;
			case HTYPE_FSCONTEXT:
				r = ident_context((iFSContext *)p_fsh->driver(hfs));
				break;
			case HTYPE_FILE:
				r = ident_file((iFSContext *)p_fsh->driver(hfs), hfs);
				break;
		}
	}

	return r;
}

iFSContext *cMGFS3_VFS::create_context(void) {
	iFSContext *r = (iFSContext *)OBJECT_BY_NAME(VFS_CONTEXT_NAME, RY_CLONE);
	if(r) {
		iDriverRoot *p_droot = GET_DRIVER_ROOT();
		if(p_droot) {
			p_droot->register_driver(r, this);
			RELEASE_OBJECT(p_droot);
		}
	}

	return r;
}

void cMGFS3_VFS::destroy_context(iFSContext *p_cxt) {
	iDriverRoot *p_droot = GET_DRIVER_ROOT();
	if(p_droot) {
		p_droot->remove_driver(p_cxt);
		RELEASE_OBJECT(p_droot);
	}
}

iFSContext *cMGFS3_VFS::get_context(_u32 cidx) {
	iFSContext *r = 0;
	_driver_request_t dr;
	dr.flags = DRF_INTERFACE|DRF_INDEX|DRF_TYPE|DRF_CLASS;
	dr.DRIVER_INTERFACE = I_FS_CONTEXT;
	dr.DRIVER_INDEX = cidx;
	dr.DRIVER_TYPE = DTYPE_FS;
	dr.DRIVER_CLASS = DCLASS_FSC;

	iDriverRoot *p_droot = GET_DRIVER_ROOT();
	if(p_droot) {
		while(!(r = (iFSContext *)p_droot->driver_request(&dr, this)) && 
					dr.DRIVER_INDEX < p_droot->get_node_limit())
			dr.DRIVER_INDEX++;
	}
	return r;
}

iFSContext *cMGFS3_VFS::get_context(FSHANDLE hstorage) {
	iFSContext *r = 0;
	_u32 idx = 0;
	iFSContext *_r = 0;
	
	while((_r = get_context(idx))) {
		if(_r->verify_storage(hstorage)) {
			r = _r;
			break;
		}
		idx++;
	}

	return r;
}

iFSContext *cMGFS3_VFS::get_context(iVolume *p_vol) {
	iFSContext *r = 0;
	_u32 idx = 0;
	iFSContext *_r = 0;
	
	while((_r = get_context(idx))) {
		if(_r->verify_storage(p_vol)) {
			r = _r;
			break;
		}
		idx++;
	}
	return r;
}

