#include "iDataStorage.h"
#include "ll_alg.h"
#include "str.h"

void *_llv_alloc_(_u32 size, void *p_udata);
void _llv_free_(void *ptr, _u32 size, void *p_udata);
_u64 _llv_lock_(_u64 hlock, void *p_udata);
void _llv_unlock_(_u64 hlock, void *p_udata);


class cVector:public iVector {
protected:
	iMutex		*m_p_mutex;
	iHeap		*m_p_heap;
	_ll_context_t	m_ll_cxt;

	friend void *_llv_alloc_(_u32 size, void *p_udata);
	friend void _llv_free_(void *ptr, _u32 size, void *p_udata);
	friend _u64 _llv_lock_(_u64 hlock, void *p_udata);
	friend void _llv_unlock_(_u64 hlock, void *p_udata);

public:
	cVector() {
		m_p_mutex = 0;
		m_p_heap = 0;
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}
	cVector(iRepositoryBase *p_repository, void *heap_base, _u32 heap_sz)  {// outside repositor construction
		REPOSITORY = p_repository;
	
		// create own heap object
		m_p_heap = (iHeap *)OBJECT_BY_INTERFACE(I_HEAP, RY_CLONE);
		m_p_mutex = (iMutex *)OBJECT_BY_INTERFACE(I_MUTEX, RY_CLONE);
	
		m_p_heap->init(heap_base, heap_sz);
	}
	~cVector() {
		object_destroy();
	}

	DECLARE_BASE(cVector);
	bool object_init(iRepositoryBase *p_rb) {
		REPOSITORY = p_rb;
		m_p_heap = (iHeap *)OBJECT_BY_INTERFACE(I_HEAP, RY_STANDALONE);
		m_p_mutex = (iMutex *)OBJECT_BY_INTERFACE(I_MUTEX, RY_CLONE);
		return true;
	}
	void object_destroy(void) {
		if(m_p_heap) {
			ll_clr(&m_ll_cxt);
		}
		if(m_p_heap)
			RELEASE_OBJECT(m_p_heap);
		if(m_p_mutex)
			RELEASE_OBJECT(m_p_mutex);
	}

	void init(_u32 data_sz, _u8 ncol=1, iHeap *p_heap=0) {
		if(p_heap)
			m_p_heap = p_heap;

		m_ll_cxt.mode 	 = LL_MODE_VECTOR;
		m_ll_cxt.size 	 = data_sz;
		m_ll_cxt.p_alloc = _llv_alloc_;
		m_ll_cxt.p_free  = _llv_free_;
		m_ll_cxt.p_lock  = _llv_lock_;
		m_ll_cxt.p_unlock= _llv_unlock_;
		m_ll_cxt.p_udata = this;
		ll_init(&m_ll_cxt, ncol);
	}

