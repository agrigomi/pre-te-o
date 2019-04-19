#include "iDriver.h"
#include "str.h"
#include "iDataStorage.h"
#include "iSystemLog.h"
#include "iSyncObject.h"
#include "iMemory.h"

#define NODE_MEM	1024
#define MAX_NODE_COUNT	(NODE_MEM / sizeof(void*))

class cDriverRoot:public iDriverRoot {
public:
	cDriverRoot() {
		_d_type_ = DTYPE_VBUS;
		_d_class_ = DCLASS_SYS;
		str_cpy(_d_ident_, (_str_t)"DRIVER HOST", sizeof(_d_ident_));
		m_v_dlist = 0;
		m_p_syslog = 0;
		m_p_heap = 0;
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	
	//virtual ~cDriverRoot() {}
	DECLARE_BASE(cDriverRoot);
	DECLARE_INIT();
	DECLARE_DESTROY();

	bool driver_init(iDriverBase _UNUSED_ *p_host=0) {
		return true;
	}
	void driver_uninit(void) {}
	
	void register_driver(iDriverBase *p_drv, iDriverBase *p_host=0);
	void remove_driver(iDriverBase *p_drv, bool recursive=false);
	void sys_update(void);
	bool enum_driver(_u32 idx, iDriverBase **pp_drv, iDriverBase **pp_dhost);
	iDriverBase *driver_request(_driver_request_t *p_dr, iDriverBase *p_host_drv=0);
	void suspend_node(iDriverBase *p_drv);
	void resume_node(iDriverBase *p_drv);
	_u16 get_node_limit(void);

protected:	
	iVector		*m_v_dlist;
	iSystemLog 	*m_p_syslog;
	iBroadcast	*m_p_broadcast;
	iHeap		*m_p_heap;
	
	void setup_vector(void);
	_u16 driver_attach(iDriverBase *p_drv);
	bool driver_detach(iDriverBase *p_drv, bool recursive=false);
};

//#define _DEBUG_

#ifdef _DEBUG_ 
#define DBG(_fmt_, ...) \
	if(m_p_syslog) \
		m_p_syslog->fwrite(_fmt_, __VA_ARGS__) 
#else
#define	DBG(_fmt_, ...)
#endif // debug		

#define LOG(_fmt_, ...) \
	if(m_p_syslog) \
		m_p_syslog->fwrite(_fmt_, __VA_ARGS__) 

typedef struct {
	iDriverBase *p_driver;
	iDriverBase *p_host;
}_driver_entry_t;

IMPLEMENT_BASE(cDriverRoot, "cDriverRoot", 0x0001);

IMPLEMENT_INIT(cDriverRoot, rbase) {
	bool r = false;
	
	REPOSITORY=rbase;
	INIT_MUTEX();
	setup_vector();
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	m_p_broadcast = (iBroadcast *)OBJECT_BY_INTERFACE(I_BROADCAST, RY_STANDALONE);
	m_p_heap = (iHeap *)OBJECT_BY_INTERFACE(I_HEAP, RY_STANDALONE);
	if(m_v_dlist && _d_mutex_ && m_p_syslog && m_p_heap) {
		r = true;
	}
	
	return r;
}

IMPLEMENT_DESTROY(cDriverRoot) {
	driver_detach(this);
	RELEASE_MUTEX();
	RELEASE_OBJECT(m_v_dlist);
	RELEASE_OBJECT(m_p_syslog);
	RELEASE_OBJECT(m_p_heap);
	_d_mutex_ = 0;
}

_u16 cDriverRoot::get_node_limit(void) {
	return MAX_NODE_COUNT;
}

void cDriverRoot::setup_vector(void) {
	if(!m_v_dlist) {
		if((m_v_dlist = (iVector *)OBJECT_BY_INTERFACE(I_VECTOR, RY_CLONE)))
			m_v_dlist->init(sizeof(_driver_entry_t));
	}
}

iDriverBase *cDriverRoot::driver_request(_driver_request_t *p_dr, iDriverBase *p_host_drv) {
	iDriverBase *r = 0;
	iDriverBase *p_drv = (p_host_drv)?p_host_drv:this;

	HMUTEX hm = p_drv->lock();
	if(p_dr->flags & DRF_INTERFACE) {
		if(str_cmp((_str_t)p_dr->_d_interface_, (_str_t)(p_drv->interface_name())) != 0)
			goto _continue_;
	}
	if(p_dr->flags & DRF_PARENT_INTERFACE) {
		if(str_cmp((_str_t)p_dr->_d_parent_interface_, (_str_t)(p_drv->_interface_name())) != 0)
			goto _continue_;
	}
	if(p_dr->flags & DRF_OBJECT_NAME) {
		if(str_cmp((_str_t)p_dr->_d_name_, (_str_t)(p_drv->object_name())) != 0)
			goto _continue_;
	}
	if(p_dr->flags & DRF_IDENT) {
		if((str_cmp((_str_t)p_dr->_d_ident_, (_str_t)(p_drv->_d_ident_))) != 0)
			goto _continue_;
	}
	if(p_dr->flags & DRF_TYPE) {
		if(p_dr->_d_type_ != p_drv->_d_type_)
			goto _continue_;
	}
	if(p_dr->flags & DRF_MODE) {
		if(p_dr->_d_mode_ != p_drv->_d_mode_)
			goto _continue_;
	}
	if(p_dr->flags & DRF_CLASS) {
		if(p_dr->_d_class_ != p_drv->_d_class_)
			goto _continue_;
	}
	if(p_dr->flags & DRF_INDEX) {
		if(p_dr->_d_index_ != p_drv->_d_index_)
			goto _continue_;
	}
	p_drv->unlock(hm);
	return p_drv;
_continue_:
	if(p_drv->_d_node_) {
		for(_u32 i = 0; i < MAX_NODE_COUNT; i++) {
			if(p_drv->_d_node_[i]) {
				if((r = driver_request(p_dr, p_drv->_d_node_[i])))
					break;
			}
		}
	}
	p_drv->unlock(hm);
	return r;
}

void cDriverRoot::suspend_node(iDriverBase *p_drv) {
	if(p_drv->_d_node_) {
		for(_u32 i = 0; i < MAX_NODE_COUNT; i++) {
			if(p_drv->_d_node_[i])
				p_drv->_d_node_[i]->suspend();
		}
	}
}
void cDriverRoot::resume_node(iDriverBase *p_drv) {
	if(p_drv->_d_node_) {
		for(_u32 i = 0; i < MAX_NODE_COUNT; i++) {
			if(p_drv->_d_node_[i])
				p_drv->_d_node_[i]->resume();
		}
	}
}

_u16 cDriverRoot::driver_attach(iDriverBase *p_drv) {
	_u16 r = INVALID_DEV_INDEX;
	iDriverBase *p_host = p_drv->DRIVER_HOST;

	if(p_host) {
		HMUTEX hm = p_host->lock();
		if(!(p_host->_d_node_)) {
			if((p_host->_d_node_ = (iDriverBase **)m_p_heap->alloc(NODE_MEM))) {
				for(_u32 i = 0; i < MAX_NODE_COUNT; i++)
					p_host->_d_node_[i] = 0;
			}
		}
		if(p_host->_d_node_) {
			_u16 idx = INVALID_DEV_INDEX;

			for(_u32 i = 0; i < MAX_NODE_COUNT; i++) {
				if(p_host->_d_node_[i] == 0 && idx == INVALID_DEV_INDEX)
					idx = i;
				if(p_host->_d_node_[i] == p_drv) {
					r = i;
					break;
				}
			}
			if(r == INVALID_DEV_INDEX && 
					idx != INVALID_DEV_INDEX && 
					p_host->_d_node_[idx] == 0 &&
					p_drv != this) {
				p_host->_d_node_[idx] = p_drv;
				p_host->_d_node_[idx]->_d_state_ |= DSTATE_ATTACHED;
				p_host->_d_node_[idx]->_d_index_ = idx;
				r = idx;
			}
		}
		p_host->unlock(hm);
	}
	return r;
}

void cDriverRoot::sys_update(void) {
	if(m_v_dlist) {
		_driver_entry_t *p = (_driver_entry_t *)m_v_dlist->first();
		if(p) {
			do {
				if(!(p->p_driver->_d_state_ & (DSTATE_INITIALIZED|DSTATE_PENDING))) {
					p->p_driver->_d_state_ |= DSTATE_PENDING;
					
					if(p->p_driver->driver_init(p->p_host))
						p->p_driver->_d_state_ |= DSTATE_INITIALIZED;

					p->p_driver->_d_state_ &= ~DSTATE_PENDING;
					if(m_p_broadcast) {
						_msg_t msg={MSG_DRIVER_INIT, (_ulong)p->p_driver};
						m_p_broadcast->send_msg(&msg);
					}
				}
				
				if((p->p_driver->_d_state_ & DSTATE_INITIALIZED) && 
						!(p->p_driver->_d_state_ & DSTATE_ATTACHED)) {
					if(driver_attach(p->p_driver) != INVALID_DEV_INDEX) {
						LOG("[sys. update]: attach '%s' 0x%h\r\n", p->p_driver->_d_ident_, p->p_driver);
						if(m_p_broadcast) {
							_msg_t msg={MSG_DRIVER_READY, (_ulong)p->p_driver};
							m_p_broadcast->send_msg(&msg);
						}
						//continue;
					} else
						LOG("[sys. update]: failed to attach '%s'\r\n", p->p_driver->_d_ident_);
				}
			}while((p = (_driver_entry_t *)m_v_dlist->next()));
		}
	}
}

void cDriverRoot::register_driver(iDriverBase *p_drv, iDriverBase *p_host) {
	setup_vector();
	if(m_v_dlist) {
		if(p_drv) {
			_driver_entry_t de;
			de.p_driver = p_drv;
			de.p_host = p_host;
			DBG("add driver '%s'\r\n", p_drv->object_name());
			m_v_dlist->add(&de);
			
			p_drv->_d_state_ |= DSTATE_PENDING;
			
			if(p_drv->driver_init(p_host)) {
				p_drv->_d_state_ &= ~DSTATE_PENDING;
				p_drv->_d_state_ |= DSTATE_INITIALIZED;
				if(m_p_broadcast) {
					_msg_t msg={MSG_DRIVER_INIT, (_ulong)p_drv};
					m_p_broadcast->send_msg(&msg);
				}
				if(driver_attach(p_drv) != INVALID_DEV_INDEX) {
					LOG("[reg. driver]: '%s' 0x%h\r\n", p_drv->_d_ident_, p_drv);
					sys_update();
					if(m_p_broadcast) {
						_msg_t msg={MSG_DRIVER_READY, (_ulong)p_drv};
						m_p_broadcast->send_msg(&msg);
					}
				} else
					LOG("[error]: failed to attach '%s'\r\n", p_drv->_d_ident_);
			} else {
				p_drv->_d_state_ &= ~DSTATE_PENDING;
				LOG("[reg. driver]: '%s' (pending...) 0x%h\r\n", p_drv->_d_ident_, p_drv);
			}
		}
	}
}

bool cDriverRoot::driver_detach(iDriverBase *p_drv, bool recursive) {
	bool r = false;
	HMUTEX hlock = p_drv->lock();
	if(p_drv->_d_node_) {
		// remove up level drivers
		for(_u32 i = 0; i < MAX_NODE_COUNT; i++) {
			if(p_drv->_d_node_[i]) {
				if(recursive) {
					p_drv->unlock(hlock);
					remove_driver(p_drv->_d_node_[i], recursive);
					hlock = p_drv->lock();
				} else { // detach node
					LOG("[det. driver]: '%s' 0x%h\r\n", p_drv->_d_node_[i]->_d_ident_, p_drv->_d_node_[i]);
					HMUTEX _hlock = p_drv->_d_node_[i]->lock();
					p_drv->_d_node_[i]->_d_state_ &= ~DSTATE_ATTACHED;
					p_drv->_d_node_[i]->_d_state_ |= DSTATE_PENDING;
					p_drv->_d_node_[i]->_d_index_ = INVALID_DEV_INDEX;
					p_drv->_d_node_[i]->unlock(_hlock);
					p_drv->_d_node_[i] = 0;
				}
			}
		}
		m_p_heap->free(p_drv->_d_node_, NODE_MEM);
	}
	// detach from host
	iDriverBase *p_host = p_drv->driver_host();
	if(p_host) {
		HMUTEX hhlock = p_host->lock();
		if(p_host->_d_node_) {
			for(_u32 i = 0; i < MAX_NODE_COUNT; i++) {
				if(p_host->_d_node_[i] == p_drv) {
					p_host->_d_node_[i]->_d_state_ &= ~DSTATE_ATTACHED;
					p_host->_d_node_[i]->_d_index_ = INVALID_DEV_INDEX;
					p_host->_d_node_[i] = 0;
					r = true;
					break;
				}
			}
		}
		p_host->unlock(hhlock);
	}
	p_drv->unlock(hlock);
	return r;
}

void cDriverRoot::remove_driver(iDriverBase *p_drv, bool recursive) {
	if(m_v_dlist) {
		HMUTEX hvm = m_v_dlist->lock();

		_driver_entry_t *p = (_driver_entry_t *)m_v_dlist->first(hvm);
		do {
			if(p_drv == p->p_driver) {
				m_v_dlist->del(hvm);
				break;
			}
		}while((p = (_driver_entry_t *)m_v_dlist->next(hvm)));

		m_v_dlist->unlock(hvm);
	}
	LOG("[rem. driver]: '%s' 0x%h\r\n", p_drv->DRIVER_IDENT, p_drv);
	if(m_p_broadcast) {
		_msg_t msg={MSG_DRIVER_UNINIT, (_ulong)p_drv};
		m_p_broadcast->send_msg(&msg);
	}
	p_drv->driver_uninit();
	if(m_p_broadcast) {
		_msg_t msg={MSG_DRIVER_REMOVE, (_ulong)p_drv};
		m_p_broadcast->send_msg(&msg);
	}
	driver_detach(p_drv, recursive);
	RELEASE_OBJECT(p_drv);
}

bool cDriverRoot::enum_driver(_u32 idx, iDriverBase **pp_drv, iDriverBase **pp_dhost) {
	bool r = false;

	_driver_entry_t *p = (_driver_entry_t *)m_v_dlist->get(idx);
	if(p) {
		*pp_drv = p->p_driver;
		*pp_dhost = p->p_host;
		r = true;
	}

	return r;
}

