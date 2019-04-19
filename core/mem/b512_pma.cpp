#include "iMemory.h"
#include "startup_context.h"
#include "str.h"
#include "iSystemLog.h"
#include "iScheduler.h"

//#define _DEBUG_
#include "debug.h"

#ifdef _DEBUG_
#define	LOG(_fmt_, ...) \
	DBG(_fmt_, __VA_ARGS__)
#else
#define LOG(_fmt_, ...) \
	if(m_p_syslog) \
		m_p_syslog->fwrite(_fmt_, __VA_ARGS__) 
#endif

#define MAX_BITMAP_BYTES	64
#define MAX_BITMAP_BITS		(MAX_BITMAP_BYTES * 8)
#define MAX_RAM_CHUNKS		20
#define PMA_PAGE_SIZE		4096

typedef struct {
	union {
		_u16 state;
		struct {
			_u16	rtype	:2;
			_u16	reserved:4;
			_u16	amount	:10;
		};
	};
	_u8 bitmap[MAX_BITMAP_BYTES];
}__attribute__((packed)) _bmap_t;

typedef struct {
	_ulong	addr;	// first page address
	_ulong	npages;	// size in pages
	_bmap_t	*p_bmp;	// pointer to bitmap
	_u32	nmaps;	// number of bitmaps
	_ulong	nfree;	// number of free pages
	_u32	spages; // number of reserved system pages
}_ram_chunk_t;

