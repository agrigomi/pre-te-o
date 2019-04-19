#include "iVFS.h"
#include "iDataStorage.h"

#define HANDLE_MAGIC	0xcda5
#define COL_ACTIVE	0
#define COL_INACTIVE	1

typedef struct {
	_u8		type;
	_u8		state;
	iDriverBase 	*driver;
	_u32		user;
	_u16		magic;
	_u8		data[FS_MAX_HANDLE_DATA];
}_fs_handle_info_t;

static void _fs_handle_msg_handler(_msg_t *p_msg, void *p_data);

class cFSHandle:public iFSHandle {
protected:
	iVector	*m_p_vhandle;
	iBroadcast *m_p_broadcast;

	friend void _fs_handle_msg_handler(_msg_t *p_msg, void *p_data);

	HMUTEX lock(HMUTEX hlock=0) {
		return m_p_vhandle->lock(hlock);
	}
	void unlock(HMUTEX hlock) {
		m_p_vhandle->unlock(hlock);
	}
	
	void msg_handler(_msg_t *p_msg) {
		switch(p_msg->msg) {
			case MSG_DRIVER_UNINIT: {
				HMUTEX hm = lock();
				m_p_vhandle->col(COL_ACTIVE, hm);
				_fs_handle_info_t *p_hi = (_fs_handle_info_t *)m_p_vhandle->first(hm);
				if(p_hi) {
					iDriverBase *p_drv = (iDriverBase *)p_msg->data;
					do {
_check_current_:						
						if(p_drv == p_hi->driver) {
							// destroy handle
							p_hi->driver = 0;
							p_hi->state = 0;
							p_hi->type = 0;
							m_p_vhandle->mov(p_hi, COL_INACTIVE, hm);
							if((p_hi = (_fs_handle_info_t *)m_p_vhandle->current(hm)))
								goto _check_current_;
						}
					} while((p_hi = (_fs_handle_info_t *)m_p_vhandle->next(hm)));
				}
				unlock(hm);
			}
			break;
		}
	}

	_fs_handle_info_t *validate(FSHANDLE h) {
		_fs_handle_info_t *p_fsh = (_fs_handle_info_t *)h;
		if(p_fsh) {
			if(p_fsh->magic == HANDLE_MAGIC)
				return p_fsh;
		}
		return 0;
	}

public:
	cFSHandle() {
		m_p_vhandle = 0;
		m_p_broadcast = 0;
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}

	~cFSHandle() {}
	DECLARE_BASE(cFSHandle);

	bool object_init(iRepositoryBase *p_rb) {
		bool r = false;
		REPOSITORY = p_rb;
		m_p_vhandle = (iVector *)OBJECT_BY_INTERFACE(I_VECTOR, RY_CLONE);
		m_p_broadcast = (iBroadcast *)OBJECT_BY_INTERFACE(I_BROADCAST, RY_STANDALONE);
		if(m_p_vhandle && m_p_broadcast) {
			m_p_vhandle->init(sizeof(_fs_handle_info_t), 2);
			m_p_broadcast->add_handler(_fs_handle_msg_handler, this);
			r = true;
		}
		return r;
	}

