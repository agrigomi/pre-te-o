#include "iDataStorage.h"
#include "sha1.h"
#include "str.h"

struct __attribute__((packed)) _bth_t {
	_bth_t		*pl;	// left
	_bth_t		*pr;	// right
	_bth_t		*pp;	// parent
	_bt_key_t	key;	// comparsion key
	_u32		dsize;	// data size
};

class cBTree:public iBTree {
public:
	cBTree() {
		m_p_heap = 0;
		m_p_mutex = 0;
		m_p_first = 0;
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}
	~cBTree() {}

	DECLARE_BASE(cBTree);
	DECLARE_INIT();
	DECLARE_DESTROY();

	void init(iHeap *p_heap=0);
	_bt_key_t add(void *data, _u32 size, HMUTEX hlock=0);
	_bt_key_t key(void *data, _u32 size);
	void *get(_bt_key_t key, _u32 *dsz=0, HMUTEX hlock=0);
	void del(_bt_key_t key, HMUTEX hlock=0);
	HMUTEX lock(HMUTEX hlock=0);
	void unlock(HMUTEX hlock);
	iMutex *get_mutex(void);

protected:
	iHeap	*m_p_heap;
	iMutex	*m_p_mutex;
	_bth_t	*m_p_first;

	_bt_key_t make_key(void *data, _u32 sz);
	bool find(_bt_key_t key, _bth_t **pp_last, _bth_t *p_begin=0);
	_bth_t *alloc_record(_u32 size);
	void attach(_bth_t *p_rec, _bth_t *p_parent=0);
};

IMPLEMENT_BASE(cBTree, "cBTree", 0x0001);

IMPLEMENT_INIT(cBTree, rbase) {
	REPOSITORY = rbase;
	m_p_heap = (iHeap *)OBJECT_BY_INTERFACE(I_HEAP, RY_STANDALONE);
	m_p_mutex = (iMutex *)OBJECT_BY_INTERFACE(I_MUTEX, RY_CLONE);
	return true;
}

IMPLEMENT_DESTROY(cBTree) {
	RELEASE_OBJECT(m_p_mutex);
}

_bt_key_t cBTree::make_key(void *data, _u32 sz) {
	_bt_key_t r = 0;
	SHA1Context shcxt;
	
	SHA1Reset(&shcxt);
	SHA1Input(&shcxt, (const uint8_t *)data, sz);
	SHA1Result(&shcxt, (uint8_t *)&r, sizeof(r));
	return r;
}

void cBTree::init(iHeap *p_heap) {
	m_p_heap = p_heap;
}

bool cBTree::find(_bt_key_t key, _bth_t **pp_last, _bth_t *p_begin) {
	bool r = false;
	*pp_last = (p_begin)?p_begin:m_p_first;
	if(*pp_last) {
		_bth_t *pc = *pp_last;
		while(pc) {
			*pp_last = pc;

			if(key == pc->key) {
				r = true;
				break;
			}

			if(key < pc->key)
				pc = pc->pl;
			else
				pc = pc->pr;
		}
	}
	return r;
}

void cBTree::attach(_bth_t *p_rec, _bth_t *p_parent) {
	_bth_t *ppr = 0;

	if(!find(p_rec->key, &ppr, p_parent)) {
		if(ppr) {
			if(p_rec->key < ppr->key)
				ppr->pl = p_rec;
			else
				ppr->pr = p_rec;

			p_rec->pp = ppr;
		}
	}
}

_bth_t *cBTree::alloc_record(_u32 size) {
	_bth_t *r = 0;
	if(m_p_heap) {
		r = (_bth_t *)m_p_heap->alloc(size + sizeof(_bth_t));
		if(r) {
			r->pl = r->pr = r->pp = 0;
			r->key = 0;
			r->dsize = size;
		}
	}
	return r;
}

_bt_key_t cBTree::add(void *data, _u32 size, HMUTEX hlock) {
	_bt_key_t r = 0;
	_bth_t *p_rec = 0;
	HMUTEX _hlock = lock(hlock);
	_bt_key_t key = make_key(data, size);

	if(find(key, &p_rec)) {
		if(p_rec)
			r = p_rec->key;
	} else {
		_bth_t *p_new_rec = alloc_record(size);
		if(p_new_rec) {
			p_new_rec->key = key;

			// copy data content
			_u8 *p_data = (_u8 *)p_new_rec;
			mem_cpy(p_data+sizeof(_bth_t), (_u8 *)data, size);

			if(p_rec)
				//attach new record
				attach(p_new_rec, p_rec);
			else
				// this is a first record !!!
				m_p_first = p_new_rec;

			r = key;
		}
	}

	unlock(_hlock);
	return r;
}

_bt_key_t cBTree::key(void *data, _u32 size) {
	return make_key(data, size);
}

void *cBTree::get(_bt_key_t key, _u32 *dsz, HMUTEX hlock) {
	void *r = 0;
	_bth_t *p_rec = 0;
	HMUTEX _hlock = lock(hlock);

	if(find(key, &p_rec)) {
		_u8 *p = (_u8 *)p_rec;
		if(dsz)
			*dsz = p_rec->dsize;
		r = p + sizeof(_bth_t);
	}

	unlock(_hlock);
	return r;
}

void cBTree::del(_bt_key_t key, HMUTEX hlock) {
	_bth_t *p_rec = 0;
	HMUTEX _hlock = lock(hlock);

	if(find(key, &p_rec)) {
		_bth_t *pp = p_rec->pp;
		_bth_t *pl = p_rec->pl;
		_bth_t *pr = p_rec->pr;

		if(pp) {
			// detach from parent
			if(pp->pl == p_rec)
				pp->pl = 0;
			else if(pp->pr == p_rec)
				pp->pr = 0;
		} else {
			if(p_rec == m_p_first) {
				// remove root record !!!
				if(pr) {
					pr->pp = 0;
					m_p_first = pr;
					pr = 0;
				} else {
					pl->pp = 0;
					m_p_first = pl;
					pl = 0;
				}
			}
		}

		if(pl)
			attach(pl);
		if(pr)
			attach(pr);

		if(m_p_heap)
			m_p_heap->free(p_rec, p_rec->dsize + sizeof(_bth_t));
	}

	unlock(_hlock);
}

HMUTEX cBTree::lock(HMUTEX hlock) {
	HMUTEX r = 0;
	if(m_p_mutex)
		r = m_p_mutex->lock(hlock);
	return r;
}

void cBTree::unlock(HMUTEX hlock) {
	if(m_p_mutex)
		m_p_mutex->unlock(hlock);
}

iMutex *cBTree::get_mutex(void) {
	return m_p_mutex;
}

