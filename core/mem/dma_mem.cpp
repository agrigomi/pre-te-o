#include "iMemory.h"
#include "str.h"

#define DMA_PAGE_SIZE	1024
#define DMA_BITMAP_SIZE	128	// 1M

class cDMAMemory:public iDMAMemory {
public:
	cDMAMemory() {
		m_p_mem_base = 0;
		m_size = 0;
		m_p_mutex = 0;
		m_used_pages = 0;
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	//virtual ~cDMAMemory() {}
	
	// iBase
	DECLARE_BASE(cDMAMemory)
	DECLARE_INIT();
	DECLARE_DESTROY();
	
	void init(_u8 *mem_base, _u32 mem_sz);
	_u8 *get_base(_u32 *size);
	_u8 *alloc_buffer(_u32 sz);
	void release_buffer(_u8 *p, _u32 sz);

protected:
	iMutex	*m_p_mutex;
	_u8	*m_p_mem_base;
	_u32	m_size;
	_u8	m_bitmap[DMA_BITMAP_SIZE];
	_u16	m_used_pages;

	void set_bits(_u16 page, _u16 count, bool set);
};

IMPLEMENT_BASE(cDMAMemory, "cDMAMemory", 0x0001);
IMPLEMENT_INIT(cDMAMemory, prepository) {
	bool r = false;
	REPOSITORY = prepository;

	mem_set(m_bitmap, 0, sizeof(m_bitmap));	
	m_p_mutex = (iMutex *)OBJECT_BY_INTERFACE(I_MUTEX, RY_CLONE);
	if(m_p_mutex)
		r = true;

	return r;
}
IMPLEMENT_DESTROY(cDMAMemory) {
	if(m_p_mutex) {
		RELEASE_OBJECT(m_p_mutex);
		m_p_mutex = 0;
	}

	if(m_p_mem_base && m_size) {
		iPMA *p_pma = (iPMA *)OBJECT_BY_INTERFACE(I_PMA, RY_STANDALONE);
		if(p_pma) {
			_u32 npages = m_size / p_pma->get_page_size();
			p_pma->free(m_p_mem_base, npages);
			RELEASE_OBJECT(p_pma);
		}

		m_p_mem_base = 0;
		m_size = 0;
	}
}

void cDMAMemory::init(_u8 *mem_base, _u32 mem_sz) {
	if(m_p_mem_base == 0 && m_size == 0) {
		if(mem_sz > ((DMA_BITMAP_SIZE * 8) * DMA_PAGE_SIZE))
			m_size = (DMA_BITMAP_SIZE * 8) * DMA_PAGE_SIZE;
		else
			m_size = mem_sz;

		m_p_mem_base = mem_base;
	}
}

_u8 *cDMAMemory::get_base(_u32 *size) {
	*size = m_size;
	return m_p_mem_base;
}

void cDMAMemory::set_bits(_u16 page, _u16 count, bool set) {
	_u16 byte;
	_u8 bit=0;
	_u16 all_pages = (_u16)(m_size / DMA_PAGE_SIZE);
	_u16 _count = count;
	_u16 _page = page;

	while(_count) {
		if(_page < all_pages) {
			byte = _page / 8;
			bit = _page % 8;
			if(set) { // alloc page
				if(!(m_bitmap[byte] & (1<<bit)))
					m_used_pages++;
				m_bitmap[byte] |= (1<<bit);
			} else { // release page
				if(m_bitmap[byte] & (1<<bit))
					m_used_pages--;
				m_bitmap[byte] &= ~(1<<bit);
			}

			_page++;
			_count--;
		} else
			break;
	}
}

_u8 *cDMAMemory::alloc_buffer(_u32 sz) {
	_u8 *r = 0;
	_u16 all_pages = (_u16)(m_size / DMA_PAGE_SIZE);
	_u16 free_pages = all_pages - m_used_pages;
	_u16 npages = sz / DMA_PAGE_SIZE;
	npages += (sz % DMA_PAGE_SIZE) ? 1 : 0;
	HMUTEX hlock = m_p_mutex->lock();

	if(npages < free_pages) {
		_u16 fbit = 0xffff;
		_u16 count = 0;
		for(_u16 i = 0; i < all_pages && count < npages; i++) {
			if(m_bitmap[i] != 0xff) {
				_u8 mask = 0x80;
				for(_u8 j = 0; j < 8 && count < npages; j++) {
					if(!(m_bitmap[i] & mask)) {
						if(fbit == 0xffff)
							fbit = (i * 8) + j;
						count++;
					} else {
						fbit = 0xffff;
						count = 0;
					}

					mask >>= 1;
				}
			} else {
				count = 0;
				fbit = 0xffff;
			}
		}

		if(fbit != 0xffff && count == npages) {
			set_bits(fbit, count, true);
			r = m_p_mem_base + (fbit * DMA_PAGE_SIZE);
		}
	}

	m_p_mutex->unlock(hlock);
	return r;
}

void cDMAMemory::release_buffer(_u8 *p, _u32 sz) {
	if(p >= m_p_mem_base && p < (m_p_mem_base + m_size)) {
		if((p + sz) < (m_p_mem_base + m_size)) {
			_u16 page = (p - m_p_mem_base) / DMA_PAGE_SIZE;
			if(!(p - m_p_mem_base) % DMA_PAGE_SIZE) {
				HMUTEX hlock = m_p_mutex->lock();
				_u16 npages = sz / DMA_PAGE_SIZE;
				npages += (sz % DMA_PAGE_SIZE) ? 1 : 0;
				set_bits(page, npages, false);
				m_p_mutex->unlock(hlock);
			}
		}
	}
}