	void *get(_u32 index, HMUTEX hlock=0) {
		return ll_get(&m_ll_cxt, index, (_u64)hlock);
	}
	void *add(void *p_data, HMUTEX hlock=0) {
		return ll_add(&m_ll_cxt, p_data, (_u64)hlock);
	}
	void *ins(_u32 index, void *p_data, HMUTEX hlock=0) {
		return ll_ins(&m_ll_cxt, index, p_data, (_u64)hlock);
	}
	void del(_u32 index, HMUTEX hlock=0) {
		ll_del(&m_ll_cxt, index, (_u64)hlock);
	}
	void del(HMUTEX hlock=0) {
		ll_del(&m_ll_cxt, (_u64)hlock);
	}
	void clr(HMUTEX hlock=0) {
		ll_clr(&m_ll_cxt, (_u64)hlock);
	}
	_u32 cnt(HMUTEX hlock=0) {
		return ll_cnt(&m_ll_cxt, (_u64)hlock);
	}
	void col(_u8 ccol, HMUTEX hlock=0) {
		ll_col(&m_ll_cxt, ccol, hlock);
	}
	bool sel(void *p_data, HMUTEX hlock=0) {
		return ll_sel(&m_ll_cxt, p_data, hlock);
	}
	bool mov(void *p_data, _u8 col, HMUTEX hlock=0) {
		return ll_mov(&m_ll_cxt, p_data, col, hlock);
	}
	void *next(HMUTEX hlock=0) {
		return ll_next(&m_ll_cxt, (_u64)hlock);
	}
	void *current(HMUTEX hlock=0) {
		return ll_current(&m_ll_cxt, (_u64)hlock);
	}
	void *first(HMUTEX hlock=0) {
		return ll_first(&m_ll_cxt, (_u64)hlock);
	}
	void *last(HMUTEX hlock=0) {
		return ll_last(&m_ll_cxt, (_u64)hlock);
	}
	void *prev(HMUTEX hlock=0) {
		return ll_prev(&m_ll_cxt, (_u64)hlock);
	}
	HMUTEX lock(HMUTEX hlock=0) {
		HMUTEX r = 0;
		if(m_p_mutex)
			r = m_p_mutex->lock(hlock);
		return r;
	}
	void unlock(HMUTEX hlock) {
		if(m_p_mutex)
			m_p_mutex->unlock(hlock);
	}
	iMutex *get_mutex(void) {
		return m_p_mutex;
	}
};
IMPLEMENT_BASE(cVector, "cVector", 0x0002);

void *_llv_alloc_(_u32 size, void *p_udata) {
	cVector *p_obj = (cVector *)p_udata;
	return p_obj->m_p_heap->alloc(size);
}
void _llv_free_(void *ptr, _u32 size, void *p_udata) {
	cVector *p_obj = (cVector *)p_udata;
	p_obj->m_p_heap->free(ptr, size);
}
_u64 _llv_lock_(_u64 hlock, void *p_udata) {
	cVector *p_obj = (cVector *)p_udata;
	return (_u64)p_obj->m_p_mutex->lock((HMUTEX)hlock);
}
void _llv_unlock_(_u64 hlock, void *p_udata) {
	cVector *p_obj = (cVector *)p_udata;
	p_obj->m_p_mutex->unlock((HMUTEX)hlock);
}


void *_llq_alloc_(_u32 size, void *p_udata);
void _llq_free_(void *ptr, _u32 size, void *p_udata);
_u64 _llq_lock_(_u64 hlock, void *p_udata);
void _llq_unlock_(_u64 hlock, void *p_udata);

class cQueue:public iQueue {
protected:
	iMutex		*m_p_mutex;
	iHeap		*m_p_heap;
	_ll_context_t	m_ll_cxt;

	friend void *_llq_alloc_(_u32 size, void *p_udata);
	friend void _llq_free_(void *ptr, _u32 size, void *p_udata);
	friend _u64 _llq_lock_(_u64 hlock, void *p_udata);
	friend void _llq_unlock_(_u64 hlock, void *p_udata);
public:
	cQueue() {
		m_p_heap = 0;
		m_p_mutex = 0;
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}
	~cQueue() {
		object_destroy();
	}
	DECLARE_BASE(cQueue);
	bool object_init(iRepositoryBase *p_rb) {
		REPOSITORY = p_rb;
		m_p_heap = (iHeap *)OBJECT_BY_INTERFACE(I_HEAP, RY_STANDALONE);
		m_p_mutex = (iMutex *)OBJECT_BY_INTERFACE(I_MUTEX, RY_CLONE);
		return true;
	}
	void object_destroy(void) {
		if(m_p_heap) {
			ll_clr(&m_ll_cxt);
		}
		if(m_p_heap)
			RELEASE_OBJECT(m_p_heap);
		if(m_p_mutex)
			RELEASE_OBJECT(m_p_mutex);
	}

