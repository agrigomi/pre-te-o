#include "repository.h"
#include "iMemory.h"
#include "iSyncObject.h"
#include "str.h"

#define RY_READY	(1<<7)
#define RY_DELETED	(1<<6)

// static objects array should be defined by ABI loader
extern _object_t __g_static_objects__[];
//////////////////////////////////////////////////////
class cRepository:public iRepository {
public:
	cRepository();
	virtual ~cRepository();
	
	_ulong add_so_storage(_object_t _UNUSED_ so_storage[], _u16 _UNUSED_ count, _u16 _UNUSED_ limit) {
		return 0;
	}
	bool remove_so_storage(_ulong _UNUSED_ so_idx) {
		return false;
	}
	bool init_so_storage(_ulong so_idx); // require initialized heap & mutex
	iBase *get_object_by_interface(_cstr_t interface_name, _ry_flags_t flags, _ulong limit=DEFAULT_RAM_LIMIT);
	iBase *get_object_by_parent_interface(_cstr_t interface_name, _ry_flags_t flags, _ulong limit=DEFAULT_RAM_LIMIT);
	iBase *get_object_by_name(_cstr_t object_name, _ry_flags_t flags, _ulong limit=DEFAULT_RAM_LIMIT);
	iBase *get_object_by_id(_ry_id_t id, _ry_flags_t flags, _ulong limit=DEFAULT_RAM_LIMIT);
	iBase *get_object(_object_request_t _UNUSED_ *oreq, _ry_flags_t _UNUSED_ flags, _ulong limit=DEFAULT_RAM_LIMIT) {
		return 0;
	}
	void release_object(iBase *p_obj);
	bool get_object_info(_object_request_t *req, _object_info_t *p_info) {
		bool r = false;
		//???
		return r;
	}
	bool get_object_info(_u32 idx, _object_info_t *p_info) {
		bool r = false;
		//???
		return r;
	}
	_u32 objects_count(void);
	
	_u16 last_error(void) {
		return m_last_error;
	}

protected:
	iMutex		*m_p_mutex;
	_u16		m_last_error;
	iHeap		*m_p_heap;
	
	HMUTEX lock(HMUTEX hm=0);
	void unlock(HMUTEX hm);

	_u16 allow_add(iBase *p_obj, _ry_flags_t flags, _ry_id_t id, _ulong *p_idx);
	_u16 allow_get(_object_t *p, _ry_flags_t req_flags, iBase **p_new_object, bool *bcall_init);
};

cRepository	__g_repository__; 
iRepository	*__g_p_repository__ = &__g_repository__;

cRepository::cRepository() {
	m_p_mutex = 0;
	m_p_heap = 0;
	m_last_error = RY_OK;
}

_u32 cRepository::objects_count(void) {
	return __g_static_objects_counter__;
}

bool cRepository::init_so_storage(_ulong _UNUSED_ so_idx) {
	bool r = false;

	// searching for standalone iHeap object
	m_p_heap = (iHeap *)get_object_by_interface(I_HEAP, RY_STANDALONE);
	if(m_p_heap) {
		m_p_mutex = (iMutex *)get_object_by_interface(I_MUTEX, RY_CLONE);

		// the second step initialization of all standalone objects
		for(_ulong i = 0; i < __g_static_objects_counter__; i++) {
			if(__g_static_objects__[i].flags & RY_STANDALONE && 
					!(__g_static_objects__[i].flags & RY_READY)) {
				if(__g_static_objects__[i].p_obj->object_init(this))
					__g_static_objects__[i].flags |= RY_READY;
			}
		}
		r = true;
	}

	return r;
}

cRepository::~cRepository() {
}

HMUTEX cRepository::lock(HMUTEX hm) {
	HMUTEX r = 0;
	
	if(m_p_mutex)
		r = m_p_mutex->lock(hm);
	
	return r;
}

void cRepository::unlock(HMUTEX hm) {
	if(m_p_mutex)
		m_p_mutex->unlock(hm);
}

