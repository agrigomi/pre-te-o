#include "iMemory.h"

#define USE_MEM_CPY

#ifdef USE_MEM_CPY
#include "str.h"
#endif

class cHeap:public iHeap {
public:
	cHeap() {
	 	m_p_heap_base = 0;
		m_heap_sz = 0;
		m_count = 0;
		m_p_mutex = 0;
		m_check = 0;
		ERROR_CODE = 0;
		m_local_mutex = false;
		REGISTER_OBJECT(RY_CLONEABLE|RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	
	cHeap(void *heap_base, _u32 heap_sz);
	
	virtual ~cHeap();
	
	// iBase
	DECLARE_BASE(cHeap)
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
  	void	*m_p_heap_base;
	_u32	m_heap_sz;
	_u32	m_count;
	_u32	m_check;
	iMutex	*m_p_mutex;
	bool	m_local_mutex;
	
	HMUTEX lock(void);
	void unlock(HMUTEX hm);
	
	_u32 check(void);
	void pack(void);
	void *insert_in(_u32 index, _u32 sz);
	void *_alloc(_u32 sz);
	void _free(void *p);
	bool _verify(void *p);
};

#define FREE_PATTERN	0xcacacaca

IMPLEMENT_BASE(cHeap, "cRingHeap", 0x0001);

//void cHeap::object_destroy(void) {
IMPLEMENT_DESTROY(cHeap) {
	if(m_p_mutex) {
		if(!m_local_mutex) 
			RELEASE_OBJECT(m_p_mutex);
		
		m_p_mutex = 0;
	}

	m_count = 0;
	m_check = 0;
}

typedef struct {
 	_u32	sz;	// user memory size
	void	*ptr;	// pointer to user memory
}__attribute__((packed)) _heap_chunk_t;


//bool cHeap::object_init(iRepositoryBase *p_repository) {
IMPLEMENT_INIT(cHeap, p_repository) {
	bool r = false;
	
	REPOSITORY = p_repository;

	m_p_mutex = (iMutex *)OBJECT_BY_INTERFACE(I_MUTEX, RY_CLONE);
	if(m_p_mutex) {
		r = true;
		if(verify(m_p_mutex))
			// the mutex object is created in this heap
			m_local_mutex = true;
		
	}

	return r;
}

#include "repository.h"
cHeap::cHeap(void *heap_base, _u32 heap_sz) {
	init(heap_base, heap_sz);
	object_init(__g_p_repository__);
}

cHeap::~cHeap() {
	CALL_DESTROY();
}

void cHeap::init(void *p_base, _u32 heap_sz) {
	m_p_heap_base = p_base;
	m_heap_sz = heap_sz;
	m_count = 0;
	m_check = 0;
}

bool cHeap::init(_u32 heap_sz) {
	bool r = false;
	m_p_heap_base = 0;
	m_heap_sz = heap_sz;
	m_count = 0;
	m_check = 0;

	iPMA *p_pma = (iPMA *)__g_p_repository__->get_object_by_interface(I_PMA, RY_STANDALONE);
	if(p_pma) {
		_ram_info_t ri;
		p_pma->info(&ri);
		
		_u32 heap_pages = heap_sz / ri.ps;
		if((m_p_heap_base = p_pma->alloc(heap_pages)))
			r = true;

		RELEASE_OBJECT(p_pma);
	}

	return r;
}

HMUTEX cHeap::lock(void) {
 	HMUTEX r = 0;
 	
	if(m_p_mutex)
	 	r = m_p_mutex->lock(r);
	
	return r;
}

void cHeap::unlock(HMUTEX hm) {
 	if(m_p_mutex)
	 	m_p_mutex->unlock(hm);
}

void *cHeap::alloc(_u32 sz, _ulong limit) {
	void *r = 0;

	HMUTEX hm = lock();
	r = _alloc(sz);
	unlock(hm);
	
	return r;
}

void *cHeap::_alloc(_u32 sz) {
	void *r = 0;
	_u32 i = 0;
	_heap_chunk_t *p_chunk = (_heap_chunk_t *)m_p_heap_base;

	if(p_chunk) {
		if(m_check)
			return 0;
		
		_umax _sz = (sz <= sizeof(_umax)) ? (sizeof(_umax) * 2) : sz;
		
		r = ((_u8 *)m_p_heap_base + m_heap_sz) - _sz;
		
		while(i < m_count) {
			r = (p_chunk + i)->ptr;
			_u32 *p = (_u32 *)r;
			if(*p == FREE_PATTERN && _sz <= (p_chunk + i)->sz)
				break;

			r = (_u8 *)r - _sz; // step back with 'sz' bites from the current chunk
			i++;
		}

		if(i == m_count) {
			// check heap space
			if((_u8 *)r > ((_u8 *)m_p_heap_base + (m_count * sizeof(_heap_chunk_t)))) {
				// add new item to heap list
				(p_chunk + i)->sz = _sz;
				(p_chunk + i)->ptr = r;

				m_count++;
			} else // no memory
				r = 0;
		} else {
			if(((p_chunk + i)->sz - _sz) <= sizeof(_umax))
				// using the all size of existing chunk
				r = (p_chunk + i)->ptr;
			else
				// divide the chunk with index i 
				r = insert_in(i,_sz);
		}
		
		if(r) {
			_u32 *p = (_u32 *)r;
			*p = 0x00;
		}

		m_check = check();
	}

	return r;
}

void cHeap::free(void *p) {
	HMUTEX hm = lock();
	_free(p);
	unlock(hm);
}

void cHeap::free(void *p, _u32 _UNUSED_ size) {
	free(p);
}

void cHeap::_free(void *p) {
	_u32 *_p = (_u32 *)p;

	if(_verify(p)) {
		*_p = FREE_PATTERN;
		// remove the unused heap chunks
		pack();
	}
}

bool cHeap::_verify(void *p) {
	bool r = false;
	if(p) {
		_u32 *_p = (_u32 *)p;
		
		if(*_p != FREE_PATTERN) {
			_heap_chunk_t *p_chunk = (_heap_chunk_t *)m_p_heap_base;
			_u32 i = 0;
			
			while(i < m_count) {
				if((p_chunk + i)->ptr == p) {
					r = true;
					break;
				}
				
				i++;
			}
		}
	}
	
	return r;
}

bool cHeap::verify(void *p) {
	bool r = false;
	HMUTEX hm = lock();
	r = _verify(p);
	unlock(hm);
	return r;
}

bool cHeap::verify(void *p, _u32 _UNUSED_ size) {
	return verify(p);
}

// check heap
// return 0 if all is OK or index of corrupted chunk
_u32 cHeap::check(void) {
 	_u32 r = 0;
	_u32 i = 0;
	void *p = (_u8 *)m_p_heap_base + m_heap_sz;
	_heap_chunk_t *p_chunk = (_heap_chunk_t *)m_p_heap_base;
	
	for(i = 0; i < m_count; i++) {
	 	p = (_u8 *)p - (p_chunk + i)->sz;
		if(p != (p_chunk + i)->ptr || (p_chunk + i)->sz < sizeof(_umax)) {
		 	r = i;
			break;
		}
	}
	
	return r;
}

void cHeap::pack(void) {
 	_heap_chunk_t *p_chunk = (_heap_chunk_t *)m_p_heap_base;
	_u32 i = 0;
	_u32 j = 0xffffffff;
	
	while(i < m_count) {
	 	_u32 *p = (_u32 *)(p_chunk + i)->ptr;
		
	 	if(*p == FREE_PATTERN) {
		 	if(j != 0xffffffff) {
			 	(p_chunk + j)->sz += (p_chunk + i)->sz;
				(p_chunk + j)->ptr = (p_chunk + i)->ptr;
#ifdef USE_MEM_CPY
				_u8 *dst = (_u8 *)m_p_heap_base + (i * sizeof(_heap_chunk_t));
				_u8 *src = (_u8 *)m_p_heap_base + ((i + 1) * sizeof(_heap_chunk_t));
				_u32 sz = (m_count - i + 1) * sizeof(_heap_chunk_t);
				mem_cpy(dst, src, sz);
#else
				_u32 *dst = (_u32 *)(((_u8 *)m_p_heap_base + (i * sizeof(_heap_chunk_t))));
				_u32 *src = (_u32 *)(((_u8 *)m_p_heap_base + ((i + 1) * sizeof(_heap_chunk_t))));
				_u32 count = ((m_count - i + 1) * sizeof(_heap_chunk_t)) / sizeof(_u32);
				
				while(count) {
				 	*dst = *src;
					dst++;
					src++;
					count--;
				}
#endif				
				m_count--;
				continue;
			} else
			 	j = i;
		} else
		 	j = 0xffffffff;
		
	 	i++;
	}
	
	m_check = check();
}

void *cHeap::insert_in(_u32 index, _u32 sz) {
 	void *r = 0;
	_heap_chunk_t *p_chunk = (_heap_chunk_t *)m_p_heap_base;
	
	if(index < m_count && (p_chunk + index)->sz > sz) {
#ifdef USE_MEM_CPY
		_u8 *dst = (_u8 *)m_p_heap_base + ((index + 1) * sizeof(_heap_chunk_t));
		_u8 *src = (_u8 *)m_p_heap_base + (index * sizeof(_heap_chunk_t));
		_u32 _sz = (m_count - index) * sizeof(_heap_chunk_t);
		mem_cpy(dst, src, _sz);
#else
	 	_u32 *dst = (_u32 *)((_u8 *)m_p_heap_base + ((m_count + 1) * sizeof(_heap_chunk_t)));
		_u32 *src = (_u32 *)((_u8 *)m_p_heap_base + (m_count * sizeof(_heap_chunk_t)));
		_u32 count = ((m_count - index) * sizeof(_heap_chunk_t)) / sizeof(_u32);
		
		while(count) {
		 	*dst = *src;
			dst--;
			src--;
			count--;
		}
#endif		
		m_count++;
		(p_chunk + index + 1)->sz = (p_chunk + index)->sz - sz;
		(p_chunk + index)->sz = sz;
		r = (p_chunk + index)->ptr = (_u8 *)((p_chunk + index)->ptr) + (p_chunk + index + 1)->sz;
	}
	
	return r;
}

void cHeap::info(_heap_info_t *p_info) {
 	_heap_chunk_t *p_chunk = (_heap_chunk_t *)m_p_heap_base;
	_u32 i = 0;
	
	HMUTEX hm = lock();
	
	p_info->base = (_u64)m_p_heap_base;
	p_info->size = m_heap_sz;
	p_info->chunk_size = sizeof(_heap_chunk_t);
	
	p_info->free = 0;
	p_info->data_load = 0;
	p_info->meta_load = 0;
	p_info->unused = 0;

        while(i < m_count) {
		_u32 *p = (_u32 *)(p_chunk + i)->ptr;
		
		if(*p == FREE_PATTERN)
			p_info->free += (p_chunk + i)->sz;
		else
			p_info->data_load += (p_chunk + i)->sz;
		
		p_info->meta_load += sizeof(_heap_chunk_t);
		p_info->unused = p_info->size - (p_info->data_load + p_info->meta_load + p_info->free);
		i++;
	}
	
	unlock(hm);
}
