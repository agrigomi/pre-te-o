#include "iDataStorage.h"

struct _vitem_hdr_t {
 	_vitem_hdr_t *prev;	// prev. item
	_vitem_hdr_t *next;	// next item
}__attribute__((packed));

class cVector:public iVector {
public:
  	cVector() {
	 	m_p_current = m_p_first = m_p_last = 0;
		m_count = m_current = m_size = m_err = 0;
		m_p_heap = 0;
		m_p_mutex = 0;
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}
	
	cVector(iRepositoryBase *p_repository, void *heap_base, _u32 heap_sz); // outside repositor construction
	
	virtual ~cVector();

	DECLARE_BASE(cVector);
	DECLARE_INIT();
	DECLARE_DESTROY();
	DECLARE_SELFCLONE(cVector);
		
	// iDataStorage
	void init(_u32 data_sz, iHeap *p_heap=0);
	// return pointer to data at 'index'
	void *get(_u32 index, HMUTEX hlock=0);
	// add item at the end of storage and return pointer to new allocated buffer
	void *add(void *p_data, HMUTEX hlock=0);
	// insert item in place passed by 'index' parameter and return pointer to new allocated buffer
	void *ins(_u32 index, void *p_data, HMUTEX hlock=0);
	// delete item
	void del(_u32 index, HMUTEX hlock=0);
	void del(HMUTEX hlock=0);
	// delete all items
	void clr(HMUTEX hlock=0);
	// return the number of items
	_u32 cnt(HMUTEX hlock=0);
	
	void *next(HMUTEX hlock=0);
	void *current(HMUTEX hlock=0);
	void *first(HMUTEX hlock=0);
	void *last(HMUTEX hlock=0);
	void *prev(HMUTEX hlock=0);

	HMUTEX lock(HMUTEX hlock=0);
	void unlock(HMUTEX hlock);
	iMutex *get_mutex(void);
	
protected:
  	iHeap		*m_p_heap;
	iMutex		*m_p_mutex;
  	_u16		m_err;		// error code
	_u32		m_count;	// count of items
	_u32		m_size;		// item data size
	_u32		m_current;	// index of the last requested item
	_vitem_hdr_t	*m_p_current;	// pointer to the last requested item
	_vitem_hdr_t	*m_p_first;	// pointer to first item
	_vitem_hdr_t	*m_p_last;	// pointer to last item
	
	void _cpy(_u8 *dst,_u8 *src,_u32 sz);
	_vitem_hdr_t *_get(_u32 index);
};


IMPLEMENT_BASE(cVector, "cVector", 0x0001);

IMPLEMENT_SELFCLONE(cVector) {
	cVector *r = 0;
	
	if(m_p_heap) {
		void *p = m_p_heap->alloc(object_size());
		if(p)
			r = object_clone(p);
	}
	
	return r;
}

IMPLEMENT_DESTROY(cVector) {
	// delete all
	clr();
	
	if(m_p_mutex) {
		if(REPOSITORY) {
			RELEASE_OBJECT(m_p_mutex);
		} else {
			m_p_mutex->object_destroy();
			if(m_p_heap)
				m_p_heap->free(m_p_mutex, m_p_mutex->object_size());
		}
	}
}

cVector::cVector(iRepositoryBase *p_repository, void *heap_base, _u32 heap_sz) {
	m_p_current = m_p_first = m_p_last = 0;
	m_count = m_current = m_size = m_err = 0;

	REPOSITORY = p_repository;
	
	// create own heap object
	m_p_heap = (iHeap *)OBJECT_BY_INTERFACE(I_HEAP, RY_CLONE);
	m_p_mutex = (iMutex *)OBJECT_BY_INTERFACE(I_MUTEX, RY_CLONE);
	
	m_p_heap->init(heap_base, heap_sz);
}

cVector::~cVector() {
	object_destroy();
}

IMPLEMENT_INIT(cVector, p_rb) {
	bool r = false;

	REPOSITORY = p_rb;
	m_p_heap = (iHeap *)OBJECT_BY_INTERFACE(I_HEAP, RY_STANDALONE);
	m_p_mutex = (iMutex *)OBJECT_BY_INTERFACE(I_MUTEX, RY_CLONE);
	
	if(m_p_heap && m_p_mutex)
		r = true;

	return r;
}

void cVector::init(_u32 data_sz, iHeap *p_heap) {
	m_size = data_sz;
	m_err = 0;
	m_count = 0;
	m_current = 0;
	m_p_current = 0;
	m_p_first = 0;
	m_p_last = 0;
	
	if(p_heap)
		m_p_heap = p_heap;
}