_u16 cRepository::allow_add(iBase *p_obj, _ry_flags_t flags, _ry_id_t id, _ulong *p_idx) {
	_u16 r = RY_ERR;
	
	*p_idx = __g_static_objects_counter__;
	
	if(__g_static_objects_counter__ < MAX_STATIC_OBJECTS) {
		_ulong i = 0;
		
		while(i < __g_static_objects_counter__) {
			_object_t *p = &__g_static_objects__[i];
			
			if(p->p_obj && p->flags) {
				// check id
				if(id != RY_NO_OBJECT_ID && p->id == id) {
					r = RY_ERR_DUPLICATE_ID;
					break;
				}
				
				// check name
				if(str_cmp(p->p_obj->object_name(), p_obj->object_name()) == 0) {
					// check flags
					if(p->flags & RY_UNIQUE) {
						r = RY_ERR_ALREADY_EXISTS;
						break;
					}
					
					if(flags == p->flags && id == p->id) {
						r = RY_ERR_ALREADY_EXISTS;
						break;
					}
				}
			} else
				*p_idx = i;
			
			i++;
		}
		
		if(i == __g_static_objects_counter__)
			r = RY_OK;
	} else
		r = RY_ERR_NO_SPACE;
	
	return r;
}

_u16 cRepository::allow_get(_object_t *p, _ry_flags_t req_flags, iBase **p_new_object, bool *bcall_init) {
	_u16 r = RY_ERR;
	
	if(req_flags & p->flags) {
		if(req_flags & p->flags & RY_CLONE) { // check for cloneable flags
			if(m_p_heap) {
				// clone object
				*p_new_object = m_p_heap->clone(p->p_obj);
				if(*p_new_object) {
					r = RY_OK;
					*bcall_init = true;
					p->ref_cnt++;
				} else
					r = RY_ERR_NO_SPACE;
			}
		} else { // check for standalone flags
			if(req_flags & p->flags & RY_STANDALONE) {
				// return pointer to standalone object
				*p_new_object = p->p_obj;
				r = RY_OK;
				*bcall_init = false;
				p->ref_cnt++;
			} else
				r = RY_ERR_INVALID_FLAGS;
		}
	} else
		r = RY_ERR_INVALID_FLAGS;
	
	return r;
}
/*
_u16 cRepository::add_object(iBase *p_obj, _ry_flags_t flags, _ry_id_t id, bool init_now) {
	_u16 r = RY_ERR;
	bool call_init = false;
	_ulong idx;

	HMUTEX _hlock = lock();
	
	if((r = allow_add(p_obj,flags,id, &idx)) == RY_OK) { // register new object
		_object_t *p_new = &__g_static_objects__[idx];
		p_new->flags = flags;
		p_new->id = id;
		p_new->p_obj = p_obj;
		p_new->ref_cnt = 0;
		
		// initialize the STANDALONE objects emediately
		if((flags & RY_STANDALONE) && init_now)
			call_init = true;
		
		if(idx == __g_static_objects_counter__)
			__g_static_objects_counter__++;
	}
	
	unlock(_hlock);
	
	if(r == RY_OK && call_init) {
		if(!p_obj->object_init(this))
			r = RY_OBJECT_INIT_FAILED;
	}
	
	m_last_error = r;

	return r;
}
*/
iBase *cRepository::get_object_by_interface(_cstr_t interface_name, _ry_flags_t flags, _ulong limit) {
	iBase *r = 0;
	_u16 _error = RY_ERR;
	_slong i = __g_static_objects_counter__ - 1;
	bool call_init = false;
	
	HMUTEX _hlock = lock();
	
	while(i >= 0) {
		_object_t *p = &__g_static_objects__[i];
		if(p->p_obj) {
			if(str_cmp((_str_t)interface_name, (_str_t)p->p_obj->interface_name()) == 0) {
				if((_error = allow_get(p, flags, &r, &call_init)) == RY_OK)
					break;
			}
		}
		
		i--;
	}

	unlock(_hlock);
	
	if(r && call_init) {
		if(r->object_init(this) == false) {
			release_object(r);
			r = 0;
		}
	}
	
	m_last_error = _error;
	
	return r;
}

