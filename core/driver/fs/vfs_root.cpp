#include "iVFS.h"
#include "iDataStorage.h"
#include "iScheduler.h"
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

#define MAX_ITRC	8
#define MAX_MP_NAME	64

typedef struct {
	_s8		mp_path[FS_MAX_PATH]; // mount point path
	_s8		mp_name[MAX_MP_NAME];
	FSHANDLE	hmp_dir; // mount point directory handle
	FSHANDLE	htp_dir; // target point directory handle
}_mount_point_t;

class cFSRoot:public iFSRoot {
public:
	cFSRoot() {
		m_pv_mounts = 0;
		m_p_syslog = 0;
		m_p_fs_host = 0;
		m_p_broadcast = 0;
		m_p_fshandle = 0;
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	~cFSRoot() {}

	DECLARE_BASE(cFSRoot);
	DECLARE_INIT();

	_fs_res_t mount(FSHANDLE hstorage, _str_t mp_name, _str_t mount_point);
	_fs_res_t mount(iVolume *p_vol, _str_t mount_point);
	_fs_res_t mount(_str_t volume_name, _str_t mount_point);
	_fs_res_t unmount(_str_t mount_point);
	_fs_res_t fopen(_u32 user_id, _str_t path, // [in]
			_str_t fname, // [in]
			_u32 flags, // [in] flags + permissions mask
			FSHANDLE *hfile, // [out] valid handle when function return FS_RESULT_OK
			FSHANDLE hcd=0 // handle of current directory
		      );
	_fs_res_t fclose(FSHANDLE hfile);
	_fs_res_t fsize(FSHANDLE hfile, _u64 *nb);
	_fs_res_t fread(FSHANDLE hfile, // [in]
		   	void *buffer, // [in] // input buffer
		   	_u32 sz,       // [in] // bytes to read
		   	_u32 *nb // [out] number of transfered bytes
		  	);
	_fs_res_t fwrite(FSHANDLE hfile, // [in]
			void *data, // [in] output buffer
			_u32 sz,	  // bytes to write
			_u32 *nb // [out] number of transfered bytes
			);
	_fs_res_t fseek(FSHANDLE hfile, // [in]
			_u64 offset // [in] offset from begin
			 );
	_fs_res_t finfo(FSHANDLE hfile, _fs_file_info_t *p_fi);

protected:
	iVector		*m_pv_mounts;
	iSystemLog 	*m_p_syslog;
	iFSHost		*m_p_fs_host;
	iBroadcast	*m_p_broadcast;	
	iFSHandle	*m_p_fshandle;

	HMUTEX lock(HMUTEX hlock=0);
	void unlock(HMUTEX hlock);
	_u32 trace_path(_str_t path, // [in]
			FSHANDLE hstart, // [in]
			FSHANDLE *hres, // [out]
			_u8 itrc=0 // iteration counter
		       );
	FSHANDLE get_mount_point(FSHANDLE hdir);
	_u32 get_pmask(FSHANDLE h);
	iFSContext *get_fs_context(FSHANDLE h);
	_str_t get_link_name(FSHANDLE h);
	_fs_res_t open_file(FSHANDLE hdir, _str_t fn, _u16 sz_fn, FSHANDLE *hfile, _u8 itrc, bool close_dir=true);
	bool compare_handle(FSHANDLE h1, FSHANDLE h2);
	_mount_point_t *get_mount_record(FSHANDLE h, _u32 *p_idx=0);
	_mount_point_t *get_mount_record(iVolume *p_vol, _u32 *p_idx=0);
	_mount_point_t *get_mount_record(iFSContext *p_cxt, _u32 *p_idx=0);
	void unmount(_mount_point_t *pmp, _u32 idx);
	void msg_handler(_msg_t *p_msg);
	friend void _fs_root_msg_handler(_msg_t *p_msg, void *p_data);
};

void _fs_root_msg_handler(_msg_t *p_msg, void *p_data) {
	cFSRoot *p_obj = (cFSRoot *)p_data;
	p_obj->msg_handler(p_msg);
}

IMPLEMENT_BASE(cFSRoot, "cFSRoot", 0x0001);
IMPLEMENT_INIT(cFSRoot, repo) {
	REPOSITORY = repo;
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	if((m_p_broadcast = (iBroadcast *)OBJECT_BY_INTERFACE(I_BROADCAST, RY_STANDALONE)))
		m_p_broadcast->add_handler(_fs_root_msg_handler, this);
	if((m_pv_mounts = (iVector *)OBJECT_BY_INTERFACE(I_VECTOR, RY_CLONE)))
		m_pv_mounts->init(sizeof(_mount_point_t));
	m_p_fs_host = (iFSHost *)OBJECT_BY_INTERFACE(I_FS_HOST, RY_STANDALONE);
	m_p_fshandle = (iFSHandle *)OBJECT_BY_INTERFACE(I_FS_HANDLE, RY_STANDALONE);
	return true;
}

HMUTEX cFSRoot::lock(HMUTEX hlock) {
	HMUTEX r = 0;
	if(m_pv_mounts) {
		iMutex *p_mutex = m_pv_mounts->get_mutex();
		r = acquire_mutex(p_mutex, hlock);
	}
	return r;
}
void cFSRoot::unlock(HMUTEX hlock) {
	if(m_pv_mounts)
		m_pv_mounts->unlock(hlock);
}

bool cFSRoot::compare_handle(FSHANDLE h1, FSHANDLE h2) {
	return m_p_fshandle->compare(h1, h2);
}

void cFSRoot::msg_handler(_msg_t *p_msg) {
	switch(p_msg->msg) {
		case MSG_DRIVER_UNINIT: {
			_mount_point_t *pmp = 0;
			iDriverBase *p_drv = (iDriverBase *)p_msg->data;
			_u32 idx=0;
			if(p_drv->DRIVER_TYPE == DTYPE_VOL) {
				// force unmount by volume
				while((pmp = get_mount_record((iVolume *)p_drv, &idx))) {
					LOG("FS_ROOT: force unmount '%s'\r\n", pmp->mp_name);
					unmount(pmp, idx);
				}
			} else {
				if(p_drv->DRIVER_TYPE == DTYPE_FS && p_drv->DRIVER_CLASS == DCLASS_FSC) {
					// force unmount by filesystem context
					while((pmp = get_mount_record((iFSContext *)p_drv, &idx))) {
						LOG("FS_ROOT: force unmount '%s'\r\n", pmp->mp_name);
						unmount(pmp, idx);
					}
				}
			}
		}
		break;
	}
}

_mount_point_t *cFSRoot::get_mount_record(FSHANDLE h, _u32 *p_idx) {
	_mount_point_t *r = 0;
	if(m_p_fshandle->type(h) == HTYPE_FILE && m_p_fshandle->driver(h)) {
		HMUTEX hlock = lock();
		_u32 idx = 0;
		_mount_point_t *pmp = (_mount_point_t *)m_pv_mounts->first(hlock);
		if(pmp) {
			do {
				if(compare_handle(h, pmp->hmp_dir) || 
						compare_handle(h, pmp->htp_dir)) {
					r = pmp;
					break;
				}
				idx++;
			} while((pmp = (_mount_point_t *)m_pv_mounts->next(hlock)));
		}

		if(p_idx)
			*p_idx = idx;

		unlock(hlock);
	}
	return r;
}

_mount_point_t *cFSRoot::get_mount_record(iVolume *p_vol, _u32 *p_idx) {
	_mount_point_t *r = 0;
	HMUTEX hlock = lock();
	_u32 idx = 0;

	_mount_point_t *pmp = (_mount_point_t *)m_pv_mounts->first(hlock);
	if(pmp) {
		do {
			iFSContext *p_fscxt_mp = (iFSContext *)m_p_fshandle->driver(pmp->hmp_dir);
			iFSContext *p_fscxt_tp = (iFSContext *)m_p_fshandle->driver(pmp->htp_dir);
			if(p_fscxt_mp && p_fscxt_tp) {
				if(p_fscxt_mp->DRIVER_CLASS == DCLASS_FSC && p_fscxt_tp->DRIVER_CLASS == DCLASS_FSC) {
					if(p_fscxt_mp->verify_storage(p_vol) || 
							p_fscxt_tp->verify_storage(p_vol)) {
						r = pmp;
						break;
					}
				}
			}

			idx++;
		} while((pmp = (_mount_point_t *)m_pv_mounts->next(hlock)));
	}

	if(p_idx)
		*p_idx = idx;

	unlock(hlock);
	return r;
}

_mount_point_t *cFSRoot::get_mount_record(iFSContext *p_cxt, _u32 *p_idx) {
	_mount_point_t *r = 0;
	HMUTEX hlock = lock();
	_u32 idx = 0;
	_mount_point_t *pmp = (_mount_point_t *)m_pv_mounts->first(hlock);
	if(pmp) {
		do {
			iFSContext *p_fscxt_mp = (iFSContext *)m_p_fshandle->driver(pmp->hmp_dir);
			iFSContext *p_fscxt_tp = (iFSContext *)m_p_fshandle->driver(pmp->htp_dir);
			if(p_fscxt_mp == p_cxt || p_fscxt_tp == p_cxt) {
				r = pmp;
				break;
			}
			
			idx++;
		} while((pmp = (_mount_point_t *)m_pv_mounts->next(hlock)));
	}
	
	if(p_idx)
		*p_idx = idx;

	unlock(hlock);
	return r;
}

FSHANDLE cFSRoot::get_mount_point(FSHANDLE hdir) {
	FSHANDLE r = hdir;
	HMUTEX hlock = lock();
	_mount_point_t *pmp = (_mount_point_t *)m_pv_mounts->first(hlock);
	if(pmp) {
		do {
			if(r) {
				if(compare_handle(pmp->hmp_dir, hdir)) {
					r = pmp->htp_dir;
					break;
				}
			} else { // return root mount point
				//r = m_p_fshandle->copy(pmp->htp_dir);
				r = pmp->htp_dir;
				break;
			}
		} while((pmp = (_mount_point_t *)m_pv_mounts->next(hlock)));
	}
	unlock(hlock);
	
	return r;
}

_u32 cFSRoot::get_pmask(FSHANDLE h) {
	_u32 r = 0;
	iDriverBase *p_drv = m_p_fshandle->driver(h);
	if(p_drv) {
		if(p_drv->DRIVER_CLASS == DCLASS_FSC) {
			iFSContext *p_fscxt = (iFSContext *)p_drv;
			p_fscxt->fpmask(h, &r);
		}
	}
	return r;
}

iFSContext *cFSRoot::get_fs_context(FSHANDLE h) {
	iFSContext *r = 0;
	iDriverBase *p_drv = m_p_fshandle->driver(h);
	if(p_drv->DRIVER_CLASS == DCLASS_FSC)
		r = (iFSContext *)p_drv;
	return r;
}

_str_t cFSRoot::get_link_name(FSHANDLE h) {
	_str_t r = 0;
	if(h) {
		if(m_p_fshandle->type(h) == HTYPE_FILE && (get_pmask(h) & FSP_SLINK)) {
			_fs_file_info_t fi;
			iFSContext *p_fscxt = (iFSContext *)m_p_fshandle->driver(h);
			if(p_fscxt) {
				p_fscxt->finfo(h, &fi);
				if(fi.lname) {
					if(str_len(fi.lname))
						r = fi.lname;
				}
			}
		}
	}
	return r;
}

_fs_res_t cFSRoot::open_file(FSHANDLE hdir, _str_t fn, _u16 sz_fn, FSHANDLE *hfile, _u8 itrc, bool _close_dir) {
	_fs_res_t r = FS_RESULT_ERR;
	_s8 fname[FS_MAX_PATH]="";
	FSHANDLE _hdir = hdir;
	
	if(_hdir) {
		if(sz_fn) {
			mem_set((_u8 *)fname, 0, sizeof(fname));
			mem_cpy((_u8 *)fname, (_u8 *)fn, (sz_fn < FS_MAX_PATH)?sz_fn:FS_MAX_PATH-1);
			clrspc(fname);

			if(m_p_fshandle->type(_hdir) == HTYPE_FILE && 
					(get_pmask(_hdir) & FSP_DIR)) {
				FSHANDLE h;
				iFSContext *p_fscxt = (iFSContext *)m_p_fshandle->driver(_hdir);
				if(p_fscxt) {
					if((r = p_fscxt->fopen(fname, _hdir, &h)) == FS_RESULT_OK) {
						// check for link
						if(get_pmask(h) & FSP_SLINK) {
							_str_t lname = get_link_name(&h);
							fclose(&h);

							if(lname)
								r = trace_path(lname, _hdir, &h, itrc);
							else
								r =  FS_RESULT_INVALID_PATH;
						}
						if(r == FS_RESULT_OK) 
							*hfile = get_mount_point(h);
					}
					if(_close_dir) {
						if(!compare_handle(_hdir, hfile))
							fclose(_hdir);
					}
				}
			}
		} else {
			*hfile = get_mount_point(_hdir);
			r = FS_RESULT_OK;
		}
	}
	DBG("FS_ROOT: open file name='%s'; hdir=0x%h; hfile=0x%h; result=%u\r\n", fname, hdir, *hfile, r);
	return r;
}

_fs_res_t cFSRoot::trace_path(_str_t path, // [in]
		 	 FSHANDLE hstart, // [in]
			 FSHANDLE *hres, // [out]
			 _u8 itrc // iteration counter
		        ) {
	_fs_res_t r = FS_RESULT_ERR;

	if(itrc < MAX_ITRC) {
		FSHANDLE _hstart = 0;
		if(path[0] == '/')
			_hstart = get_mount_point(0);
		else
			_hstart = get_mount_point(hstart);

		if(_hstart) {
			FSHANDLE hcd = _hstart;
			
			_u16 len = str_len(path);
			_u16 i = 0, j = 0;
			for(; i < len; i++) {
				switch(path[i]) {
					case '/':
						if((r = open_file(hcd, 
								path + j, 
								i - j, 
								&hcd, 
								itrc+1)) == FS_RESULT_OK) {
							if(!(get_pmask(hcd) & FSP_DIR)) {
								i = len;
								//??? check for end of path
							}
							j = i + 1;
						} else {
							m_p_syslog->write("FSRoot: failed to open '\r\n");
							m_p_syslog->write(path, j);
							m_p_syslog->write("'\r\n");
							return r;
						}
						break;

					case ':': 
						// ??? trace absolute path
						break;
				}
			}
			r = open_file(hcd, path + j, i - j, hres, itrc+1);
		}
	} else
		r = FS_RESULT_ITRC_ERR;

	return r;
}

_fs_res_t cFSRoot::mount(FSHANDLE hstorage, _str_t mp_name, _str_t mount_point) {
	_fs_res_t r = FS_RESULT_ERR;
	_mount_point_t mp;
	FSHANDLE hmp;
	FSHANDLE hstorage_root = 0;

	if(hstorage) {
		// check for directory handle in hstorage 
		if(m_p_fshandle->type(hstorage) == HTYPE_FILE && (get_pmask(hstorage) & FSP_DIR))
			hstorage_root = hstorage;
		else {
			// ident filesystem if hstorage is volume or file handle
			iFS *p_fs = m_p_fs_host->ident(hstorage);
			if(p_fs) {
				iFSContext *p_cxt = p_fs->get_context(hstorage);
				if(!p_cxt) {
					if((p_cxt = p_fs->create_context()))
						p_cxt->add_storage(hstorage);
				}

				if(p_cxt)
					hstorage_root = p_cxt->root();
			} else {
				LOG("FS_ROOT: faied to mount unknown filesystem in '%s'\r\n", mount_point);
				r = FS_RESULT_UNKNOWN;
			}
		}

		if(hstorage_root) {
			_u32 mount_cnt = m_pv_mounts->cnt();
			if(!mount_cnt) {
				// first mount point (ROOT)
				mp.hmp_dir = hstorage_root;
				str_cpy(mp.mp_path, mount_point, FS_MAX_PATH);
				str_cpy(mp.mp_name, mp_name, MAX_MP_NAME);
				mp.htp_dir = hstorage_root;
				m_pv_mounts->add(&mp);
				LOG("FS_ROOT: mount '%s%s' as ROOT in '%s' handle=0x%h\r\n", mp_name, 
							(m_p_fshandle->type(hstorage) == HTYPE_VOLUME)?":/":"/",
							mount_point, mp.htp_dir);
				r = FS_RESULT_OK;
			} else {
				if((r = trace_path(mount_point, 0, &hmp)) == FS_RESULT_OK) {
					mp.hmp_dir = hmp;
					str_cpy(mp.mp_path, mount_point, FS_MAX_PATH);
					str_cpy(mp.mp_name, mp_name, MAX_MP_NAME);
					mp.htp_dir = hstorage_root;
					m_pv_mounts->add(&mp);
					LOG("FS_ROOT: mount '%s%s' in '%s' handle=0x%h\r\n", mp_name, 
						(m_p_fshandle->type(hstorage) == HTYPE_VOLUME)?":/":"/",
						mount_point, mp.htp_dir);
				}
			}
		}
	}
	return r;
}
_fs_res_t cFSRoot::mount(iVolume *p_vol, _str_t mount_point) {
	_fs_res_t r = FS_RESULT_ERR;
	FSHANDLE hv = m_p_fshandle->create(p_vol, 0, HTYPE_VOLUME);
	if(hv) {
		m_p_fshandle->update_state(hv, m_p_fshandle->state(hv)|HSTATE_OPEN);
		if((r = mount(hv, p_vol->DRIVER_IDENT, mount_point)) != FS_RESULT_OK)
			m_p_fshandle->remove(hv);
	}
	return r;
}
_fs_res_t cFSRoot::mount(_str_t volume_name, _str_t mount_point) {
	_u32 r = FS_RESULT_ERR;
	iVolume *p_vol = m_p_fs_host->volume_by_name(volume_name);
	if(p_vol)
		r = mount(p_vol, mount_point);
	return r;
}

void cFSRoot::unmount(_mount_point_t *pmp, _u32 idx) {
	HMUTEX hlock = lock();
	_mount_point_t tmp;
	mem_cpy((_u8 *)&tmp, (_u8 *)pmp, sizeof(_mount_point_t));
	m_pv_mounts->del(idx, hlock);
	unlock(hlock);

	fclose(tmp.htp_dir);
	fclose(tmp.hmp_dir);
	LOG("FS_ROOT: unmount '%s'\r\n", tmp.mp_name);
}

_fs_res_t cFSRoot::unmount(_str_t mount_point) {
	_u32 r = FS_RESULT_ERR;
	FSHANDLE hmp;

	if((r = trace_path(mount_point, 0, &hmp)) == FS_RESULT_OK) {
		_u32 idx = 0xffffffff;
		_mount_point_t *pmp = get_mount_record(hmp, &idx);
		if(pmp && idx != 0xffffffff)
			unmount(pmp, idx);
		else
			r = FS_RESULT_INVALID_PATH;

		fclose(&hmp);
	}
	return r;
}

_fs_res_t cFSRoot::fopen(_u32 user_id, _str_t path, // [in]
		_str_t fname, // [in]
		_u32 flags, // [in] flags + permissions mask
		FSHANDLE *hfile, // [out] valid handle when function return FS_RESULT_OK
		FSHANDLE hcd // handle of current directory
	      ) {
	_fs_res_t r = FS_RESULT_ERR;
	FSHANDLE hdir;
	_u8 lfname = (_u8)((fname)?str_len(fname):0);

	if((r = trace_path((path)?path:(_str_t)"", hcd, &hdir)) == FS_RESULT_OK) {
		if(lfname || (flags & FSP_CREATE_NEW)) {
			if(get_pmask(hdir)) {
				if(lfname) {
					iFSContext *p_cxt = get_fs_context(hdir);
					if(p_cxt) {
						if(flags & FSP_CREATE_NEW) {
							r = p_cxt->fcreate(user_id, fname, flags, hdir, hfile);
							DBG("FS_ROOT: create file '%s/%s' handle=0x%h\r\n", path, fname, *hfile);
						} else 
							r = open_file(hdir, fname, lfname, hfile, 0);
					}
					fclose(hdir);
				}
			} else
				r = FS_RESULT_INVALID_PATH;
		} else
			*hfile = get_mount_point(hdir);
	}
	return r;
}

_fs_res_t cFSRoot::fclose(FSHANDLE hfile) {
	_fs_res_t r = FS_RESULT_ERR;
	if(m_p_fshandle->type(hfile) == HTYPE_FILE) {
		_mount_point_t *pmp = get_mount_record(hfile);
		if(!pmp) {
			iFSContext *p_cxt = get_fs_context(hfile);
			if(p_cxt) {
				DBG("FS_ROOT: close handle 0x%h\r\n", hfile);
				r = p_cxt->fclose(hfile);
			}
		} else {
			DBG("FS_ROOT: skyp close handle 0x%h because it's mountpoint '%s'\r\n", hfile, pmp->mp_path);
		}
	}
	return r;
}

_fs_res_t cFSRoot::fsize(FSHANDLE hfile, _u64 *nb) {
	_fs_res_t r = 0;
	if(m_p_fshandle->type(hfile) == HTYPE_FILE) {
		iFSContext *p_cxt = get_fs_context(hfile);
		if(p_cxt)
			r = p_cxt->fsize(hfile, nb);
	}
	return r;
}
_fs_res_t cFSRoot::fread(FSHANDLE hfile, // [in]
		   	void *buffer, // [in] // input buffer
		   	_u32 sz, // [in] // max. size of input buffer
		   	_u32 *nb // [in/out] number of bytes to read/readed
		  	) {
	_fs_res_t r = FS_RESULT_ERR;
	if(m_p_fshandle->type(hfile) == HTYPE_FILE) {
		iFSContext *p_cxt = get_fs_context(hfile);
		if(p_cxt)
			r = p_cxt->fread((_u8 *)buffer, sz, nb, hfile);
	}
	return r;
}
_fs_res_t cFSRoot::fwrite(FSHANDLE hfile, // [in]
		    	  void *data, // [in] output buffer
			  _u32 sz,	  // bytes to write
		    	  _u32 *nb // [out] number of transfered bytes
		   	) {
	_fs_res_t r = FS_RESULT_ERR;
	if(m_p_fshandle->type(hfile) == HTYPE_FILE) {
		iFSContext *p_cxt = get_fs_context(hfile);
		if(p_cxt)
			r = p_cxt->fwrite((_u8 *)data, sz, nb, hfile);
	}
	return r;
}
_fs_res_t cFSRoot::fseek(FSHANDLE hfile, // [in]
			_u64 offset // [in] offset from begin
			) {
	_fs_res_t r = FS_RESULT_ERR;
	if(m_p_fshandle->type(hfile) == HTYPE_FILE) {
		iFSContext *p_cxt = get_fs_context(hfile);
		if(p_cxt)
			r = p_cxt->fseek(offset, hfile);
	}
	return r;
}
_fs_res_t cFSRoot::finfo(FSHANDLE hfile, _fs_file_info_t *p_fi) {
	_fs_res_t r = FS_RESULT_ERR;
	iFSContext *p_cxt = get_fs_context(hfile);
	if(p_cxt)
		r = p_cxt->finfo(hfile, p_fi);
	return r;
}

