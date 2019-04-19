#include "iMemory.h"
#include "slab.h"
#include "repository.h"
#include "iScheduler.h"
#include "addr.h"


class cSlabHeap:public iHeap {
public:
	cSlabHeap() {
		m_p_mutex = 0;
		m_p_pma = 0;
		m_init = false;
		REGISTER_OBJECT(RY_STANDALONE|RY_CLONEABLE, RY_NO_OBJECT_ID);
	}

	~cSlabHeap() {}

	// iBase
	DECLARE_BASE(cSlabHeap)
	DECLARE_INIT();
	DECLARE_DESTROY();
	// iHeap
	void init(void *p_base, _u32 heap_sz);
	bool init(_u32 heap_sz);
	void *alloc(_u32 sz, _ulong limit=DEFAULT_HEAP_LIMIT);
	void free(void *p);
	void free(void *p, _u32 size);
	bool verify(void *p);
	bool verify(void *p, _u32 size);
	void info(_heap_info_t *p_info);

protected:
	_slab_context_t	m_scxt;
	iMutex		*m_p_mutex;
	iPMA		*m_p_pma;
	bool		m_init;
	bool		m_irqs;

	
	friend void *page_alloc(_u32 n, _ulong limit, void *p_udata);
	friend void page_free(void *ptr, _u32 n, void *p_udata);
	friend _slab_hlock_t slab_lock(_slab_hlock_t hlock, void *p_udata);
	friend void slab_unlock(_slab_hlock_t hlock, void *p_udata);
};

void *page_alloc(_u32 n, _ulong limit, void *p_udata) {
	void *r = 0;
	cSlabHeap *p_obj = (cSlabHeap *)p_udata;
	if(p_obj->m_p_pma) {
		_u32 pma_pages = (n * p_obj->m_scxt.page_size) / p_obj->m_p_pma->get_page_size();
		r = p_obj->m_p_pma->alloc(pma_pages, RT_SYSTEM, limit);
	}
	return r;
}
void page_free(void *ptr, _u32 n, void *p_udata) {
	cSlabHeap *p_obj = (cSlabHeap *)p_udata;
	if(p_obj->m_p_pma)
		p_obj->m_p_pma->free(ptr, n);
}
_slab_hlock_t slab_lock(_slab_hlock_t hlock, void *p_udata) {
	_slab_hlock_t r = 0;
	cSlabHeap *p_obj = (cSlabHeap *)p_udata;
	if(p_obj->m_p_mutex)
		r = acquire_mutex(p_obj->m_p_mutex, hlock);
	return r;
}
void slab_unlock(_slab_hlock_t hlock, void *p_udata) {
	cSlabHeap *p_obj = (cSlabHeap *)p_udata;
	if(p_obj->m_p_mutex)
		p_obj->m_p_mutex->unlock((HMUTEX)hlock);
}

IMPLEMENT_BASE(cSlabHeap, "cSlabHeap", 0x0001);
IMPLEMENT_INIT(cSlabHeap, rbase) {
	bool r = true;
	REPOSITORY = rbase;
	return r;
}
IMPLEMENT_DESTROY(cSlabHeap) {
	if(m_p_mutex) {
		RELEASE_OBJECT(m_p_mutex);
	}

	slab_destroy(&m_scxt);
}
void cSlabHeap::init(void _UNUSED_ *p_base, _u32 heap_sz) {
	init(heap_sz);
}
bool cSlabHeap::init(_u32 _UNUSED_ heap_sz) {
	bool r = false;
	m_init = false;
	m_scxt.p_mem_alloc = page_alloc;
	m_scxt.p_mem_free  = page_free;
	m_scxt.p_lock      = slab_lock;
	m_scxt.p_unlock    = slab_unlock;
	m_scxt.p_udata     = this;
	if((m_p_pma = (iPMA *)__g_p_repository__->get_object_by_interface(I_PMA, RY_STANDALONE))) {
		m_scxt.page_size   = m_p_pma->get_page_size();
		if((m_init = r = slab_init(&m_scxt)))
			m_p_mutex = (iMutex *)__g_p_repository__->get_object_by_interface(I_MUTEX, RY_CLONE);
	}
	return r;
}
void *cSlabHeap::alloc(_u32 sz, _ulong limit) {
	void *r = 0;
	if(m_init)
		r = slab_alloc(&m_scxt, sz, limit);
	return r;
}
void cSlabHeap::free(void *p) {
	if(m_init)
		slab_free(&m_scxt, p);
}
void cSlabHeap::free(void *p, _u32 size) {
	if(m_init)
		slab_free(&m_scxt, p, size);
}
bool cSlabHeap::verify(void *p) {
	bool r = false;
	if(m_init)
		r = slab_verify(&m_scxt, p);
	return r;
}
bool cSlabHeap::verify(void *p, _u32 size) {
	bool r = false;
	if(m_init)
		r = slab_verify(&m_scxt, p, size);
	return r;
}
void cSlabHeap::info(_heap_info_t *p_info) {
	_slab_status_t sst;
	if(m_init) {
		slab_status(&m_scxt, &sst);
		p_info->base = (_u64)m_scxt.p_slab;
		p_info->size = (sst.ndpg + sst.nspg) * m_scxt.page_size;
		p_info->chunk_size = sizeof(_slab_t);
		p_info->data_load = sst.ndpg * m_scxt.page_size;
		p_info->meta_load = sst.nspg * m_scxt.page_size;
		p_info->free = 0;
		p_info->unused = 0;
		p_info->objects = sst.naobj;
	}
}