class cBPMA:public iPMA {
public:
	cBPMA() {
		m_p_mutex = 0;
		m_nmap = 0;
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	~cBPMA() {}

	// iBase
	DECLARE_BASE(cBPMA);
	DECLARE_EARLY_INIT();
	DECLARE_INIT();
	DECLARE_DESTROY();
	
	void info(_ram_info_t *p_info);
	
	// consequently allocation
	void *alloc(_u32 npages, _u8 type=RT_SYSTEM, _ulong limit=DEFAULT_PMA_LIMIT);
	
	// consequently free
	_u32 free(void *p, _u32 npages);
	
	_u16 get_page_size(void) {
		return m_page_size;
	}
	
	// inconsistent allocation
	_u32 alloc(_u32 npages, _u64 *page_array, _u8 type=RT_SYSTEM, _ulong limit=DEFAULT_PMA_LIMIT);
	
	// inconsistent free
	_u32 free(_u32 npages, _u64 *page_array);

	// allocation by address
	_u32 alloc(void *p, _u32 npages, _u8 type=RT_SYSTEM);
	_u8 get_type(void *p);

	_ulong virtual_to_physical(_ulong vaddr) {
		return vaddr - m_vbase;
	}
	_ulong physical_to_virtual(_ulong paddr) {
		return m_vbase + paddr;
	}
	
protected:
	iSystemLog	*m_p_syslog;
	_ulong		m_vbase;
	iMutex		*m_p_mutex;
	_u16		m_page_size;
	_ram_chunk_t	m_rmap[MAX_RAM_CHUNKS];
	_u16		m_nmap;

	void init_ram_chunk(_ulong addr, // ram address
			    _ulong size // ram size in bytes
			   );
	bool init(_ram_part_t *p_ram_array, // pointer to ram parts array
		  _u16 nparts, // number of ram parts
		  _u16 page_size // page size in bytes
		 );

	_u32 alloc(_ram_chunk_t *p_rc, _u32 npages, _u64 *page_array, _u8 type, _ulong limit);
	_u32 alloc(_ulong start_addr, _bmap_t *p_bmp, _u32 npages, _u64 *page_array, _u8 type);
	
	HMUTEX lock(HMUTEX hlock=0);
	void unlock(HMUTEX hlock);
	iMutex *get_mutex(void);
	bool calc_bit_position(_ulong addr, // in
				_ram_chunk_t **p_rc, // out
				_bmap_t **p_bmap, // out
				_u16 *p_byte, //out
				_u8  *p_bit // out
				);
	_ulong calc_addr_by_bit_position(_ram_chunk_t *p_rc,
					 _bmap_t *p_bmp,
					 _u8  bit
					);
	void test_bitmap(_bmap_t *p_bmp, // in
			 _u32 *fab, // out (free at begin)
			 _u32 *fae // out (free at end)
			);
	_ulong find(_ulong limit, // in
		  _u32 count, //in
		  _u8 type, // in
		  _ram_chunk_t **pp_rc, // out
		  _bmap_t **pp_bmp, // out
		  _u32 *sbit // out
		 );
	_u32 set_bits(_bmap_t *p_bmp, _u32 sb, _u32 eb, bool set, bool force=false);
	_u32 seq_set_bits(_ram_chunk_t *p_rc, _bmap_t *p_bmp, _u32 sbit, _u32 count, _u8 type, bool set);
	void dump(void);
};

IMPLEMENT_BASE(cBPMA,"cBPMA",0x0002);

extern _startup_context_t *__g_p_startup_context__;

IMPLEMENT_EARLY_INIT(cBPMA) { // called by object registration
	m_vbase = (_ulong)__g_p_startup_context__->vbase;

	_ram_part_t ram[MAX_RAM_CHUNKS];
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
	
iMutex *cBPMA::get_mutex(void) {
	iMutex *r = m_p_mutex;
	if(!r) {
		if(REPOSITORY)
			r = m_p_mutex = (iMutex *)OBJECT_BY_INTERFACE(I_MUTEX, RY_CLONE);
	}
	return r;
}

IMPLEMENT_INIT(cBPMA, p_rb) {
	bool r = false;

	REPOSITORY = p_rb; // keep repository pointer
	
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	get_mutex();
	if(m_p_mutex)
		r = true;
	
	return r;
}
IMPLEMENT_DESTROY(cBPMA) {
}

HMUTEX cBPMA::lock(HMUTEX hlock) {
	HMUTEX r = 0;

	iMutex *p_mutex = get_mutex();
	if(p_mutex) {
		r = acquire_mutex(p_mutex, hlock);
	}

	return r;
}

void cBPMA::unlock(HMUTEX hlock) {
	iMutex *p_mutex = get_mutex();
	if(p_mutex)
		p_mutex->unlock(hlock);
}

void cBPMA::init_ram_chunk(_ulong addr, // ram address
			   _ulong size // ram size in bytes
			  ) {
	// alignment the memory to be divisible by page size
	if(size >= m_page_size * 2) {
		DBG("b512_pma: init chunk 0x%h --- %d --- 0x%h\r\n", addr, size/m_page_size, addr+size);
		_u32 rem = m_page_size - (addr % m_page_size);
		_ulong _addr = addr + rem;
		_ulong _size = size - rem;

		_ulong npages = _size / m_page_size;
		_ulong nbitmaps = npages / MAX_BITMAP_BITS;
		//nbitmaps += (npages % MAX_BITMAP_BITS)?1:0;
		_ulong bitmap_pages = (nbitmaps * sizeof(_ram_chunk_t)) / m_page_size;
		bitmap_pages += ((nbitmaps * sizeof(_ram_chunk_t)) % m_page_size) ? 1 : 0;

		for(_ulong i = 0; i < bitmap_pages; i++)
			mem_set((_u8 *)_addr+(i*m_page_size), 0, m_page_size);

		m_rmap[m_nmap].p_bmp = (_bmap_t *)_addr;

		_addr += bitmap_pages * m_page_size;
		npages -= bitmap_pages;

		//set_bits(m_rmap[m_nmap].p_bmp + (nbitmaps - 1), npages % MAX_BITMAP_BITS, MAX_BITMAP_BITS -1, true);

		m_rmap[m_nmap].addr = _addr;
		m_rmap[m_nmap].npages = npages;
		m_rmap[m_nmap].nmaps = nbitmaps;
		m_rmap[m_nmap].nfree = npages;
		m_rmap[m_nmap].spages = bitmap_pages;

		m_nmap++;
	}
}

bool cBPMA::init(_ram_part_t *p_ram_array, // pointer to ram parts array
		_u16 nparts, // number of ram parts
		_u16 page_size // page size in bytes
		) {
	bool r = true;

	m_nmap = 0;
	m_page_size = page_size;

	mem_set((_u8 *)m_rmap, 0, sizeof(m_rmap));
	
	for(_u16 i = 0; i < nparts; i++)
		init_ram_chunk(p_ram_array[i].addr, p_ram_array[i].size);

	return r;
}

_u8 cBPMA::get_type(void *p) {
	_u8 r = 0;

	_ram_chunk_t *p_rc;
	_bmap_t *p_bmp;
	_u16 byte;
	_u8 bit;

	if(calc_bit_position((_ulong)p, &p_rc, &p_bmp, &byte, &bit))
		r = p_bmp->rtype;

	return r;
}

void cBPMA::info(_ram_info_t *p_info) {
	p_info->ps = m_page_size;
	p_info->rp = p_info->pu = p_info->sp = 0;
	_ulong nfree = 0;
	for(_u32 i = 0; i < m_nmap; i++) {
		p_info->rp += m_rmap[i].npages;
		nfree += m_rmap[i].nfree;
		p_info->sp += m_rmap[i].spages;
	}
	p_info->pu += p_info->rp - nfree;
}

_u32 cBPMA::set_bits(_bmap_t *p_bmp, _u32 sb, _u32 eb, bool set, bool force) {
	_u32 r = 0;
	
	if(sb < MAX_BITMAP_BITS && eb < MAX_BITMAP_BITS) {
		_u32 cb = sb;

		while(cb <= eb) {
			_u32 byte = cb >> 3;
			_u8 bit = cb % 8;

			if(!bit && (eb - cb) >= 8) {
				if(set) {
					if(!p_bmp->bitmap[byte]) {
						p_bmp->bitmap[byte] = 0xff;
						p_bmp->amount += 8;
						r += 8;
					} else
						goto _set_per_bit_;
				} else {
					if(p_bmp->bitmap[byte] == 0xff) {
						p_bmp->bitmap[byte] = 0;
						p_bmp->amount -= 8;
						r += 8;
					} else
						goto _set_per_bit_;
				}
				
				cb += 8;
			} else {
_set_per_bit_:
				_u8 mask = (0x80 >> bit);

				if(set) {
					if(!(p_bmp->bitmap[byte] & mask)) {
						p_bmp->bitmap[byte] |= mask;
						p_bmp->amount++;
						r++;
					}
				} else {
					if(p_bmp->bitmap[byte] & mask) {
						p_bmp->bitmap[byte] &= ~mask;
						p_bmp->amount--;
						r++;
					}
				}

				cb++;
			}
		}

		if(force)
			r = cb;
	}
	
	return r;
}

void cBPMA::test_bitmap(_bmap_t *p_bmp, // in
			_u32 *fab, // out (free at begin)
			_u32 *fae // out (free at end)
			) {
	if(p_bmp->amount == 0)
		*fab = *fae = MAX_BITMAP_BITS;
	else {
		*fab = *fae = 0;
		if(p_bmp->amount < MAX_BITMAP_BITS) {
			for(_u32 i = 0; i < MAX_BITMAP_BYTES; i++) {
				if(p_bmp->bitmap[i] == 0)
					*fab += 8;
				else {
					_u8 mask = 0x80;
					while(mask) {
						if(!(p_bmp->bitmap[i] & mask))
							*fab += 1;
						else {
							i = MAX_BITMAP_BYTES;
							break;
						}
						mask >>= 1;
					}
				}
			}

			for(_s32 i = MAX_BITMAP_BYTES - 1; i >= 0 ; i--) {
				if(p_bmp->bitmap[i] == 0)
					*fae += 8;
				else {
					_u8 mask = 0x01;
					while(mask) {
						if(!(p_bmp->bitmap[i] & mask))
							*fae += 1;
						else {
							i = 0xffffffff;
							break;
						}
						mask <<= 1;
					}
				}
			}
		}
	}
}

bool cBPMA::calc_bit_position(_ulong addr, // in
				_ram_chunk_t **p_rc, // out
				_bmap_t **p_bmap, // out
				_u16 *p_byte, //out
				_u8  *p_bit // out
			     ) {
	bool r = false;

	for(_u32 i = 0; i < m_nmap; i++) {
		if(addr >= m_rmap[i].addr && addr < (m_rmap[i].addr + (m_rmap[i].npages * m_page_size))) {
			_ulong ch_addr = addr - m_rmap[i].addr;
			_ulong page = (ch_addr / m_page_size);

			*p_rc = &m_rmap[i];
			_ulong nbyte = page / 8;
			*p_bit = page % 8;
			_u32 nbmap = nbyte / MAX_BITMAP_BYTES;
			*p_bmap = m_rmap[i].p_bmp + nbmap;
			*p_byte = nbyte % MAX_BITMAP_BYTES;
			r = true;
			break;
		}
	}

	return r;
}

_ulong cBPMA::calc_addr_by_bit_position(_ram_chunk_t *p_rc,
					_bmap_t *p_bmp,
					_u8  bit
					) {
	_ulong r = 0;
	for(_u32 i = 0; i < p_rc->nmaps; i++) {
		if(p_rc->p_bmp == p_bmp) {
			_ulong base = p_rc->addr + ((i * MAX_BITMAP_BITS) * m_page_size);
			r = base + (bit * m_page_size);
			break;
		}
	}
	return r;
}

_ulong cBPMA::find(_ulong limit, // in
	  	 _u32 count, //in
		 _u8 type, // in
		 _ram_chunk_t **pp_rc, // out
		 _bmap_t **pp_bmp, // out
		 _u32 *sbit // out
		) {
	_ulong r = 0;
	_ram_chunk_t *p_rc=0;
	_bmap_t *p_bmp=0;
	_u16 byte=MAX_BITMAP_BYTES-1;

	_s32 rcidx = m_nmap-1;
	while(rcidx >= 0) {
		p_rc = &m_rmap[rcidx];
		if(p_rc->addr < limit && p_rc->nfree >= count) {
			_u32 cnt = count;
			while(cnt) {
				_s32 bmpidx = p_rc->nmaps-1;
				while(bmpidx >= 0) {
					p_bmp = &p_rc->p_bmp[bmpidx];
					_ulong bitmap_end = p_rc->addr+((bmpidx*MAX_BITMAP_BITS)*m_page_size);
					bitmap_end += MAX_BITMAP_BITS * m_page_size;
					if(bitmap_end <= limit && 
							(p_bmp->rtype == type || p_bmp->rtype == 0) && 
							p_bmp->amount < MAX_BITMAP_BITS) {
						byte = MAX_BITMAP_BYTES - 1;
_find_l1_:						
						_u8 mask = 1;
						while(mask) {
							if(p_bmp->bitmap[byte] == 0xff) {
								if(byte)
									byte--;
								else
									break;
								cnt = count;
								goto _find_l1_;
							} else {
								if(p_bmp->bitmap[byte] & mask) {
									cnt = count;
								} else {
									cnt--;
									if(!cnt) {
										// done
										*pp_rc = p_rc;
										*pp_bmp = p_bmp;
										*sbit = (byte * 8);
										if(mask) {
											_u8 _mask = 0x80;
											while(!(_mask & mask)) {
												*sbit += 1;
												_mask >>= 1;
											}
										}
										r = p_rc->addr + 
											(((bmpidx * MAX_BITMAP_BITS) + *sbit) * m_page_size);
										return r;
									}
								}
							}
							mask <<= 1;
						}
						if(byte) {
							byte--;
							goto _find_l1_;
						}
					} else
						cnt = count;
					bmpidx--;
				}
				if(bmpidx < 0)
					break;
			}
		}

		rcidx--;
	}

	return r;
}

_u32 cBPMA::seq_set_bits(_ram_chunk_t *p_rc, _bmap_t *p_bmap, _u32 sbit, _u32 count, _u8 type, bool set) {
	_u32 r = 0;
	_bmap_t *p_bmp = p_bmap;
	_u32 sb = sbit;

	while(r < count) {
		_u32 eb = sb + (count - r);
		if(eb > MAX_BITMAP_BITS)
			eb = MAX_BITMAP_BITS;
		r += set_bits(p_bmp, sb, eb-1, set, false);
		sb = 0;
		if(set) {
			if(p_bmp->rtype == 0)
				p_bmp->rtype = type;
		} else {
			if(p_bmp->amount == 0)
				p_bmp->rtype = 0;
		}

		p_bmp++;
	}

	if(set)
		p_rc->nfree -= r;
	else
		p_rc->nfree += r;

	return r;
}

// consequently allocation
void *cBPMA::alloc(_u32 npages, _u8 type, _ulong limit) {
	void *r = 0;

	/*if(npages == 1)
		alloc(npages, (_u64 *)&r, type, limit);
	else {*/
		HMUTEX hlock = lock();
		
		_ram_chunk_t *p_rc = 0;
		_bmap_t *p_bmp = 0;
		_u32 sbit = 0;
		_ulong addr = find(limit, npages, type, &p_rc, &p_bmp, &sbit);
		if(addr) {
			seq_set_bits(p_rc, p_bmp, sbit, npages, type, true);
			r = (void *)addr;
		}
		
/*
		for(_u32 i = 0; i < m_nmap; i++) {
			if(m_rmap[i].nfree >= npages) {
				_u32 pages = 0;
				_u32 fbit = 0xffffffff;
				_u32 nfbmp = 0xffffffff;

				for(_u32 j = 0; j < m_rmap[i].nmaps && pages < npages; j++) {
					_bmap_t *p_bmp = m_rmap[i].p_bmp + j;
					
					if(p_bmp->rtype == 0 || p_bmp->rtype == type) {
						_u32 fab=0, fae=0;

						test_bitmap(p_bmp, &fab, &fae);
						if((pages + fab) >= npages) {
							pages += fab;
							if(nfbmp == 0xffffffff) {
								fbit = 0;
								nfbmp = j;
							}
						} else { // continue with the end of bitmap
							if(fae) {
								if(nfbmp == 0xffffffff) {
_mem_alloc_pos_:
									fbit = MAX_BITMAP_BITS - fae;
									pages = fae;
									nfbmp = j;
								} else {
									if(fae == MAX_BITMAP_BITS)
										pages += fae;
									else
										goto _mem_alloc_pos_;
								}
							} else {
								nfbmp = fbit = 0xffffffff;
								pages = 0;
							}
						}
					} else { // not the same type of memory
						nfbmp = fbit = 0xffffffff;
						pages = 0;
					}
				}

				if(pages >= npages && nfbmp != 0xffffffff && fbit != 0xffffffff) {
					seq_set_bits(&m_rmap[i],  m_rmap[i].p_bmp + nfbmp, fbit, npages, type, true);
					_ulong base_addr = m_rmap[i].addr + ((nfbmp * MAX_BITMAP_BITS) * m_page_size);
					r = (void *)(base_addr + (fbit * m_page_size));
					break;
				}
			}
		}
*/
		unlock(hlock);
	//}

	return r;
}

// consequently free
_u32 cBPMA::free(void *p, _u32 npages) {
	_u32 r = 0;

	HMUTEX hlock = lock();

	_ram_chunk_t *p_rc;
	_bmap_t *p_bmp;
	_u16 byte;
	_u8 bit;

	if(calc_bit_position((_ulong)p, &p_rc, &p_bmp, &byte, &bit)) {
		_u32 sb = (byte * 8) + bit;
		r = seq_set_bits(p_rc, p_bmp, sb, npages, 0, false);
	}

	unlock(hlock);

	return r;
}
	
_u32 cBPMA::alloc(_ulong start_addr, _bmap_t *p_bmap, _u32 npages, _u64 *page_array, _u8 type) {
	_u32 r = 0;

	if(p_bmap->rtype == type || p_bmap->rtype == 0) {
		if(p_bmap->amount < MAX_BITMAP_BITS) {
			for(_u32 i = 0; i < MAX_BITMAP_BYTES && r < npages && (p_bmap->amount+r) < MAX_BITMAP_BITS; i++) {
				if(p_bmap->bitmap[i] != 0xff) {
					_u8 mask = 0x80;
					for(_u32 j = 0; j < 8 && r < npages && (p_bmap->amount+r) < MAX_BITMAP_BITS; j++) {
						if(!(p_bmap->bitmap[i] & mask)) {
							*(page_array + r) = start_addr + (((i * 8) + j) * m_page_size);
							p_bmap->bitmap[i] |= mask;
							r++;
						}

						mask >>= 1;
					}
				}
			}
			
			if(p_bmap->rtype == 0)
				p_bmap->rtype = type;

			p_bmap->amount += r;
		}
	}

	return r;
}

_u32 cBPMA::alloc(_ram_chunk_t *p_rc, _u32 npages, _u64 *page_array, _u8 type, _ulong limit) {
	_u32 r = 0;

	for(_u32 i = 0; i < p_rc->nmaps && r < npages; i++)
		r += alloc(p_rc->addr + ((i * MAX_BITMAP_BITS) * m_page_size), 
			p_rc->p_bmp + i, npages - r, page_array + r, type);

	p_rc->nfree -= r;
	return r;
}

// inconsistent allocation
_u32 cBPMA::alloc(_u32 npages, _u64 *page_array, _u8 type, _ulong limit) {
	_u32 r = 0;

	HMUTEX hlock = lock();

	for(_u16 i = 0; i < m_nmap && r < npages; i++) {
		if(m_rmap[i].nfree)
			r += alloc(&m_rmap[i], npages - r, page_array + r, type, limit);
	}

	unlock(hlock);

	return r;
}

// inconsistent free
_u32 cBPMA::free(_u32 npages, _u64 *page_array) {
	_u32 r = 0;

	HMUTEX hlock = lock();
	
	for(_u32 i = 0; i < npages; i++) {
		_ram_chunk_t *p_rc;
		_bmap_t *p_bmap;
		_u16 byte=0;
		_u8 bit=0;

		if(calc_bit_position(*(page_array + i), &p_rc, &p_bmap, &byte, &bit)) {
			_u8 mask = 0x80 >> bit;
			if(p_bmap->bitmap[byte] & mask) {
				p_bmap->bitmap[byte] &= ~mask;
				p_bmap->amount--;
				p_rc->nfree++;
				r++;
			}
		}
	}

	unlock(hlock);
	
	return r;
}

// allocation by address
_u32 cBPMA::alloc(void *p, _u32 npages, _u8 type) {
	_u32 r = 0;

	HMUTEX hlock = lock();

	_ram_chunk_t *p_rc;
	_bmap_t *p_bmp;
	_u16 byte;
	_u8 bit;

	if(calc_bit_position((_ulong)p, &p_rc, &p_bmp, &byte, &bit)) {
		_u32 sb = (byte * 8) + bit;
		r = seq_set_bits(p_rc, p_bmp, sb, npages, type, false);
	}

	unlock(hlock);

	return r;
}
void cBPMA::dump(void) {
}