	void init(_u32 data_sz, _u8 ncol=1, iHeap *p_heap=0) {
		if(p_heap)
			m_p_heap = p_heap;

		m_ll_cxt.mode 	 = LL_MODE_QUEUE;
		m_ll_cxt.size 	 = data_sz;
		m_ll_cxt.p_alloc = _llq_alloc_;
		m_ll_cxt.p_free  = _llq_free_;
		m_ll_cxt.p_lock  = _llq_lock_;
		m_ll_cxt.p_unlock= _llq_unlock_;
		m_ll_cxt.p_udata = this;
		ll_init(&m_ll_cxt, ncol);
	}

	void *get(_u32 index, HMUTEX hlock=0) {
		return ll_get(&m_ll_cxt, index, (_u64)hlock);
	}
	void *add(void *p_data, HMUTEX hlock=0) {
		return ll_add(&m_ll_cxt, p_data, (_u64)hlock);
	}
	void *ins(_u32 index, void *p_data, HMUTEX hlock=0) {
		return ll_ins(&m_ll_cxt, index, p_data, (_u64)hlock);
	}
	void del(_u32 index, HMUTEX hlock=0) {
		ll_del(&m_ll_cxt, index, (_u64)hlock);
	}
	void del(HMUTEX hlock=0) {
		ll_del(&m_ll_cxt, (_u64)hlock);
	}
	void clr(HMUTEX hlock=0) {
		ll_clr(&m_ll_cxt, (_u64)hlock);
	}
	_u32 cnt(HMUTEX hlock=0) {
		return ll_cnt(&m_ll_cxt, (_u64)hlock);
	}
	void col(_u8 ccol, HMUTEX hlock=0) {
		ll_col(&m_ll_cxt, ccol, hlock);
	}
	bool sel(void *p_data, HMUTEX hlock=0) {
		return ll_sel(&m_ll_cxt, p_data, hlock);
	}
	bool mov(void *p_data, _u8 col, HMUTEX hlock=0) {
		return ll_mov(&m_ll_cxt, p_data, col, hlock);
	}
	void roll(HMUTEX hlock=0) {
		ll_roll(&m_ll_cxt, (_u64)hlock);
	}
	void *next(HMUTEX hlock=0) {
		return ll_next(&m_ll_cxt, (_u64)hlock);
	}
	void *current(HMUTEX hlock=0) {
		return ll_current(&m_ll_cxt, (_u64)hlock);
	}
	void *first(HMUTEX hlock=0) {
		return ll_first(&m_ll_cxt, (_u64)hlock);
	}
	void *last(HMUTEX hlock=0) {
		return ll_last(&m_ll_cxt, (_u64)hlock);
	}
	void *prev(HMUTEX hlock=0) {
		return ll_prev(&m_ll_cxt, (_u64)hlock);
	}
	HMUTEX lock(HMUTEX hlock=0) {
		HMUTEX r = 0;
		if(m_p_mutex)
			r = m_p_mutex->lock(hlock);
		return r;
	}
	void unlock(HMUTEX hlock) {
		if(m_p_mutex)
			m_p_mutex->unlock(hlock);
	}
	iMutex *get_mutex(void) {
		return m_p_mutex;
	}
};
IMPLEMENT_BASE(cQueue, "cQueue", 0x0002);

void *_llq_alloc_(_u32 size, void *p_udata) {
	cQueue *p_obj = (cQueue *)p_udata;
	return p_obj->m_p_heap->alloc(size);
}
void _llq_free_(void *ptr, _u32 size, void *p_udata) {
	cQueue *p_obj = (cQueue *)p_udata;
	p_obj->m_p_heap->free(ptr, size);
}
_u64 _llq_lock_(_u64 hlock, void *p_udata) {
	cQueue *p_obj = (cQueue *)p_udata;
	return (_u64)p_obj->m_p_mutex->lock((HMUTEX)hlock);
}
void _llq_unlock_(_u64 hlock, void *p_udata) {
	cQueue *p_obj = (cQueue *)p_udata;
	p_obj->m_p_mutex->unlock((HMUTEX)hlock);
}