HMUTEX cVector::lock(HMUTEX hlock) {
	HMUTEX r = 0;
	
	if(m_p_mutex)
		r = m_p_mutex->lock(hlock);
	
	return r;
}

void cVector::unlock(HMUTEX hlock) {
	if(m_p_mutex)
		m_p_mutex->unlock(hlock);
}

iMutex *cVector::get_mutex(void) {
	return m_p_mutex;
}

_vitem_hdr_t *cVector::_get(_u32 index) {
	_u32 i = 0;
	_vitem_hdr_t *p = 0;
	
	if(index < m_count) {
		if((p = m_p_current)) {
			i = m_current;

			while(i != index && i < m_count) {
			    if(i > index) { // move back
				    p = p->prev;
				    i--;
				    if(!i)
					    break;
			    }

			    if(i < index) {	// move forward
				    p = p->next;
				    i++;
			    }
			}

			if(i == index) {
				// found the true index
				//   make this index current
				m_current = i;
				m_p_current = p;
			}
		}
	
	}
	
	return p;
}

void *cVector::get(_u32 index, HMUTEX hlock) {
 	_vitem_hdr_t *p = 0;
 	void *r = 0;
	
	HMUTEX hm = lock(hlock);

	// get the pointer to item header
	p = _get(index);
	if(p) {
		// move the pointer to user data area 
		//	(skip the item header)
		r = (p + 1);
	}
	
	unlock(hm);
	
	return r;
}

void *cVector::add(void *p_data, HMUTEX hlock) {
 	_vitem_hdr_t *_p = 0;
	_u32 sz = m_size + sizeof(_vitem_hdr_t);
	void *r = 0;
	
	HMUTEX hm = lock(hlock);
	
	m_err = 0;
	
	_p = (_vitem_hdr_t *)m_p_heap->alloc(sz);
	if(_p) {
	 	if(m_p_last)
		 	m_p_last->next = _p;
			
	 	_p->prev = m_p_last;
		_p->next = 0;
			
		// copy user data to new allocated memory
		_u8 *dst = (_u8 *)_p;
		_u8 *src = (_u8 *)p_data;
		_cpy(dst + sizeof(_vitem_hdr_t), src, m_size);
		//////////////////////////////////////////

		m_p_last = m_p_current = _p;
		
		if(!m_p_first)
		 	m_p_first = _p;
			
		// make the new item as current
		m_current = m_count;
		// increase items counter
		m_count++;
		// move the pointer to user data area 
		//	(skip the item header)
		r = (_p + 1);
	} else // assign the error code from heap
	 	m_err = m_p_heap->object_error();
	
	unlock(hm);
 
 	return r;
}

void *cVector::ins(_u32 index, void *p_data, HMUTEX hlock) {
 	_vitem_hdr_t *p_prev = 0; // prev
	_vitem_hdr_t *p_new = 0; // new
	_vitem_hdr_t *p_cur = 0; // current
	_u32 sz = m_size + sizeof(_vitem_hdr_t);
	void *r = 0;
	
	if(index < m_count) {
		HMUTEX hm = lock(hlock);
		
		m_err = 0;
	
		p_cur = _get(index);
		if(p_cur) {
			// allocate memory for the new item
			p_new = (_vitem_hdr_t *)m_p_heap->alloc(sz);
			if(p_new) {
				// initialize the new header
				p_new->prev = p_new->next = 0;
				
				// copy user data to new allocated area
				_u8 *dst = (_u8 *)p_new;
				_u8 *src = (_u8 *)p_data;
				_cpy(dst + sizeof(_vitem_hdr_t), src, m_size);
				////////////////////////////////////////
				
				if(index > 0) {
					p_prev = _get(index - 1);
					if(p_prev)
						p_prev->next = p_new;
				} else
					// replace the first item
					m_p_first = p_new;
				
				p_new->prev = p_prev;
				p_new->next = p_cur;
				p_cur->prev = p_new;
				
				m_p_current = p_new;
				m_current = index;
				m_count++;
				
				// skip header (return pointer to user data area)
				r = (p_new + 1);
			} else // allocation error (error code = heap error code)
				m_err = m_p_heap->object_error();
		}

		unlock(hm);
	} else
		add(p_data);
	
 	return r;
}

