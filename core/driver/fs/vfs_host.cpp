#include "iVFS.h"
#include "iSystemLog.h"
#include "iDataStorage.h"
#include "iScheduler.h"

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
	iVolume	*p_vol;
}_vol_ident_t;

class cFSHost:public iFSHost {
public:
	cFSHost() {
		DRIVER_TYPE = DTYPE_VBUS;
		DRIVER_MODE = 0;
		DRIVER_CLASS = DCLASS_SYS;
		_bus_type_[0]='S';_bus_type_[1]='Y';_bus_type_[2]='S';_bus_type_[3]=0;
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	~cFSHost() {}
	
	// iBase
	DECLARE_BASE(cFSHost);
	DECLARE_INIT();
	DECLARE_DRIVER_INIT();

	iFS *filesystem(_u32 idx);
	iVolume *volume(_u32 idx);
	iVolume *volume_by_serial(_str_t serial);
	iVolume *volume_by_name(_str_t name);
	iFS *ident(iVolume *p_vol);
	iFS *ident(FSHANDLE hstorage);
	void driver_uninit(void) {}

protected:
	iBroadcast	*m_p_broadcast;	
	iSystemLog	*m_p_syslog;
	iVector		*m_p_vector;
	iFSHandle	*m_p_fshandle;

	friend void _fs_host_msg_handler(_msg_t *p_msg, void *p_data);
	void msg_handler(_msg_t *p_msg);
};

IMPLEMENT_BASE(cFSHost, "cFSHost", 0x0001);
IMPLEMENT_INIT(cFSHost, rbase) {
	bool r = false;
	REPOSITORY = rbase;
	str_cpy(DRIVER_IDENT, (_str_t)"VFS Host", sizeof(DRIVER_IDENT));
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	m_p_vector = (iVector *)OBJECT_BY_INTERFACE(I_VECTOR, RY_CLONE);
	m_p_fshandle = (iFSHandle *)OBJECT_BY_INTERFACE(I_FS_HANDLE, RY_STANDALONE);
	if(m_p_vector && m_p_fshandle) {
		m_p_vector->init(sizeof(_vol_ident_t));
		r = true;
	}
	REGISTER_DRIVER(); // self registration
	return r;
}

void _fs_host_msg_handler(_msg_t *p_msg, void *p_data) {
	cFSHost *p_obj = (cFSHost *)p_data;
	p_obj->msg_handler(p_msg);
}

IMPLEMENT_DRIVER_INIT(cFSHost, host) {
	bool r = false;
	if(!(DRIVER_HOST = host)) {
		m_p_broadcast = (iBroadcast *)OBJECT_BY_INTERFACE(I_BROADCAST, RY_STANDALONE);
		if(m_p_broadcast) {
			m_p_broadcast->add_handler(_fs_host_msg_handler, this);
			r = true;
		}
		DRIVER_HOST = GET_DRIVER_ROOT();
	}
	return r;
}

extern _u32 _max_driver_nodes_;

iFS *cFSHost::filesystem(_u32 idx) {
	iFS * r = 0;
	_driver_request_t dr;
	dr.flags = DRF_INDEX|DRF_INTERFACE|DRF_TYPE|DRF_CLASS;
	dr.DRIVER_INTERFACE = I_FS;
	dr.DRIVER_TYPE = DTYPE_FS;
	dr.DRIVER_CLASS = DCLASS_VFS;
	dr.DRIVER_INDEX = idx;

	iDriverRoot *p_droot = GET_DRIVER_ROOT();
	if(p_droot) {
		while(!(r = (iFS *)p_droot->driver_request(&dr, this)) && dr.DRIVER_INDEX < p_droot->get_node_limit())
			dr.DRIVER_INDEX++;
	}
	
	return r;
}

iVolume *cFSHost::volume(_u32 idx) {
	iVolume *r = 0;
	HMUTEX hlock = acquire_mutex(m_p_vector->get_mutex());
	_vol_ident_t *p_vidt = (_vol_ident_t *)m_p_vector->get(idx, hlock);
	if(p_vidt) 
		r = p_vidt->p_vol;
	m_p_vector->unlock(hlock);
	return r;
}

iVolume *cFSHost::volume_by_serial(_str_t serial) {
	iVolume *r = 0;

	_u32 vidx = 0;
	iVolume *pv = 0;

	while((pv = volume(vidx))) {
		if(!str_len(pv->serial()))
			ident(pv);
		
		if(str_cmp(serial, pv->serial()) == 0) {
			r = pv;
			break;
		}
		vidx++;
	}

	return r;
}

iVolume *cFSHost::volume_by_name(_str_t name) {
	iVolume *r = 0;

	_u32 vidx = 0;
	iVolume *pv = 0;

	while((pv = volume(vidx))) {
		if(str_cmp(name, pv->name()) == 0) {
			r = pv;
			break;
		}
		vidx++;
	}
	
	return r;
}

iFS *cFSHost::ident(iVolume *p_vol) {
	iFS *r = 0;
	iFS *p_fs = 0;
	_u32 fs_idx = 0;
	while((p_fs = filesystem(fs_idx))) {
		if(p_fs->ident_volume(p_vol)) {
			r = p_fs;
			break;
		}
		fs_idx++;
	}
	return r;
}

iFS *cFSHost::ident(FSHANDLE hstorage) {
	iFS *r = 0;
	switch(m_p_fshandle->type(hstorage)) {
		case HTYPE_VOLUME:
		case HTYPE_FSCONTEXT:
		case HTYPE_FILE: {
				iFS *p_fs = 0;
				_u32 fs_idx = 0;
				while((p_fs = filesystem(fs_idx))) {
					if(p_fs->ident(hstorage)) {
						r = p_fs;
						break;
					}
					fs_idx++;
				}
			}
			break;
	}

	return r;
}

void cFSHost::msg_handler(_msg_t *p_msg) {
	iDriverBase *p_drv = 0;
	switch(p_msg->msg) {
		case MSG_DRIVER_READY:
			p_drv = (iDriverBase *)p_msg->data;
			if(p_drv->DRIVER_TYPE == DTYPE_VOL) {
				DBG("FSHost: handle message (DRIVER_READY, '%s')\r\n", p_drv->DRIVER_IDENT);
				if(m_p_vector) {
					_vol_ident_t vidt;
					vidt.p_vol = (iVolume *)p_drv;
					m_p_vector->add(&vidt);
				}
				break;
			}
			break;
		case MSG_DRIVER_UNINIT:
			p_drv = (iDriverBase *)p_msg->data;
			if(p_drv->DRIVER_TYPE == DTYPE_VOL) {
				p_drv = (iDriverBase *)p_msg->data;
				DBG("FSHost: handle message (DRIVER_UNINIT, '%s')\r\n", p_drv->DRIVER_IDENT);
				if(m_p_vector) {
					HMUTEX hlock = acquire_mutex(m_p_vector->get_mutex());
					_u32 cnt = m_p_vector->cnt(hlock);
					for(_u32 i = 0; i < cnt; i++) {
						_vol_ident_t *p_vidt = (_vol_ident_t *)m_p_vector->get(i, hlock);
						if(p_vidt) {
							if(p_vidt->p_vol == (iVolume *)p_drv) {
								m_p_vector->del(i, hlock);
								break;
							}
						} else
							break;
					}
					m_p_vector->unlock(hlock);
				}
				
				iFS *p_fs = ident((iVolume *)p_drv);
				if(p_fs) {
					iFSContext *p_cxt = 0;
					while((p_cxt = p_fs->get_context((iVolume *)p_drv))) {
						p_cxt->remove_storage((iVolume *)p_drv);
						_fs_context_info_t cxti;
						p_cxt->info(&cxti);
						if(!cxti.nitems)
							p_fs->destroy_context(p_cxt);
					}
				}
			}
			break;
	}
}

