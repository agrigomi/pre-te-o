#include "iDataStorage.h"
#include "iMemory.h"
#include "str.h"

//#define _DEBUG_
#include "debug.h"

typedef struct {
	_u8 	sz_vname;	// size of variable name
	_u16	sz_value;
	_u16	sz_vdata;
	_str_t	data;
} __attribute__((packed)) _env_entry_t;

class cEnv:public iEnv {
protected:
	iVector *m_p_vector;
	iHeap	*m_p_heap;

	_str_t alloc_var_buffer(_str_t vname, // [in]
				_str_t val,  // [in]
				_u16 sz_val, // [in]
				_u16 *sz_buffer // [out]
				) {
		_str_t r = 0;
		*sz_buffer = str_len(vname) + sz_val + 2;
		if((r = (_str_t)m_p_heap->alloc(*sz_buffer))) {
			mem_set((_u8 *)r, 0, *sz_buffer);
			_u32 bidx = str_cpy(r, vname, str_len(vname)+1);
			*(r + bidx) = '=';
			bidx++;
			mem_cpy((_u8 *)r+bidx, (_u8 *)val, sz_val);
		}
		return r;
	}

	_env_entry_t *get(_str_t vname) {
		_env_entry_t *r = 0;
		DBG("ENV: get('%s') =", vname);
		HMUTEX hlock = m_p_vector->lock();
		_env_entry_t * _r = (_env_entry_t *)m_p_vector->first(hlock);
		if(_r) {
			do {
				if(str_len(vname) == _r->sz_vname) {
					if(mem_cmp((_u8 *)vname, (_u8 *)_r->data, _r->sz_vname) == 0) {
						r = _r;
						break;
					}
				}
			} while((_r = (_env_entry_t *)m_p_vector->next(hlock)));
		}
		m_p_vector->unlock(hlock);
#ifdef _DEBUG_
		if(r) {
			DBG("'%s'\r\n", r->data+r->sz_vname+1);
		} else {
			DBG("'%s'\r\n", "null");
		}
#endif
		return r;
	}

public:
	cEnv() {
		m_p_vector = 0;
		m_p_heap = 0;
		REGISTER_OBJECT(RY_ONCE, RY_NO_OBJECT_ID);
	}
	~cEnv() {}

	DECLARE_BASE(cEnv);

	bool object_init(iRepositoryBase *p_rb) {
		bool r = false;
		REPOSITORY=p_rb;
		m_p_vector = (iVector *)OBJECT_BY_INTERFACE(I_VECTOR, RY_CLONE);
		m_p_heap = (iHeap*)OBJECT_BY_INTERFACE(I_HEAP, RY_ONCE);
		if(m_p_vector && m_p_heap) {
			m_p_vector->init(sizeof(_env_entry_t));
			r = true;
		}
		return r;
	}
	
	void clr(void) {
		HMUTEX hlock = m_p_vector->lock();
		_env_entry_t *p = (_env_entry_t *)m_p_vector->first(hlock);
		if(p) {
			do {
				m_p_heap->free(p->data, p->sz_vdata);
			} while((p = (_env_entry_t *)m_p_vector->next(hlock)));
		}
		m_p_vector->clr(hlock);
		m_p_vector->unlock(hlock);
	}

	bool get(_str_t var, _str_t *val, _u32 *sz_val) {
		bool r = false;
		_env_entry_t *p = get(var);
		if(p) {
			*val = p->data + p->sz_vname + 1;
			*sz_val = p->sz_value;
			r = true;
		}
		return r;
	}
	
	bool get(_u32 idx, _str_t *val, _u32 *sz_val) {
		bool r = false;
		_env_entry_t *p = (_env_entry_t *)m_p_vector->get(idx);
		if(p) {
			*val = p->data + p->sz_vname + 1;
			*sz_val = p->sz_value;
			r = true;
		}
		return r;
	}
	
	bool set(_str_t var, _str_t val, _u32 sz_val) {
		bool r = false;

		_env_entry_t *p = get(var);
#ifdef _DEBUG_
		_s8 dbg_val[256]="";
		str_cpy(dbg_val, val, (sz_val) < sizeof(dbg_val)?sz_val:sizeof(dbg_val));
		DBG("ENV: set('%s') = '%s'\r\n", var, dbg_val);
#endif				
		if(p) {
			HMUTEX hlock = m_p_vector->lock();
			if(p->sz_value < sz_val) { // allocate new buffer
				m_p_heap->free(p->data);
				p->data = alloc_var_buffer(var, val, sz_val, &p->sz_vdata);
			} else { // reuse old buffer
				mem_set((_u8 *)(p->data + p->sz_vname + 1), 0, p->sz_value);
				mem_cpy((_u8 *)(p->data + p->sz_vname + 1), (_u8 *)val, sz_val);
			}
			p->sz_value = sz_val;
			m_p_vector->unlock(hlock);
		} else { // create new variable
			_env_entry_t env_new;
			env_new.sz_vname = str_len(var);
			env_new.sz_value = sz_val;
			if((env_new.data = alloc_var_buffer(var, val, sz_val, &env_new.sz_vdata))) {
				if(m_p_vector->add(&env_new))
					r = true;
			}
		}
		return r;
	}
};
IMPLEMENT_BASE(cEnv, "cEnv", 0x0001);

