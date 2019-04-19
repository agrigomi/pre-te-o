#include "repository.h"
#include "iMemory.h"
#include "iSyncObject.h"
#include "str.h"
#include "iSystemLog.h"
#include "iScheduler.h"
#include "iDataStorage.h"

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
	_so_storage_t	m_so;
	bool		m_is_init;
	iVector		*m_p_vso;

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
	iVector *get_vector(void) {
		if(!m_p_vso) {
			m_p_vso = (iVector *)get_object_by_interface(I_VECTOR, RY_CLONE);
			if(m_p_vso)
				m_p_vso->init(sizeof(_so_storage_t));
		}
		return m_p_vso;
	}
	HMUTEX lock(HMUTEX hm=0) {
		HMUTEX r = 0;
		iMutex *p_mutex = get_mutex();
		if(p_mutex)
			r = acquire_mutex(m_p_mutex, hm);
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
	_object_t *object_request(_so_storage_t *p_sos, _object_request_t *_or, HMUTEX hlock=0) {
		_object_t *r = 0;
		if(p_sos) {
			HMUTEX _hlock = 0;
			if(m_p_mutex)
				_hlock = acquire_mutex(m_p_mutex, hlock);

			for(_u16 i = 0; i < p_sos->count; i++) {
				_object_t *po = &p_sos->so_storage[i];
				if(object_compare(_or, po)) {
					r = po;
					break;
				}
			}
			if(m_p_mutex)
				m_p_mutex->unlock(_hlock);
		}
		return r;
	}
	_object_t *object_request(_object_request_t *_or, HMUTEX hlock=0, HMUTEX hvlock=0) {
		_object_t *r = 0;
		if(m_is_init) {
			HMUTEX _hlock = 0;
			if(m_p_mutex)
				_hlock = acquire_mutex(m_p_mutex, hlock);

			if(!(r = object_request(&m_so, _or, _hlock))) {
				if((m_p_vso = get_vector())) {
					HMUTEX _hvlock = m_p_vso->lock(hvlock);
					_so_storage_t *p_sos = (_so_storage_t *)m_p_vso->first(_hvlock);
					if(p_sos) {
						do {
							if((r = object_request(p_sos, _or, _hlock)))
								break;
						}while((p_sos = (_so_storage_t *)m_p_vso->next(_hvlock)));
					}
					m_p_vso->unlock(_hvlock);
				}
			}
			if(m_p_mutex)
				m_p_mutex->unlock(_hlock);
		}
		return r;
	}
	_so_storage_t *validate_so_index(_ulong so_idx, HMUTEX hvlock=0) {
		_so_storage_t *r = 0;
		if(so_idx == (_ulong)&m_so)
			r = &m_so;
		else {
			if((m_p_vso = get_vector())) {
				HMUTEX _hvlock = m_p_vso->lock(hvlock);
				_so_storage_t *_r = (_so_storage_t *)m_p_vso->first(_hvlock);
				if(_r) {
					do {
						if((_ulong)_r == so_idx) {
							r = _r;
							break;
						}
					}while((_r = (_so_storage_t *)m_p_vso->next(_hvlock)));
				}
				m_p_vso->unlock(_hvlock);
			}
		}
		return r;
	}

	void remove_so_entry(_so_storage_t *p_sos, HMUTEX hvlock=0) {
		if(p_sos != &m_so) {
			if((m_p_vso = get_vector())) {
				HMUTEX _hvlock = m_p_vso->lock(hvlock);
				_so_storage_t *_p_sos = (_so_storage_t *)m_p_vso->first(_hvlock);
				if(_p_sos) {
					do {
						if(_p_sos == p_sos) {
							m_p_vso->del(_hvlock);
							break;
						}
					}while((_p_sos = (_so_storage_t *)m_p_vso->next(_hvlock)));
				}
				m_p_vso->unlock(_hvlock);
			}
		}
	}

public:
	cRepository() {
		m_p_heap = 0;
		m_p_mutex = 0;
		m_p_broadcast = 0;
		m_p_vso = 0;
		m_is_init = false;
		mem_set((_u8 *)&m_so, 0, sizeof(m_so));
	}
	~cRepository() {}

	_ulong add_so_storage(_object_t so_storage[], _u16 count, _u16 limit) {
		_ulong r = INVALID_SO_STORAGE_INDEX;
		HMUTEX _hlock = lock();
		if(!m_is_init) { // init main SO storage
			m_so.so_storage = so_storage;
			m_so.count = count;
			m_so.limit = limit;
			r = (_ulong)&m_so;
			m_is_init = true;
		} else {
			if((m_p_vso = get_vector())) {
				HMUTEX hvlock = m_p_vso->lock();
				// verify storage address
				_so_storage_t *p_sos = (_so_storage_t *)m_p_vso->first(hvlock);
				if(p_sos) {
					do {
						if(so_storage == p_sos->so_storage) {
							m_p_vso->unlock(hvlock);
							goto _add_so_done_;
						}
					}while((p_sos = (_so_storage_t *)m_p_vso->next(hvlock)));
				}
				// verify objects
				p_sos = (_so_storage_t *)m_p_vso->first(hvlock);
				if(p_sos) {
					do {
						_object_request_t _or;
						_or.r_flags = ORF_NAME|ORF_ID;
						for(_u16 i = 0; i < count; i++) {
							if(so_storage[i].p_obj) {
								_or.name = so_storage[i].p_obj->object_name();
								_or.id = so_storage[i].id;
								_object_t *po = object_request(&_or,
												_hlock,
												hvlock);
								if(po)
									so_storage[i].flags |= RY_DELETED;
							} else
								so_storage[i].flags |= RY_DELETED;
						}
					}while((p_sos = (_so_storage_t *)m_p_vso->next(hvlock)));
				}
				// add storage
				_so_storage_t so;
				so.so_storage = so_storage;
				so.count = count;
				so.limit = limit;
				void *ptr = m_p_vso->add(&so, hvlock);
				r = (_ulong)ptr;
				m_p_vso->unlock(hvlock);
			}
		}
		_add_so_done_:
		unlock(_hlock);
		return r;
	}
	bool remove_so_storage(_ulong so_idx) {
		bool r = false;
		_so_storage_t *p_sos = validate_so_index(so_idx);
		if(p_sos) {
			HMUTEX hlock = lock();
			_u16 i = 0;
			for(; i < p_sos->count; i++) {
				_object_t *p = &(p_sos->so_storage[i]);
				if(p->ref_cnt)
					break;
			}
			if(i == p_sos->count) {
				i = 0;
				iBroadcast *p_broadcast = get_broadcast();
				for(; i < p_sos->count; i++) {
					_object_t *p = &(p_sos->so_storage[i]);

					unlock(hlock);
					if(p_broadcast) {
						_msg_t msg={MSG_OBJECT_DESTROY, (_ulong)p->p_obj};
						p_broadcast->send_msg(&msg);
					}
					p->p_obj->object_destroy();
					hlock = lock();
				}
				remove_so_entry(p_sos);
				r = true;
			}
			unlock(hlock);
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
		if(p_sos == &m_so) {
			get_mutex();
			get_vector();
		}
		return r;
	}
	iBase *get_object(_object_request_t *_or, _ry_flags_t flags, _ulong limit=DEFAULT_RAM_LIMIT) {
		iBase *r = 0;
		HMUTEX hlock = 0;
		_object_t *po = object_request(_or, hlock);
		if(po) {
			if(po->flags & flags) {
				if((flags & po->flags) & RY_CLONE) {
					iHeap *p_heap = get_heap();
					if(p_heap) {
						if((r = p_heap->clone(po->p_obj, limit))) {
							if(r->object_init(this) == true) {
								po->ref_cnt++;
							} else {
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
		_u32 r = m_so.count;
		if((m_p_vso = get_vector())) {
			HMUTEX hvlock = m_p_vso->lock();
			_so_storage_t *p_so = (_so_storage_t *)m_p_vso->first(hvlock);
			if(p_so) {
				do {
					r += p_so->count;
				}while((p_so = (_so_storage_t *)m_p_vso->next(hvlock)));
			}
			m_p_vso->unlock(hvlock);
		}
		return r;
	}
	_u16 last_error(void) {
		return m_last_error;
	}
};

cRepository	__g_repository__;
iRepository	*__g_p_repository__ = &__g_repository__;

