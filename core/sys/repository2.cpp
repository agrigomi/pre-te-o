#include "repository.h"
#include "iMemory.h"
#include "iSyncObject.h"
#include "str.h"
#include "iSystemLog.h"
#include "iScheduler.h"

#define LOG(fmt, ...) { \
	iSystemLog *p_log = get_sys_log(); \
	if(p_log) \
		p_log->fwrite(fmt, __VA_ARGS__); \
}

#define RY_READY	(1<<7)
#define RY_DELETED	(1<<6)

typedef struct {
	_object_t 	*so_storage;
	_u16		count;
	_u16		limit;		
}__attribute__((packed)) _so_storage_t;

class cRepository:public iRepository {
protected:
	iMutex		*m_p_mutex;
	iHeap		*m_p_heap;
	iSystemLog	*m_p_syslog;
	_u16		m_last_error;
	iBroadcast	*m_p_broadcast;

	_so_storage_t m_sos[MAX_SO_STORAGES];
	_u16 m_sos_idx;

	iHeap *get_heap(void) {
		if(!m_p_heap)
			m_p_heap = (iHeap *)get_object_by_interface(I_HEAP, RY_STANDALONE);
		return m_p_heap;
	}
	iMutex *get_mutex(void) {
		if(!m_p_mutex)
			m_p_mutex = (iMutex *)get_object_by_interface(I_MUTEX, RY_CLONE);
		return m_p_mutex;
	}
	iSystemLog *get_sys_log(void) {
		if(!m_p_syslog)
			m_p_syslog = (iSystemLog *)get_object_by_interface(I_SYSTEM_LOG, RY_STANDALONE);
		return m_p_syslog;
	}
	iBroadcast *get_broadcast(void) {
		if(!m_p_broadcast)
			m_p_broadcast = (iBroadcast *)get_object_by_interface(I_BROADCAST, RY_STANDALONE);
		return m_p_broadcast;
	}
	HMUTEX lock(HMUTEX hm=0) {
		HMUTEX r = 0;
		iMutex *p_mutex = get_mutex();
		if(p_mutex)
			r = p_mutex->lock(hm);
		return r;
	}
	void unlock(HMUTEX hm) {
		iMutex *p_mutex = get_mutex();
		if(p_mutex)
			p_mutex->unlock(hm);
	}
	bool object_compare(_object_request_t *_or, _object_t *po) {
		bool r = false;
		if(po->p_obj && !(po->flags & RY_DELETED)) {
			if(_or->r_flags & ORF_CMP_OR) {
				if(_or->r_flags & ORF_NAME) {
					if(str_cmp(_or->name, po->p_obj->object_name()) == 0)
						r = true;
				}
				if(_or->r_flags & ORF_INTERFACE) {
					if(str_cmp(_or->interface, (_str_t)(po->p_obj->interface_name())) == 0)
						r = true;
				}
				if(_or->r_flags & ORF_PINTERFACE) {
					if(str_cmp(_or->pinterface, (_str_t)(po->p_obj->_interface_name())) == 0)
						r = true;
				}
				if(_or->r_flags & ORF_ID) {
					if(po->id != RY_NO_OBJECT_ID && _or->id != RY_NO_OBJECT_ID) {
						if(_or->id == po->id)
							r = true;
					}
				}
			} else {
				if(_or->r_flags & ORF_NAME) {
					if(str_cmp(_or->name, po->p_obj->object_name()) != 0)
						goto _object_compare_done_;
				}
				if(_or->r_flags & ORF_INTERFACE) {
					if(str_cmp(_or->interface, (_str_t)(po->p_obj->interface_name())) != 0)
						goto _object_compare_done_;
				}
				if(_or->r_flags & ORF_PINTERFACE) {
					if(str_cmp(_or->pinterface, (_str_t)(po->p_obj->_interface_name())) != 0)
						goto _object_compare_done_;
				}
				if(_or->r_flags & ORF_ID) {
					if(po->id != RY_NO_OBJECT_ID && _or->id != RY_NO_OBJECT_ID) {
						if(_or->id != po->id)
							goto _object_compare_done_;
					} else
						goto _object_compare_done_;
				}
				r = true;
			}
		}
		_object_compare_done_:
		return r;
	}
	_object_t *object_request(_object_request_t *_or, HMUTEX _UNUSED_ hlock=0) {
		_object_t *r = 0;
		HMUTEX _hlock = 0;
		bool irqs=true;
		if(m_p_mutex) {
			irqs = enable_interrupts(false);
			_hlock = m_p_mutex->lock(hlock);
		}

		_u32 idx = 0;

		for(_u16 i = 0; i < m_sos_idx; i++) {
			if(m_sos[i].so_storage) {
				for(_u16 j = 0; j < m_sos[i].count; j++,idx++) {
					_object_t *po = &(m_sos[i].so_storage[j]);
					if(object_compare(_or, po)) {
						r = po;
						goto _object_request_done_;
					}
				}
			}
		}

		_object_request_done_:
		if(m_p_mutex) {
			m_p_mutex->unlock(_hlock);
			enable_interrupts(irqs);
		}
		return r;
	}
	_so_storage_t *validate_so_index(_ulong so_idx) {
		_so_storage_t *r = 0;
		for(_u16 i = 0; i < m_sos_idx; i++) {
			if(so_idx == (_ulong)&m_sos[i]) {
				r = &m_sos[i];
				break;
			}
		}
		return r;
	}

public:
	cRepository() {
		m_p_heap = 0;
		m_p_mutex = 0;
		m_sos_idx = 0;
		m_p_broadcast = 0;
		mem_set((_u8 *)m_sos, 0, sizeof(m_sos));
	}
	~cRepository() {}
	