iBase *cRepository::get_object_by_parent_interface(_cstr_t interface_name, _ry_flags_t flags, _ulong limit) {
	iBase *r = 0;
	_u16 _error = RY_ERR;
	_slong i = __g_static_objects_counter__ - 1;
	bool call_init = false;
	
	HMUTEX _hlock = lock();
	
	while(i >= 0) {
		_object_t *p = &__g_static_objects__[i];
		if(p->p_obj) {
			if(str_cmp((_str_t)interface_name, (_str_t)p->p_obj->_interface_name()) == 0) {
				if((_error = allow_get(p, flags, &r, &call_init)) == RY_OK)
					break;
			}
		}
		
		i--;
	}

	unlock(_hlock);
	
	if(r && call_init) {
		if(r->object_init(this) == false) {
			release_object(r);
			r = 0;
		}
	}
	
	m_last_error = _error;
	
	return r;
}

iBase *cRepository::get_object_by_name(_cstr_t object_name, _ry_flags_t flags, _ulong limit) {
	iBase *r = 0;
	_u16 _error = RY_ERR;
	_ulong i = 0;
	bool call_init = false;
	
	HMUTEX _hlock = lock();
	
	while(i < __g_static_objects_counter__) {
		_object_t *p = &__g_static_objects__[i];
		if(p->p_obj) {
			if(str_cmp((_str_t)object_name, (_str_t)p->p_obj->object_name()) == 0) {
				if((_error = allow_get(p, flags, &r,&call_init)) == RY_OK)
					break;
				else
					r = 0;
			}
		}
		i++;
	}

	unlock(_hlock);

	if(r && call_init) {
		if(r->object_init(this) == false) {
			release_object(r);
			r = 0;
		}
	}
	
	m_last_error = _error;
	
	return r;
}

iBase *cRepository::get_object_by_id(_ry_id_t id, _ry_flags_t flags, _ulong limit) {
	iBase *r = 0;
	_u16 _error = RY_ERR;
	_ulong i = 0;
	bool call_init = false;
	
	HMUTEX _hlock = lock();
	
	if(id != RY_NO_OBJECT_ID) {
		while(i < __g_static_objects_counter__) {
			_object_t *p = &__g_static_objects__[i];
			if(p->p_obj) {
				if(id == p->id) {
					if((_error = allow_get(p, flags, &r,&call_init)) == RY_OK)
						break;
				}
			}
			
			i++;
		}
	}
	
	unlock(_hlock);
	
	if(r && call_init) {
		if(r->object_init(this) == false) {
			release_object(r);
			r = 0;
		}
	}

	m_last_error = _error;
	
	return r;
}

void cRepository::release_object(iBase *p_obj) {
	_ulong i = 0;
	_u32 idx = 0xffffffff;
	
	HMUTEX _hlock = lock();
	while(i < __g_static_objects_counter__) {
		_object_t *p = &__g_static_objects__[i];
		if(p->p_obj && p->p_obj == p_obj) {
			idx = i;
			break;
		}
		i++;
	}
	unlock(_hlock);
	
	if(idx != 0xffffffff) { // index of standalone object
		if(__g_static_objects__[i].ref_cnt)
			__g_static_objects__[i].ref_cnt--;
	} else { // search in heap for cloned object
		if(m_p_heap) {
			if(m_p_heap->verify(p_obj)) {
				// found the cloned object
				HMUTEX _hlock = lock();
				
				// search fot object model in repository
				i = 0;
				while(i < __g_static_objects_counter__) {
					_object_t *p = &__g_static_objects__[i];
					if(p->p_obj && str_cmp(p->p_obj->object_name(), p_obj->object_name()) == 0) {
						// found the object model
						if(p->ref_cnt) // decrease reference counter
							p->ref_cnt--;
						
						break;
					}
					i++;
				}
		
				unlock(_hlock);
			
				p_obj->object_destroy();
				m_p_heap->free(p_obj);
			}
		}
	}
}
