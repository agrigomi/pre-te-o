#include "iMemory.h"
#include "startup_context.h"
#include "iScheduler.h"
#include "str.h"
#include "iSystemLog.h"

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

#define MAX_REC_AMOUNT	24
#define PMA_PAGE_SIZE	4096
#define MAX_RAM_BLOCKS	20

typedef union {
	_u32	rec;
	struct __attribute__((packed)) {
		_u32	bmp	:MAX_REC_AMOUNT;
		_u32	amount	:5;
		_u32	type	:3;
	};
}__attribute__((packed)) _rec_t;

typedef struct {
	_ulong	addr; // address of first page
	_ulong	nrec; // number of records
	_rec_t	*p_rec; // pointer to first record
	_ulong	nfree; // number of free pages
}_ram_t;

class cB24PMA:public iPMA {
public:
	cB24PMA() {
		m_p_mutex = 0;
		m_nram = 0;
		m_page_size = PMA_PAGE_SIZE;
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	~cB24PMA() {}

	DECLARE_BASE(cB24PMA);
	DECLARE_EARLY_INIT();
	DECLARE_INIT();

	void info(_ram_info_t *p_info);
	// consequently alloc
	void *alloc(_u32 npages, _u8 type=RT_SYSTEM, _ulong limit=DEFAULT_PMA_LIMIT);
	// consequently free
	_u32 free(void *p, _u32 npages);
	// inconsistent alloc
	_u32 alloc(/* in */_u32 npages, // number of pages to alloc
		   /* out */_u64 *page_array, // array of page addresses in PM
		  /* in */_u8 type=RT_SYSTEM,
		  /* in */ _ulong limit=DEFAULT_PMA_LIMIT
		  );
	// inconsistent free
	_u32 free(/* in */_u32 npages, // number pages to release
		  /* in */_u64 *page_array // array of page addresses in PM
		 );
	_u32 alloc(void *p, _u32 npages, _u8 type=RT_SYSTEM);
	_u8 get_type(void *p);
	_u16 get_page_size(void) {
		return m_page_size;
	}

	_ulong virtual_to_physical(_ulong vaddr) {
		return vaddr - m_vbase;
	}
	_ulong physical_to_virtual(_ulong paddr) {
		return m_vbase + paddr;
	}
	void dump(void);

protected:
	iSystemLog	*m_p_syslog;
	_ulong		m_vbase;
	iMutex		*m_p_mutex;
	_u16		m_page_size;
	_ram_t		m_ram[MAX_RAM_BLOCKS];
	_u16		m_nram;
	
	HMUTEX lock(HMUTEX hlock=0);
	void unlock(HMUTEX hlock);
	iMutex *get_mutex(void);
	bool init(_ram_part_t *p_ram_array, // pointer to ram parts array
		  _u16 nparts, // number of ram parts
		  _u16 page_size // page size in bytes
		 );
	void init_ram_block(_ulong addr, _ulong size);
	bool address_to_position(_ulong addr, // in
					_ram_t **p_block, // out
					_s16	*block_idx, // out
					_rec_t **p_rec, // out
					_ulong	*rec_idx, // out
					_u8     *p_bit // out
				);
	_ulong position_to_address(_ram_t *p_block, _rec_t *p_rec, _u8 bit);
	_ulong position_to_address(_s16 block_idx, _ulong rec_idx, _u8 bit);
	_u32 set_bits(_ram_t *p_block, _rec_t *p_rec, _u8 bit, _u32 count, _u8 type, bool set);
	bool find(_ulong limit, _u8 type, _u32 count, // in
			_ram_t **p_block, _s16 *block_idx, //out
			_rec_t ** p_rec, _ulong *rec_idx, _u8 *p_bit // out
		 );
};

IMPLEMENT_BASE(cB24PMA,"cB42PMA",0x0003);

extern _startup_context_t *__g_p_startup_context__;

IMPLEMENT_EARLY_INIT(cB24PMA) { // called by object registration
	m_vbase = (_ulong)__g_p_startup_context__->vbase;

	_ram_part_t ram[MAX_RAM_BLOCKS];
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

iMutex *cB24PMA::get_mutex(void) {
	iMutex *r = m_p_mutex;
	if(!r) {
		if(REPOSITORY)
			r = m_p_mutex = (iMutex *)OBJECT_BY_INTERFACE(I_MUTEX, RY_CLONE);
	}
	return r;
}

IMPLEMENT_INIT(cB24PMA, p_rb) {
	bool r = false;

	REPOSITORY = p_rb; // keep repository pointer
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	
	get_mutex();
	if(m_p_mutex)
		r = true;

	return r;
}

HMUTEX cB24PMA::lock(HMUTEX hlock) {
	HMUTEX r = 0;

	iMutex *p_mutex = get_mutex();
	if(p_mutex) {
		r = acquire_mutex(p_mutex, hlock);
	}

	return r;
}

void cB24PMA::unlock(HMUTEX hlock) {
	iMutex *p_mutex = get_mutex();
	if(p_mutex)
		p_mutex->unlock(hlock);
}

void cB24PMA::init_ram_block(_ulong addr, _ulong size) {
	if(size > ((MAX_REC_AMOUNT + 1)*m_page_size)) {
		// allign ram address
		_u32 rem = m_page_size - (addr % m_page_size);
		_ulong _addr = addr + rem;
		_ulong _size = size - rem;

		_ulong ram_pages = _size / m_page_size; // number of pages
		_ulong records = ram_pages / MAX_REC_AMOUNT;
		_ulong rec_mem = (records * sizeof(_rec_t));
		_ulong rec_pages = rec_mem / m_page_size;
		rec_pages += (rec_mem % m_page_size)?1:0;

		ram_pages -= rec_pages;

		records = ram_pages / MAX_REC_AMOUNT;
		rec_mem = records * sizeof(_rec_t);
		rec_pages = rec_mem / m_page_size;
		rec_pages += (rec_mem % m_page_size)?1:0;
		
		// clear record area
		mem_set((_u8 *)_addr, 0, rec_pages * m_page_size);

		m_ram[m_nram].addr  = _addr + (rec_pages * m_page_size);
		m_ram[m_nram].nrec  = records;
		m_ram[m_nram].p_rec = (_rec_t *)_addr;
		m_ram[m_nram].nfree = records * MAX_REC_AMOUNT;
		m_nram++;
	}
}

bool cB24PMA::init(_ram_part_t *p_ram_array, // pointer to ram parts array
		  _u16 nparts, // number of ram parts
		  _u16 page_size // page size in bytes
		 ) {
	bool r = true;
	m_nram = 0;
	m_page_size = page_size;
	mem_set((_u8 *)m_ram, 0, sizeof(m_ram));
	for(_u16 i = 0; i < nparts; i++)
		init_ram_block(p_ram_array[i].addr, p_ram_array[i].size);
	return r;
}

bool cB24PMA::address_to_position(_ulong addr, // in
					_ram_t **pp_block, // out
					_s16	*p_block_idx, // out
					_rec_t **pp_rec, // out
					_ulong	*p_rec_idx, // out
					_u8     *p_bit // out
				) {
	bool r = false;
	// allign address
	_ulong _addr = addr - (addr % m_page_size);
	// search block
	for(_s16 i = 0; i < m_nram; i++) {
		_ulong block_end = m_ram[i].addr + ((m_ram[i].nrec * MAX_REC_AMOUNT) * m_page_size);
		if(m_ram[i].addr <= _addr && _addr < block_end) {
			_ram_t *p_block = &m_ram[i];
			_s16 block_idx = i;
			_ulong block_page = (_addr - m_ram[i].addr) / m_page_size; // block_page
			_ulong rec_idx = block_page / MAX_REC_AMOUNT; // record index
			_rec_t *p_rec = &(m_ram[i].p_rec[rec_idx]);
			_u8 bit = block_page % MAX_REC_AMOUNT; // record bit

			*pp_block = p_block;
			*pp_rec = p_rec;
			*p_block_idx = block_idx;
			*p_rec_idx = rec_idx;
			*p_bit = bit;

			r = true;
			break;
		}
	}
	return r;
}

_ulong cB24PMA::position_to_address(_ram_t *p_block, _rec_t *p_rec, _u8 bit) {
	_ulong r = 0;
	for(_u32 i = 0; i < p_block->nrec; i++) {
		_rec_t *pr = p_block->p_rec + i;
		if(pr == p_rec) {
			_ulong base = p_block->addr + ((i * MAX_REC_AMOUNT) * m_page_size);
			r = base + (bit * m_page_size);
			break;
		}
	}
	return r;
}

_ulong cB24PMA::position_to_address(_s16 block_idx, _ulong rec_idx, _u8 bit) {
	_ulong r = 0;
	if(block_idx < m_nram) {
		if(rec_idx < m_ram[block_idx].nrec) { 
			_ulong base = m_ram[block_idx].addr + ((rec_idx * MAX_REC_AMOUNT) * m_page_size);
			r = base + (bit * m_page_size);
		}
	}
	return r;
}

_u32 cB24PMA::set_bits(_ram_t *p_block, _rec_t *p_rec, _u8 bit, _u32 count, _u8 type, bool set) {
	_rec_t *rec = p_rec;
	_rec_t *rec_end = (p_block->p_rec + p_block->nrec);
	_u32 cnt = 0;
	_ulong mask = (1<<(MAX_REC_AMOUNT-1));
	mask >>= bit;

	while(cnt < count && rec < rec_end) {
		rec->type = type;
		while(mask && cnt < count) {
			if(set) {
				if(!(rec->bmp & mask)) {
					rec->bmp |= mask;
					rec->amount++;
					p_block->nfree--;
				}
			} else {
				if(rec->bmp & mask) {
					rec->bmp &= ~mask;
					rec->amount--;
					p_block->nfree++;
				}
			}
			mask >>= 1;
			cnt++;
		}
		rec++;
		mask = (1 << (MAX_REC_AMOUNT - 1));
	}

	return cnt;
}

bool cB24PMA::find(_ulong limit, _u8 type, _u32 count, // in
			_ram_t **pp_block, _s16 *p_bidx, // out 
			_rec_t **pp_rec, _ulong *p_ridx, _u8 *p_bit // out
		  ) {
	bool r = false;
	_s16 block_idx = m_nram - 1;
	_ram_t *p_block = &m_ram[block_idx];
	_ulong rec_idx = p_block->nrec-1;
	_rec_t *p_rec = &(p_block->p_rec[rec_idx]);
	_u8 bit = 0xff;
	_u32 cnt = 0;
	_ulong end_block_addr = (p_block->addr + ((p_block->nrec * MAX_REC_AMOUNT) * m_page_size))-1;
	_ulong _limit = limit;

	while(_limit <= p_block->addr) {
		if(block_idx) {
			block_idx--;
			p_block = &m_ram[block_idx];
			rec_idx = p_block->nrec-1;
			p_rec = &(p_block->p_rec[rec_idx]);
			end_block_addr = (p_block->addr + ((p_block->nrec * MAX_REC_AMOUNT) * m_page_size))-1;
		} else
			return false;
	}

	_u8 shift = 0;
	if(_limit > end_block_addr) {
		_limit = end_block_addr;
		shift = 1;
	}

	if(!address_to_position(_limit, &p_block, &block_idx, &p_rec, &rec_idx, &bit))
		return false;

	_ulong mask = 1;
	if(bit != 0xff)
		mask = (1 << (MAX_REC_AMOUNT-shift)) >> bit;

	while(p_block >= &m_ram[0]) {
		while(p_rec >= p_block->p_rec) {
			if((p_rec->type == 0 || p_rec->type == type) && p_rec->amount < MAX_REC_AMOUNT) {
				_ulong v = p_rec->bmp;
				while(mask < (1 << MAX_REC_AMOUNT)) {
					if(!(v & mask)) {
						cnt++;
						if(cnt == count)
							goto _find_done_;
					} else
						cnt = 0;

					mask <<= 1;
				}
			} else
				cnt = 0;

			if(rec_idx) {
				rec_idx--;
				p_rec = &(p_block->p_rec[rec_idx]);
				mask = 1;
			} else
				break;
		}
		
		if(block_idx) {
			block_idx--;
			p_block = &m_ram[block_idx];
			rec_idx = p_block->nrec-1;
			p_rec = &(p_block->p_rec[rec_idx]);
			mask = 1;
			cnt = 0;
		} else
			break;
	}
_find_done_:
	if(cnt == count) {
		*pp_block = p_block;
		*pp_rec = p_rec;
		*p_bidx = block_idx;
		*p_ridx = rec_idx;
		_ulong m = (1 << (MAX_REC_AMOUNT-1));
		for(bit = 0; bit < MAX_REC_AMOUNT; bit++) {
			if(m & mask) {
				*p_bit = bit;
				break;
			}
			m >>= 1;
		}
		r = true;
	}

	return r;
}

void cB24PMA::info(_ram_info_t *p_info) {
	HMUTEX hlock = lock();
	p_info->ps = m_page_size;
	p_info->rp = 0;
	p_info->pu = 0;
	p_info->sp = 0;
	for(_u32 i = 0; i < m_nram; i++) {
		p_info->rp += (m_ram[i].nrec * MAX_REC_AMOUNT);
		p_info->pu += ((m_ram[i].nrec * MAX_REC_AMOUNT) - m_ram[i].nfree);
		p_info->sp += ((m_ram[i].nrec * sizeof(_rec_t)) / m_page_size);
		p_info->sp += ((m_ram[i].nrec * sizeof(_rec_t)) % m_page_size)?1:0;
	}
	unlock(hlock);
}

// consequently alloc
void *cB24PMA::alloc(_u32 npages, _u8 type, _ulong limit) {
	void *r = 0;
	_ram_t *p_block=0;
	_rec_t *p_rec=0;
	_s16 block_idx = 0;
	_ulong rec_idx = 0;
	_u8 bit=0;

	HMUTEX hlock = lock();

	if(find(limit, type, npages, &p_block, &block_idx, &p_rec, &rec_idx, &bit)) {
		_u32 _r = set_bits(p_block, p_rec, bit, npages, type, true);
		if(_r == npages)
			r = (void *)position_to_address(block_idx, rec_idx, bit);
		else // rollback
			set_bits(p_block, p_rec, bit, _r, 0, false);
	}

	unlock(hlock);

	return r;
}

// consequently free
_u32 cB24PMA::free(void *p, _u32 npages) {
	_u32 r = 0;
	_ram_t *p_block=0;
	_rec_t *p_rec=0;
	_s16 block_idx = 0;
	_ulong rec_idx = 0;
	_u8 bit=0;

	HMUTEX hlock = lock();

	if(address_to_position((_ulong)p, &p_block, &block_idx, &p_rec, &rec_idx, &bit))
		r = set_bits(p_block, p_rec, bit, npages, p_rec->type, false);

	unlock(hlock);

	return r;
}

// inconsistent alloc
_u32 cB24PMA::alloc(/* in */_u32 npages, // number of pages to alloc
		   /* out */_u64 *page_array, // array of page addresses in PM
		   /* in */_u8 type,
	  	   /* in */ _ulong limit
		  ) {
	_u32 r = 0;
	HMUTEX hlock = lock();
	for(_u32 i = 0; i < npages; i++) {
		_ram_t *p_block=0;
		_rec_t *p_rec=0;
		_s16 block_idx = 0;
		_ulong rec_idx = 0;
		_u8 bit=0;

		_ulong _limit = limit;
		if(find(_limit, type, 1, &p_block, &block_idx, &p_rec, &rec_idx, &bit)) {
			if(set_bits(p_block, p_rec, bit, 1, type, true) == 1) {
				_ulong addr = position_to_address(block_idx, rec_idx, bit);
				if(addr) {
					_limit = page_array[i] = addr;
					r = i;
				} else
					break;
			} else
				break;
		} else
			break;
	}
	unlock(hlock);
	return r;
}

// inconsistent free
_u32 cB24PMA::free(/* in */_u32 npages, // number pages to release
		   /* in */_u64 *page_array // array of page addresses in PM
		  ) {
	_u32 r = 0;
	HMUTEX hlock = lock();
	for(_u32 i = 0; i < npages; i++) {
		_ram_t *p_block=0;
		_rec_t *p_rec=0;
		_s16 block_idx = 0;
		_ulong rec_idx = 0;
		_u8 bit=0;

		if(address_to_position(page_array[i], &p_block, &block_idx, &p_rec, &rec_idx, &bit)) {
			if(set_bits(p_block, p_rec, bit, 1, p_rec->type, false) != 1)
				break;
		} else
			break;
	}
	unlock(hlock);
	return r;
}

_u32 cB24PMA::alloc(void *p, _u32 npages, _u8 type) {
	_u32 r = 0;
	HMUTEX hlock = lock();
	_ram_t *p_block=0;
	_rec_t *p_rec=0;
	_s16 block_idx = 0;
	_ulong rec_idx = 0;
	_u8 bit=0;
	
	if(address_to_position((_ulong)p, &p_block, &block_idx, &p_rec, &rec_idx, &bit))
		r = set_bits(p_block, p_rec, bit, npages, type, true);

	unlock(hlock);
	return r;
}

_u8 cB24PMA::get_type(void *p) {
	_u8 r = 0;
	HMUTEX hlock = lock();
	_ram_t *p_block=0;
	_rec_t *p_rec=0;
	_s16 block_idx = 0;
	_ulong rec_idx = 0;
	_u8 bit=0;
	
	if(address_to_position((_ulong)p, &p_block, &block_idx, &p_rec, &rec_idx, &bit))
		r = p_rec->type;
	
	unlock(hlock);
	return r;
}

void cB24PMA::dump(void) {
	for(_u16 i = 0; i < m_nram; i++) {
		LOG("0x%h (%u) 0x%h --- 0x%h\r\n", m_ram[i].p_rec, m_ram[i].nrec,
			m_ram[i].addr, m_ram[i].addr + ((m_ram[i].nrec * MAX_REC_AMOUNT)*m_page_size)-1);
	}
}