	_ulong add_so_storage(_object_t so_storage[], _u16 count, _u16 limit) {
		_ulong r = INVALID_SO_STORAGE_INDEX;
		HMUTEX _hlock=0;
		bool irqs=true;
		if(m_p_mutex) {
			irqs = enable_interrupts(false);
			_hlock = m_p_mutex->lock();
		}

		_u16 so_idx = INVALID_SO_STORAGE_INDEX;
		//verify storage address
		for(_u16 i = 0; i < m_sos_idx; i++) {
			if(m_sos[i].so_storage) {
				if(m_sos[i].so_storage == so_storage) {
					goto _add_so_done_;
				}
			} else {
				if(so_idx == INVALID_SO_STORAGE_INDEX)
					so_idx = i;
			}
		}

		// verify objects
		_object_request_t _or;
		_or.r_flags = ORF_NAME|ORF_ID;
		for(_u16 i = 0; i < count; i++) {
			if(so_storage[i].p_obj) {
				_or.name = so_storage[i].p_obj->object_name();
				_or.id = so_storage[i].id;
				_object_t *po = object_request(&_or, _hlock);
				if(po) { // remove this object
					so_storage[i].flags |= RY_DELETED;
				}
			} else
				so_storage[i].flags |= RY_DELETED;
		}

		//add storage
		if(so_idx != INVALID_SO_STORAGE_INDEX) {
			m_sos[so_idx].so_storage = so_storage;
			m_sos[so_idx].count = count;
			m_sos[so_idx].limit = limit;
			r = (_ulong)&m_sos[so_idx];
		} else {
			if(m_sos_idx < MAX_SO_STORAGES) {
				m_sos[m_sos_idx].so_storage = so_storage;
				m_sos[m_sos_idx].count = count;
				m_sos[m_sos_idx].limit = limit;
				r = (_ulong)&m_sos[m_sos_idx];
				m_sos_idx++;
			}
		}

		_add_so_done_:
		if(m_p_mutex) {
			m_p_mutex->unlock(_hlock);
			enable_interrupts(irqs);
		}
		return r;
	}
	bool remove_so_storage(_ulong so_idx) {
		bool r = false;
		_so_storage_t *p_sos = validate_so_index(so_idx);
		if(p_sos) {
			iMutex *p_mutex = get_mutex();
			HMUTEX hlock = 0;

			if(p_mutex)
				hlock = p_mutex->lock();

			for(_u16 i = 0; i < p_sos->count; i++) {
				_object_t *p = &(p_sos->so_storage[i]);
				iBroadcast *p_broadcast = get_broadcast();

				if(p_mutex)
					p_mutex->unlock(hlock);

				if(p_broadcast) {
					_msg_t msg={MSG_OBJECT_DESTROY, (_ulong)p->p_obj};
					p_broadcast->send_msg(&msg);
				}
				p->p_obj->object_destroy();

				if(p_mutex)
					hlock = p_mutex->lock();
			}

			p_sos->so_storage = 0;
			p_sos->count = 0;
			p_sos->limit = 0;
			r = true;
			if(p_mutex)
				p_mutex->unlock(hlock);
		}

		return r;
	}
	bool init_so_storage(_ulong so_idx) {// require initialized heap & mutex
		bool r = false;
		_so_storage_t *p_sos = validate_so_index(so_idx);
		if(p_sos) {
			if(p_sos->so_storage) {
				for(_u16 i = 0; i < p_sos->count; i++) {
					_object_t *po = &(p_sos->so_storage[i]);
					if(po->flags & RY_STANDALONE) {
						if(!(po->flags & (RY_READY|RY_DELETED)) && po->p_obj) {
							if(po->p_obj->object_init(this))
								po->flags |= RY_READY;
							else
								LOG("Failed to init object '%s'\r\n", 
									po->p_obj->object_name());
						}
					} else
						po->flags |= RY_READY;
				}
				r = true;
			}
		}
		get_mutex();
		return r;
	}
	iBase *get_object(_object_request_t *_or, _ry_flags_t flags, _ulong limit=DEFAULT_RAM_LIMIT) {
		iBase * r = 0;
		_object_t *po = object_request(_or);
		if(po) {
			if(po->flags & flags) {
				if((flags & po->flags) & RY_CLONE) {
					iHeap *p_heap = get_heap();
					if(p_heap) {
						if((r = p_heap->clone(po->p_obj, limit))) {
							if(r->object_init(this) == true)
								po->ref_cnt++;
							else {
								release_object(r);
								r = 0;
							}
						}
					}
				} else {
					if((flags & po->flags) & RY_STANDALONE) {
						r = po->p_obj;
						po->ref_cnt++;
					}
				}
			}
		}
		return r;
	}
	iBase *get_object_by_interface(_cstr_t interface_name, _ry_flags_t flags, 
						_ulong limit=DEFAULT_RAM_LIMIT) {
		_object_request_t _or;
		_or.r_flags = ORF_INTERFACE;
		_or.interface = (_str_t)interface_name;
		return get_object(&_or, flags, limit);
	}
	iBase *get_object_by_parent_interface(_cstr_t interface_name, _ry_flags_t flags, 
						_ulong limit=DEFAULT_RAM_LIMIT) {
		_object_request_t _or;
		_or.r_flags = ORF_PINTERFACE;
		_or.pinterface = (_str_t)interface_name;
		return get_object(&_or, flags, limit);
	}
	iBase *get_object_by_name(_cstr_t object_name, _ry_flags_t flags, _ulong limit=DEFAULT_RAM_LIMIT) {
		_object_request_t _or;
		_or.r_flags = ORF_NAME;
		_or.name = (_str_t)object_name;
		return get_object(&_or, flags, limit);
	}
	iBase *get_object_by_id(_ry_id_t id, _ry_flags_t flags, _ulong limit=DEFAULT_RAM_LIMIT) {
		_object_request_t _or;
		_or.r_flags = ORF_ID;
		_or.id = id;
		return get_object(&_or, flags, limit);
	}
	bool get_object_info(_object_request_t *req, _object_info_t *p_info) {
		bool r = false;
		_object_t *p = object_request(req);
		if(p) {
			if(p->p_obj) {
				p_info->_interface_name = p->p_obj->_interface_name();
				p_info->interface_name = p->p_obj->interface_name();
				p_info->object_name = p->p_obj->object_name();
				p_info->flags = p->flags;
				p_info->id = p->id;
				p_info->size = p->p_obj->object_size();
				r = true;
			}
		}
		return r;
	}
	void release_object(iBase *p_obj) {
		if(p_obj) {
			_object_request_t _or;
			_or.r_flags = ORF_NAME|ORF_INTERFACE;
			_or.interface = (_str_t)p_obj->interface_name();
			_or.name = p_obj->object_name();

			_object_t *p = object_request(&_or);
			if(p) {
				if(p->ref_cnt)
					p->ref_cnt--;
			}

			iHeap *p_heap = get_heap();
			if(p_heap) {
				if(p_heap->verify(p_obj, p_obj->object_size())) {
					iBroadcast *p_broadcast = get_broadcast();
					if(p_broadcast) {
						_msg_t msg={MSG_OBJECT_DESTROY, (_ulong)p_obj};
						p_broadcast->send_msg(&msg);
					}
					p_obj->object_destroy();
					p_heap->free(p_obj, p_obj->object_size());
				}
			}
		}
	}
	_u32 objects_count(void) {
		_u32 r = 0;
		for(_u16 i = 0; i < m_sos_idx; i++) {
			r += m_sos[i].count;
		}
		return r;
	}
	_u16 last_error(void) {
		return m_last_error;
	}
};

cRepository	__g_repository__; 
iRepository	*__g_p_repository__ = &__g_repository__;

