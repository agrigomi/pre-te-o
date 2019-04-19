#include "iMemory.h"
#include "startup_context.h"
#include "str.h"

// Physical Memory Allocator (header)



#define MAX_RAM_PARTS	24
#define PMA_PAGE_SIZE	4096

typedef struct {
	_u8	*p_bitmap; // bitmap address
	_u64	bsize;	// bitmap size in pages (bits)
	_u64	ram_addr; // point the fiest byte of ram area
	_u64	ram_size; // ram size in bytes
	_u64	npu; // number of pages in use
}_bmp_ram_part_t;

class cBPMA:public iPMA {
public:
	cBPMA() {
		m_nparts = 0;
		m_p_mutex = 0;
		ERROR_CODE = 0;
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	
	virtual ~cBPMA() {}
	
	// iBase
	DECLARE_BASE(cBPMA);
	DECLARE_EARLY_INIT();
	DECLARE_INIT();
	DECLARE_DESTROY();
	
	void info(_ram_info_t *p_info);
	
	// consequently allocation
	void *alloc(_u32 npages, _u8 type=RT_SYSTEM);
	
	// consequently free
	_u32 free(void *p, _u32 npages);
	
	_u16 get_page_size(void) {
		return m_page_size;
	}
	
	// inconsistent allocation
	_u32 alloc(_u32 npages, _u64 *page_array, _u8 type=RT_SYSTEM);
	
	// inconsistent free
	_u32 free(_u32 npages, _u64 *page_array);

	// allocation by address
	_u32 alloc(void *p, _u32 npages, _u8 type=RT_SYSTEM);
	_u8 get_type(void _UNUSED_ *p) {
		return 0;
	}

	_ulong virtual_to_physical(_ulong vaddr) {
		return vaddr - m_vbase;
	}
	_ulong physical_to_virtual(_ulong paddr) {
		return m_vbase + paddr;
	}
	
protected:
	_u16		m_err;
	iMutex		*m_p_mutex;
	_bmp_ram_part_t	m_ram[MAX_RAM_PARTS];
	_u8		m_nparts;
	_u16		m_page_size;
	_ulong		m_vbase;
	
	bool init(_ram_part_t *p_ram_array, // pointer to ram parts array
		  _u16 nparts, // number of ram parts
		  _u16 page_size // page size in bytes
		 );
	
	HMUTEX lock(HMUTEX hlock=0);
	void unlock(HMUTEX hlock);

	_u32 bit_set(void *p, _u32 count, bool set);
};

// Physical Memory Allocator (implementation)
IMPLEMENT_BASE(cBPMA,"cBitmapPMA",0x0001);

// declared in loader.cpp
extern _startup_context_t *__g_p_startup_context__;

IMPLEMENT_EARLY_INIT(cBPMA) { // called by object registration
	m_vbase = (_ulong)__g_p_startup_context__->vbase;

	_ram_part_t ram[MAX_RAM_PARTS];
	_u32 ram_idx = 0;
	
	for(_u32 i = 0; i < __g_p_startup_context__->mm_cnt; i++) {
		if(__g_p_startup_context__->mmap[i].type == MEM_TYPE_FREE) {
			ram[ram_idx].addr = __g_p_startup_context__->mmap[i].address;
			ram[ram_idx].size = __g_p_startup_context__->mmap[i].size;

			for(_u32 j = 0; j < __g_p_startup_context__->rm_cnt; j++) {
				_mem_tag_t *p_rt = &__g_p_startup_context__->rmap[j];
				if(ram[ram_idx].addr == p_rt->address && p_rt->size <= ram[ram_idx].size) {
					ram[ram_idx].addr += p_rt->size;
					ram[ram_idx].size -= p_rt->size;
				}
			}

			ram[ram_idx].addr += m_vbase;

			ram_idx++;
		}
	}
	
	init(ram, ram_idx, PMA_PAGE_SIZE);
}

IMPLEMENT_INIT(cBPMA, p_rb) {
	bool r = false;

	REPOSITORY = p_rb; // keep repository pointer
	
	m_p_mutex = (iMutex *)OBJECT_BY_INTERFACE(I_MUTEX, RY_CLONE);
	if(m_p_mutex)
		r = true;
	
	return r;
}

IMPLEMENT_DESTROY(cBPMA) {
	RELEASE_OBJECT(m_p_mutex);
}

HMUTEX cBPMA::lock(HMUTEX hlock) {
	HMUTEX r = 0;
	
	if(m_p_mutex)
		r = m_p_mutex->lock(hlock);
	
	return r;
}

void cBPMA::unlock(HMUTEX hlock) {
	if(m_p_mutex)
		m_p_mutex->unlock(hlock);
}

bool cBPMA::init(_ram_part_t *p_ram_array, // pointer to ram parts array
		_u16 nparts, // number of ram parts
		_u16 page_size // page size in bytes
		) {
	bool r = false;
	
	mem_set((_u8 *)m_ram, 0, sizeof(m_ram));
	m_nparts = 0;

	// calculating the bitmap size in bytes
	// for all pages of address space
	_u64 bitmap_sz = 0;
	for(_u16 i = 0; i < nparts && i < MAX_RAM_PARTS; i++) {
		// number of pages in this part
		_u64 npages = p_ram_array[i].size / page_size;
		
		// bitmap size in bytes calculated for all area
		_u64 bsize = npages / 8;
		bsize += (npages % 8) ? 1 : 0;
		
		bitmap_sz += bsize;
	}
	
	// calculating the number of pages for bitmap area
	_u64 bitmap_pages = bitmap_sz / page_size;
	bitmap_pages += (bitmap_sz % page_size) ? 1 : 0;
	
	// exclude pages reserved by bitmap
	bitmap_sz -= bitmap_pages / 8;
			
	// searching for memory place to fit bitmap
	_u8 *bitmap_addr = 0;
	for(_u16 i = 0; i < nparts && i < MAX_RAM_PARTS; i++) {
		if(p_ram_array[i].size > bitmap_sz) {
			// manipulate this part to protect bitmap area
			bitmap_addr = (_u8 *)p_ram_array[i].addr;
			p_ram_array[i].size -= bitmap_sz;
			p_ram_array[i].addr += bitmap_sz;
			mem_set(bitmap_addr, 0, bitmap_sz);
			break;
		}
	}

	_u8 *p_bitmap = bitmap_addr;
	for(_u16 i = 0; i < nparts && i < MAX_RAM_PARTS; i++) {
		_u32 rem = p_ram_array[i].addr % page_size;
		
		if(rem) {
			// alignment of addresses so that they are 
			// multiples of the page size
			m_ram[i].ram_addr = p_ram_array[i].addr + (page_size - rem);
			m_ram[i].ram_size = p_ram_array[i].size - (page_size - rem);
		} else {
			m_ram[i].ram_addr = p_ram_array[i].addr;
			m_ram[i].ram_size = p_ram_array[i].size;
		}
		
		m_ram[i].p_bitmap = p_bitmap;
		m_ram[i].npu = 0;
		
		// number of pages in this part
		_u64 npages = m_ram[i].ram_size / page_size;
		// assign bitmap size in pages (bits)
		m_ram[i].bsize = npages;

		// bitmap size in bytes
		_u64 bsize = npages / 8;
		//bsize += (npages % 8) ? 1 : 0;
		p_bitmap += bsize;
	}
	
	m_page_size = page_size;
	m_nparts = nparts;
	r = true;
	
	return r;
}

void cBPMA::info(_ram_info_t *p_info) {
	p_info->ps = m_page_size;
	p_info->rp = 0;
	p_info->pu = 0;
	p_info->sp = 0;
	
	for(_u8 i = 0; i < m_nparts; i++) {
		p_info->rp += m_ram[i].ram_size / m_page_size;
		p_info->pu += m_ram[i].npu;
		p_info->sp += (m_ram[i].bsize / 8) / m_page_size;
	}
}

// consequently allocation
void *cBPMA::alloc(_u32 npages, _u8 _UNUSED_ type) {
	void *r = 0;

	HMUTEX hlock = lock();

	for(_u16 i = 0; i < m_nparts; i++) {
		if((m_ram[i].bsize - m_ram[i].npu) >= npages) {
			_u8 *pb = m_ram[i].p_bitmap;
			_u32 fbit = 0xffffffff;
			_u32 nbytes = m_ram[i].bsize / 8;
			_u32 byte = 0;
			_u32 bit = 0;
			_u32 page = 0;
			nbytes += (m_ram[i].bsize % 8) ? 1 : 0;

			for(; byte < nbytes && bit < m_ram[i].bsize && page < npages; byte++) {
				if(pb[byte] == 0xff) {
					bit += 8;
					page = 0;
					fbit = 0xffffffff;
				} else {
					if(pb[byte] == 0 && (page+8) <= npages) {
						if(fbit == 0xffffffff)
							fbit = bit;
						page += 8;
						bit += 8;
					} else {
						_u8 mask = 0x80;
						while(mask && page < npages && bit < m_ram[i].bsize) {
							if(pb[byte] & mask) {
								fbit = 0xffffffff;
								page = 0;
							} else {
								if(fbit == 0xffffffff)
									fbit = bit;
								page++;
							}
							mask >>= 1;
							bit++;
						}
					}
				}
			}

			if(fbit != 0xffffffff && page >= npages) {
				void *addr = (void *)(m_ram[i].ram_addr + (fbit * m_page_size));
				if(bit_set(addr, npages, true) == npages) {
					r = addr;
					break;
				} else
					bit_set(addr, npages, false);
			}
		}
	}
	
	unlock(hlock);

	return r;
}

_u32 cBPMA::alloc(_u32 npages, _u64 *page_array, _u8 _UNUSED_ type) {
	_u32 r = 0;
	
	HMUTEX hlock = lock();
	
	_u32 fp[MAX_RAM_PARTS];
	_u8 part=0;
	
	for(part = 0; part < m_nparts; part++)
		fp[part] = m_ram[part].bsize - m_ram[part].npu;
	
	// number of free pages
	
	_u32 nfree_pages = 0;
	for(_u8 i = 0; i < part; i++)
		nfree_pages += fp[i];

	if(nfree_pages >= npages) { // the available space is enough
		_u32 array_idx = 0;
		
		for(_u8 i = 0; i < part; i++) {
			if(fp[i]) {
				_u32 nbytes = m_ram[i].bsize / 8;
				nbytes += (m_ram[i].bsize % 8) ? 1 : 0;
				
				_u32 bit = 0;
				_u8 *pb = m_ram[i].p_bitmap;
				
				for(_u32 j = 0; j < nbytes; j++) {
					if(*(pb + j) == 0xff) {
						bit += 8;
						if(bit >= m_ram[i].bsize)
							break;
					} else {
						_u8 mask = 0x80; // bit mask
						
						while(mask) {
							if(!(*(pb + j) & mask)) {// a one empty page has found
								// calculate the physical address of page
								*(page_array + array_idx) = 
										(_u64)(m_ram[i].ram_addr + (bit * m_page_size));
								
								// allocate it
								*(pb + j) |= mask;
								m_ram[i].npu++;

								array_idx++;
								if(array_idx >= npages) { // ENOUGH
									goto _alloc_done_;
								}
							}
							
							mask >>= 1;
							bit++;
							if(bit >= m_ram[i].bsize) { // no more pages in this part
								j = nbytes; // terminate the parent loop
								break;
							}
						}
					}
				}
			}
		}
_alloc_done_:		
		r = array_idx;
	}
		
	unlock(hlock);
	
	return r;
}

_u32 cBPMA::bit_set(void *p, _u32 count, bool set) {
	_u32 r = 0;
	for(_u16 i = 0; i < m_nparts; i++) {
		if((_u64)p >= m_ram[i].ram_addr && (_u64)p < (m_ram[i].ram_addr + m_ram[i].ram_size)) {
			_u32 bit = ((_u64)p - m_ram[i].ram_addr) / m_page_size;
			_u32 offset = bit / 8; // offset in bitmap
			_u8 boffset = bit % 8; // offset in byte

			while(r < count) {
				if((r + 8) < count && boffset == 0) {
					if(set)
						*(m_ram[i].p_bitmap + offset) = 0xff;
					else
						*(m_ram[i].p_bitmap + offset) = 0;

					r += 8;
				} else {
					_u8 mask = 0x80 >> boffset;
					while(mask && (r < count)) {
						if(set)
							*(m_ram[i].p_bitmap + offset) |= mask;
						else
							*(m_ram[i].p_bitmap + offset) &= ~mask;
						mask >>= 1;
						r++;
					}
				}
				
				offset++;
				boffset = 0;
			}
			
			if(set)
				m_ram[i].npu += r;
			else
				m_ram[i].npu -= r;
			
			break;
		}
	}
	return r;
}

// consequently free
_u32 cBPMA::free(void *p, _u32 npages) {
	HMUTEX hlock = lock();
	_u32 r = bit_set(p, npages, false);
	unlock(hlock);
	return r;
}

_u32 cBPMA::free(_u32 npages, _u64 *page_array) {
	_u32 r = 0;
	
	while(r < npages) {
		_u32 seq = 1;
		
		for(_u32 i = r; i < npages; i++) {
			if((i + 1) < npages) {
				if((page_array[i+1] - page_array[i]) != m_page_size)
					break;
				
				seq++;
			} else
				break;
		}
		
		_u32 _r = free((void *)page_array[r], seq);
		
		if(_r)
			r += _r;
		else
			break;
	}
	
	return r;
}

_u32 cBPMA::alloc(void *p, _u32 npages, _u8 _UNUSED_ type) {
	HMUTEX hlock = lock();
	_u32 r = bit_set(p, npages, true);
	unlock(hlock);
	return r;
}