	FSHANDLE create(iDriverBase *p_drv, _u32 user, _u8 type, _u8 *data=0) {
		FSHANDLE r = 0;
		if(p_drv) {
			switch(type) {
				case HTYPE_DEVICE:
					if(p_drv->DRIVER_TYPE != DTYPE_DEV)
						return r;
					break;
				case HTYPE_VOLUME:
					if(p_drv->DRIVER_TYPE != DTYPE_VOL)
						return r;
					break;
				case HTYPE_FILE:
				case HTYPE_FSCONTEXT:
					if(p_drv->DRIVER_CLASS != DCLASS_FSC)
						return r;
					break;
			}

			HMUTEX hlock = lock();
			m_p_vhandle->col(COL_INACTIVE, hlock); // select list of inactive handles
			if((r = (_fs_handle_info_t *)m_p_vhandle->first(hlock))) {
				_fs_handle_info_t *p_hi = (_fs_handle_info_t *)r;
				p_hi->driver = p_drv;
				p_hi->user = user;
				p_hi->type = type;
				p_hi->state = 0;
				if(data)
					mem_cpy(p_hi->data, data, FS_MAX_HANDLE_DATA);
				if(!m_p_vhandle->mov(r, COL_ACTIVE, hlock))
					r = 0;
				m_p_vhandle->col(COL_ACTIVE, hlock);
			} else {// create new entry
				_fs_handle_info_t hi;
				mem_set((_u8 *)&hi, 0, sizeof(_fs_handle_info_t));
				hi.driver = p_drv;
				hi.user = user;
				hi.type = type;
				hi.magic = HANDLE_MAGIC;
				if(data)
					mem_cpy(hi.data, data, FS_MAX_HANDLE_DATA);
				m_p_vhandle->col(COL_ACTIVE, hlock);
				r = m_p_vhandle->add(&hi, hlock);
			}
			unlock(hlock);
		}
		return r;
	}
	bool update_user(FSHANDLE h, _u32 user) {
		bool r = false;
		_fs_handle_info_t *p_hi = validate(h);
		if(p_hi) {
			p_hi->user = user;
			r = true;
		}
		return r;
	}
	bool update_state(FSHANDLE h, _u8 state) {
		bool r = false;
		_fs_handle_info_t *p_hi = validate(h);
		if(p_hi) {
			HMUTEX hlock = lock();
			p_hi->state = state;
			r = true;
			unlock(hlock);
		}
		return r;
	}
	bool update_data(FSHANDLE h, _u8 *data) {
		bool r = false;
		_fs_handle_info_t *p_hi = validate(h);
		if(p_hi) {
			HMUTEX hlock = lock();
			mem_cpy(p_hi->data, data, FS_MAX_HANDLE_DATA);
			r = true;
			unlock(hlock);
		}
		return r;
	}
	bool compare(FSHANDLE h1, FSHANDLE h2) {
		bool r = false;
		if(h1 != h2) {
			_fs_handle_info_t *ph1i = validate(h1);
			_fs_handle_info_t *ph2i = validate(h2);
			if(ph1i && ph2i) {
				if(ph1i->type == ph2i->type) {
					switch(ph1i->type) {
						case HTYPE_DEVICE:
						case HTYPE_VOLUME:
						case HTYPE_FSCONTEXT:
							if(ph1i->driver == ph2i->driver)
								r = true;
							break;
						case HTYPE_FILE:
							if(ph1i->driver == ph2i->driver) {
								if(mem_cmp(ph1i->data, ph2i->data, FS_MAX_HANDLE_DATA) == 0)
									r = true;
							}
							break;
					}
				}
			}
		} else
			r = true;
		return r;
	}
	void remove(FSHANDLE h) {
		_fs_handle_info_t *p_hi = validate(h);
		if(p_hi) {
			HMUTEX hlock = lock();
			p_hi->type = 0;
			p_hi->state = 0;
			p_hi->driver = 0;
			m_p_vhandle->mov(p_hi, COL_INACTIVE, hlock);
			unlock(hlock);
		}
	}
	_u8 *data(FSHANDLE h) {
		_u8 *r = 0;
		_fs_handle_info_t *p_hi = validate(h);
		if(p_hi)
			r = p_hi->data;
		return r;
	}
	_u8 state(FSHANDLE h) {
		_u8 r = 0;
		_fs_handle_info_t *p_hi = validate(h);
		if(p_hi) {
			HMUTEX hlock = lock();
			r = p_hi->state;
			unlock(hlock);
		}
		return r;
	}
	_u32 user(FSHANDLE h) {
		_u32 r = 0;
		_fs_handle_info_t *p_hi = validate(h);
		if(p_hi)
			r = p_hi->user;
		return r;
	}
	_u8 type(FSHANDLE h) {
		_u8 r = 0;
		_fs_handle_info_t *p_hi = validate(h);
		if(p_hi)
			r = p_hi->type;
		return r;
	}
	iDriverBase *driver(FSHANDLE h) {
		iDriverBase *r = 0;
		_fs_handle_info_t *p_hi = validate(h);
		if(p_hi) {
			HMUTEX hlock = lock();
			r = p_hi->driver;
			unlock(hlock);
		}
		return r;
	}
};
IMPLEMENT_BASE(cFSHandle, "cFSHandle", 0x001);

static void _fs_handle_msg_handler(_msg_t *p_msg, void *p_data) {
	cFSHandle *p_obj = (cFSHandle *)p_data;
	p_obj->msg_handler(p_msg);
}