void cVector::del(_u32 index, HMUTEX hlock) {
 	HMUTEX hm=0;
	m_err = 0;

	hm = lock(hlock);
	
	_vitem_hdr_t *p_cur = _get(index);
	if(p_cur) {
	 	if(p_cur->prev)
		 	// releate prev to next
		 	p_cur->prev->next = p_cur->next;
		
		if(p_cur->next) {
 			m_p_current = p_cur->next;

		 	// releate next to prev
			p_cur->next->prev = p_cur->prev;
		}
		
		if(p_cur == m_p_first)
		 	m_p_current = m_p_first = p_cur->next;
		
		if(p_cur == m_p_last) {
		 	m_p_current = m_p_last = p_cur->prev;
			m_current--;
		}
			
		m_count--;
			
		// release memory
		_u32 sz = m_size + sizeof(_vitem_hdr_t);
		m_p_heap->free(p_cur, sz);
	}
	
	unlock(hm);
}

void cVector::del(HMUTEX hlock) {
 	HMUTEX hm=0;
	m_err = 0;

	hm = lock(hlock);

	_vitem_hdr_t *p_cur = m_p_current;
	if(p_cur) {
	 	if(p_cur->prev)
		 	// releate prev to next
		 	p_cur->prev->next = p_cur->next;
		
		if(p_cur->next) {
 			m_p_current = p_cur->next;

		 	// releate next to prev
			p_cur->next->prev = p_cur->prev;
		}
		
		if(p_cur == m_p_first)
		 	m_p_current = m_p_first = p_cur->next;
		
		if(p_cur == m_p_last) {
		 	m_p_current = m_p_last = p_cur->prev;
			m_current--;
		}
			
		m_count--;
			
		// release memory
		_u32 sz = m_size + sizeof(_vitem_hdr_t);
		m_p_heap->free(p_cur, sz);
	}
	
	
	unlock(hm);
}

void cVector::clr(HMUTEX hlock) {
	_vitem_hdr_t *p = m_p_first;
	
	HMUTEX hm = lock(hlock);
	_u32 sz = m_size + sizeof(_vitem_hdr_t);
	
 	for(_u32 i = 0; i < m_count; i++) {
	 	if(p) {
			_vitem_hdr_t *_p = p->next;
			m_p_heap->free(p, sz);
			p = _p;
			m_p_first = p;
		} else
		 	break;
	}
	
 	m_p_current = m_p_first = m_p_last = 0;
	m_count = m_current = m_err = 0;
	
	unlock(hm);
}

_u32 cVector::cnt(HMUTEX hlock) {
	_u32 r = 0;
	HMUTEX hm = lock(hlock);
	r = m_count;
	unlock(hm);
	return r; 
}

void cVector::_cpy(_u8 *dst,_u8 *src,_u32 sz) {
	_u32 _sz = sz;
	_u32 i = 0;
	
	while(_sz) {
	 	*(dst + i) = *(src + i);
		i++;
		_sz--;
	}
}

void *cVector::next(HMUTEX hlock) {
	void *r = 0;
	HMUTEX hm = lock(hlock);

	_vitem_hdr_t *p = m_p_current;
	if(p && p->next) {
		m_p_current = p->next;
		r = (m_p_current + 1);
		m_current++;
	}
	
	unlock(hm);
	return r;
}

void *cVector::current(HMUTEX hlock) {
	void *r =0;
	HMUTEX hm = lock(hlock);
	_vitem_hdr_t *p = m_p_current;
	if(p)
		r = p+1;
	
	unlock(hm);
	return r;
}

void *cVector::first(HMUTEX hlock) {
	void *r = 0;
	HMUTEX hm = lock(hlock);
	_vitem_hdr_t *p = m_p_first;
	if(p) {
		m_p_current = p;
		r = p+1;
		m_current = 0;
	}
	
	unlock(hm);
	return r;
}

void *cVector::last(HMUTEX hlock) {
	void *r = 0;
 	HMUTEX hm = lock(hlock);
	_vitem_hdr_t *p = m_p_last;
	if(p) {
		m_p_current = p;
		r = p+1;
		m_current = m_count - 1;
	}
	
	unlock(hm);
	return r;
}

void *cVector::prev(HMUTEX hlock) {
	void *r = 0;
 	HMUTEX hm = lock(hlock);
	_vitem_hdr_t *p = m_p_current;
	if(p && p->prev) {
		m_p_current = p->prev;
		r = (m_p_current + 1);
		m_current--;
	}
	
	unlock(hm);
	return r;
}

