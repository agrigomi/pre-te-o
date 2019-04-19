#include "iBlockDevice.h"
#include "iSystemLog.h"
#include "iSyncObject.h"
#include "iScheduler.h"
#include "iDataStorage.h"
#include "iCPUHost.h"

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

#define CBSTATE_PRESENT	(1<<0)
#define CBSTATE_DIRTY	(1<<1)

#define INVALID_HANDLE_INDEX	0xffffffff
#define MAX_POOL_MB		256

#define CACHE_FLUSH_TIME_MS	5000 // 3s

#define CBA // use cache block addressing mode instead of LBA
#define USE_HEAP
#define USE_SYSTEM_HEAP

typedef struct {
	_u64		bstart;		// start block (LBA)
	_u32		cstate	:2;	// chunk state
	_u32		bcount	:30; 	// number of blocks
	_u64		tstamp;		// reference counter
	iMutex		*p_mutex;
}__attribute__((packed)) _chunk_hdr_t;

typedef struct {
	_cache_info_t	cinfo;
	_chunk_hdr_t	*p_fhdr;	// pointer to first chunk
	_u32		mem_sz;		// memory size in bytes
	_u32		chunk_sz;	// size of chunk in bytes
	_u16		hdr_size;	// size of chunk header
	iMutex		*p_mutex;
}__attribute__((packed)) _cache_entry_t;

class cCache:public iCache {
public:
	cCache() {
		m_p_pma = 0;
		m_p_syslog = 0;
		m_p_vector = 0;
		m_p_cmap = 0;
		m_p_heap = 0;
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	~cCache() {}
	
	DECLARE_BASE(cCache);
	DECLARE_INIT();
	DECLARE_DESTROY();

	HCACHE alloc(_cache_info_t *p_ci);
	void free(HCACHE handle);
	_u32 cache_ioctl(_u32 cmd, _cache_io_t *cio);

protected:
	iSystemLog 	*m_p_syslog;
	iPMA		*m_p_pma;
	iVector		*m_p_vector;
	_cache_entry_t	*m_p_cmap;
	iHeap		*m_p_heap;
	
	HMUTEX	lock(HMUTEX hlock=0);
	void unlock(HMUTEX hlock);
	HMUTEX lock(iMutex *p_mutex, HMUTEX hlock=0);
	void unlock(iMutex *p_mutex, HMUTEX hlock);
	HMUTEX	lock(_chunk_hdr_t *p_ch, HMUTEX hlock=0);
	void unlock(_chunk_hdr_t *p_ch, HMUTEX hlock);
	HMUTEX lock(_cache_entry_t *hc, HMUTEX hlock=0);
	void unlock(_cache_entry_t *hc, HMUTEX hlock);

	_u32 cache_buffer_lock(_cache_io_t *cio);
	_u32 cache_buffer_unlock(_cache_io_t *cio);
	_u32 cache_dirty(_cache_io_t *cio);
	_u32 cache_io(_cache_io_t *cio, bool write=false);
	void cache_flush(HCACHE chandle);
	void cache_flush(void);
	_u8 *alloc_cache_memory(_u32 mem_sz);
	_u32 htoidx(HCACHE h, HMUTEX hlock);
	_chunk_hdr_t *get_chunk(_cache_entry_t *hc, _u32 num);
	_u8 *get_cache_ptr(_cache_entry_t *hc, _chunk_hdr_t *p_ch);
	_u8 *get_bstate_ptr(_chunk_hdr_t *p_ch);
	_chunk_hdr_t *select_chunk(_cache_entry_t *hc, _u64 bstart, HMUTEX hclock=0);
	void set_bstate(_chunk_hdr_t *p_ch, _u64 bstart, _u32 bcount, _u8 mask);
	_u8 get_bstate(_chunk_hdr_t *p_ch, _u64 bnum);
	void cache_flush_chunk(_cache_entry_t *hc, _chunk_hdr_t *p_ch);
	_u32 read_whole_chunk(_cache_entry_t *p_hcache, _chunk_hdr_t *p_ch, _u64 bstart);
	_u32 update_chunk(_cache_entry_t *p_hcache, _chunk_hdr_t *p_ch);
	_u8 *get_chunk_buffer(_cache_entry_t *p_hcache, _chunk_hdr_t *p_ch, _u64 bstart, _u32 *p_bcount);
#ifdef USE_HEAP
	bool alloc_cache_pool(void);
#endif
	friend _s32 _auto_flush_thread(void *p_data);
};

static volatile bool _af_create_=true;
static volatile bool _af_running_=false;
static volatile bool _af_stopped_=true;

_s32 _auto_flush_thread(void *p_data) {
	cCache *p_obj = (cCache *)p_data;
	if(_af_running_ == false) {
		_af_running_=true;
		_af_stopped_=false;

		while(_af_running_) {
			p_obj->cache_flush();
			sleep_systhread(5000);
		}

		_af_stopped_=true;
	}
	return 0;
}

IMPLEMENT_BASE(cCache, "cCache", 0x0001);
IMPLEMENT_INIT(cCache, rbase) {
	bool r = false;
	REPOSITORY = rbase;
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	m_p_pma = (iPMA *)OBJECT_BY_INTERFACE(I_PMA, RY_STANDALONE);
	m_p_vector = (iVector *)OBJECT_BY_INTERFACE(I_VECTOR, RY_CLONE);
#ifdef USE_SYSTEM_HEAP
	m_p_heap = (iHeap *)OBJECT_BY_INTERFACE(I_HEAP, RY_STANDALONE);
#else		
	m_p_heap = (iHeap *)OBJECT_BY_INTERFACE(I_HEAP, RY_CLONE);
#endif	
	if(m_p_pma && m_p_vector && m_p_heap) {
		m_p_vector->init(sizeof(_cache_entry_t));
#ifdef USE_HEAP		
		r = alloc_cache_pool();
#else
		r = true;
#endif		
	}
	return r;
}
IMPLEMENT_DESTROY(cCache) {
	if(m_p_vector) {
		if(_af_running_) {
			_af_running_ = false;
			while(_af_stopped_==false)
				sleep_systhread(100);
			_af_create_=true;
		}
		m_p_vector->clr();
		RELEASE_OBJECT(m_p_vector);
		m_p_vector = 0;
	}
	RELEASE_OBJECT(m_p_heap);
}

HMUTEX cCache::lock(iMutex *p_mutex, HMUTEX hlock) {
	HMUTEX r = 0;

	if(p_mutex) {
		iBlender *p_shd = get_blender();
		if(p_shd) {
			if(p_shd->is_running())
				r = p_shd->acquire_mutex(p_mutex, hlock);
		}
		
		if(!r)
			r = p_mutex->lock(hlock);
	}

	return r;
}
void cCache::unlock(iMutex *p_mutex, HMUTEX hlock) {
	if(p_mutex)
		p_mutex->unlock(hlock);
}

HMUTEX cCache::lock(HMUTEX hlock) {
	HMUTEX r = 0;
	
	if(m_p_vector) {
		iMutex *p_mutex = m_p_vector->get_mutex();
		r = lock(p_mutex, hlock);
	}
	
	return r;
}
void cCache::unlock(HMUTEX hlock) {
	if(m_p_vector)
		m_p_vector->unlock(hlock);
}

HMUTEX cCache::lock(_chunk_hdr_t *p_ch, HMUTEX hlock) {
	if(!p_ch->p_mutex)
		p_ch->p_mutex = (iMutex *)OBJECT_BY_INTERFACE(I_MUTEX, RY_CLONE);

	return lock(p_ch->p_mutex, hlock);
}

void cCache::unlock(_chunk_hdr_t *p_ch, HMUTEX hlock) {
	unlock(p_ch->p_mutex, hlock);
}

HMUTEX cCache::lock(_cache_entry_t *hc, HMUTEX hlock) {
	return lock(hc->p_mutex, hlock);
}
void cCache::unlock(_cache_entry_t *hc, HMUTEX hlock) {
	unlock(hc->p_mutex, hlock);
}
#ifdef USE_HEAP
bool cCache::alloc_cache_pool(void) {
	bool r = false;
#ifndef	USE_SYSTEM_HEAP
	_ram_info_t ri;

	m_p_pma->info(&ri);
	_u32 free_pages = ri.rp - (ri.pu + ri.sp);
	_u32 pool_pages =  free_pages / 3;
	_u32 max_pool_pages = (MAX_POOL_MB *(1024*1024)) / ri.ps;

	if(pool_pages > max_pool_pages)
		pool_pages = max_pool_pages;

	_u32 pool_size = pool_pages * ri.ps;
	r = m_p_heap->init(pool_size);
#else
	r = true;
#endif
	return r;
}
#endif
_u8 *cCache::alloc_cache_memory(_u32 mem_sz) {
	_u8 *r = 0;
	_u32 page_size = m_p_pma->get_page_size();
	_u32 npages = mem_sz / page_size;
	npages += (mem_sz  % page_size)?1:0;
#ifndef USE_HEAP
	r = (_u8 *)m_p_pma->alloc(npages);
#else
	r = (_u8 *)m_p_heap->alloc(npages * page_size);
#endif// USE_HEAP	
	return r;
}

HCACHE cCache::alloc(_cache_info_t *p_ci) {
	HCACHE r  = 0;
	_u32 bstate_sz = (p_ci->chunk_size / 4 ); // 2 bits per block for block state
	_u32 hdr_bytes = bstate_sz + sizeof(_chunk_hdr_t);
	_u32 hdr_kb = hdr_bytes/1024;
	hdr_kb += (hdr_bytes % 1024)?1:0;
	hdr_bytes = hdr_kb*1024;
	_u32 chunk_sz = (p_ci->chunk_size * p_ci->block_size) + hdr_bytes; // size of chunk in bytes
	_u32 cache_mem_sz = p_ci->num_chunks * chunk_sz; // all cache size in bytes

	_u8 *p_cmem = alloc_cache_memory(cache_mem_sz);
	if(p_cmem) {
		_cache_entry_t che;

		mem_set((_u8 *)&che, 0, sizeof(_cache_entry_t));

		mem_cpy((_u8 *)(&che.cinfo), (_u8 *)p_ci, sizeof(_cache_info_t));
		che.mem_sz = cache_mem_sz;
		che.p_fhdr = (_chunk_hdr_t *)p_cmem;
		che.chunk_sz = chunk_sz;
		che.hdr_size = hdr_bytes;
		mem_set(p_cmem, 0, cache_mem_sz);
		che.p_mutex = (iMutex *)OBJECT_BY_INTERFACE(I_MUTEX, RY_CLONE);
		r = m_p_vector->add(&che);
		DBG("cache alloc: %u Kb\r\n", cache_mem_sz/1024);
		DBG("chunks: %u; bstate: %u; hdr: %u; cache: %u; chunk size: %u\r\n", 
			p_ci->num_chunks, bstate_sz, hdr_bytes,p_ci->chunk_size * p_ci->block_size, chunk_sz);
		if(_af_create_ == true) {
			create_systhread(_auto_flush_thread, 0, this);
			_af_create_=false;
		}
	} else {
		LOG("ERROR: failed to alloc cache memory (%u KB)\r\n", cache_mem_sz/1024);
	}
	return r;
}

_u32 cCache::htoidx(HCACHE h, HMUTEX hlock) {
	_u32 r = INVALID_HANDLE_INDEX;
	HMUTEX hm = lock(hlock);
	_u32 cnt = m_p_vector->cnt(hm);
	for(_u32 i = 0; i < cnt; i++) {
		if(m_p_vector->get(i,hm) == h) {
			r = i;
			break;
		}
	}
	unlock(hm);
	return r;
}

_chunk_hdr_t *cCache::get_chunk(_cache_entry_t *hc, _u32 num) {
	_chunk_hdr_t *r = 0;
	if(num < hc->cinfo.num_chunks) {
		_u8 *p_chdr = (_u8 *)hc->p_fhdr;
		r = (_chunk_hdr_t *)(p_chdr + (num * hc->chunk_sz));
	}
	return r;
}

_u8 *cCache::get_cache_ptr(_cache_entry_t *hc, _chunk_hdr_t *p_ch) {
	return ((_u8 *)p_ch)+hc->hdr_size;
}

_u8 *cCache::get_bstate_ptr(_chunk_hdr_t *p_ch) {
	return ((_u8 *)p_ch)+sizeof(_chunk_hdr_t);
}

void cCache::free(HCACHE handle) {
	HMUTEX hlock = lock();
	_u32 vidx = htoidx(handle, hlock);
	if(vidx != INVALID_HANDLE_INDEX) {
		_cache_entry_t *p_che = (_cache_entry_t *)handle;
		HMUTEX hclock = lock(p_che);
		// release all chunk linked mutex
		_u32 nc = p_che->cinfo.num_chunks;
		for(_u32 i = 0; i < nc; i++) {
			_chunk_hdr_t *p_ch = get_chunk(p_che, i);
			if(p_ch) {
				HMUTEX hlock = lock(p_ch);
				cache_flush_chunk(p_che, p_ch);
				unlock(p_ch, hlock);
				if(p_ch->p_mutex)
					RELEASE_OBJECT(p_ch->p_mutex);
			}
		}
		_u32 page_size = m_p_pma->get_page_size();
		_u32 npages = p_che->mem_sz / page_size;
		npages += (p_che->mem_sz % page_size) ? 1 : 0;
#ifndef USE_HEAP
		m_p_pma->free(p_che->p_fhdr, npages);
#else
		m_p_heap->free(p_che->p_fhdr, npages * page_size);					
#endif
		unlock(p_che, hclock);
		RELEASE_OBJECT(p_che->p_mutex);
		m_p_vector->del(vidx, hlock);
	}
	unlock(hlock);
}

_chunk_hdr_t *cCache::select_chunk(_cache_entry_t *hc, _u64 bstart, HMUTEX hclock) {
	_chunk_hdr_t *r = 0;
	_u32 nc = 0;
	_chunk_hdr_t *p_ch = 0;
	_chunk_hdr_t *p_ch_free = 0;
	_chunk_hdr_t *p_ch_min_ref = 0;
	_u64  tmexp = 0xffffffffffffffff;

	HMUTEX hlock = lock(hc, hclock);
	
	_u32 ch_nblocks = hc->cinfo.chunk_size;
	while((p_ch = get_chunk(hc, nc))) {
		_u32 ch_start = p_ch->bstart;
		_u32 ch_end = p_ch->bstart + ch_nblocks;
#ifndef CBA
		_u32 bend = bstart + ch_nblocks;
#endif

		if(p_ch->cstate & CBSTATE_PRESENT) {
			if((bstart >= ch_start && bstart < ch_end) 
#ifndef CBA
					||
					(bend > ch_start && bend < ch_end)
#endif
					) {
				r = p_ch;
				break;
			}
		} else { // keep the first free chunk
			if(!p_ch_free)
				p_ch_free = p_ch;

			if(p_ch->tstamp < tmexp) {
				tmexp = p_ch->tstamp;
				p_ch_min_ref = p_ch;
			}
		}
		nc++;
	}

	if(!r)
		r = p_ch_free;

	if(!r)
		r = p_ch_min_ref;

	if(r) {
		iCPU *p_cpu = get_cpu();
		if(p_cpu)
			r->tstamp = p_cpu->timestamp();
	}

	unlock(hc, hlock);

	return r;
}

void cCache::set_bstate(_chunk_hdr_t *p_ch, _u64 bstart, _u32 bcount, _u8 mask) {
	_u8 *bstate = get_bstate_ptr(p_ch);
	_u64 bs = bstart;
	if(bs >= p_ch->bstart && bs < (p_ch->bstart + p_ch->bcount)) {
		for(_u32 i = 0; i < bcount; i++) {
			_u32 offset = bs - p_ch->bstart;
			_u32 sbyte = offset / 4;
			_u8 fbit = (offset % 4) * 2;
			_u8 _mask = 0x80 >> fbit;
			if(mask & CBSTATE_DIRTY)
				*(bstate + sbyte) |= _mask;
			else
				*(bstate + sbyte) &= ~_mask;

			_mask >>= 1;

			if(mask & CBSTATE_PRESENT)
				*(bstate + sbyte) |= _mask;
			else
				*(bstate + sbyte) &= ~_mask;
				
			bs++;
		}
	}
}

_u8 cCache::get_bstate(_chunk_hdr_t *p_ch, _u64 bnum) {
	_u8 r = 0;
	_u8 *bstate = get_bstate_ptr(p_ch);
	if(bnum >= p_ch->bstart && bnum < (p_ch->bstart + p_ch->bcount)) {
		_u32 offset = bnum - p_ch->bstart;
		_u32 byte = offset / 4;
		_u8 fbit = (offset % 4) * 2;
		r = (*(bstate + byte) & (0xc0 >> fbit) << fbit) >> 6;
	}

	return r;
}

_u32 cCache::read_whole_chunk(_cache_entry_t *p_hcache, _chunk_hdr_t *p_ch, _u64 bstart) {
	_u32 r = 0;

	cache_flush_chunk(p_hcache, p_ch);

	_cache_io_t cio;
#ifdef CBA
	_u32 chsz = p_hcache->cinfo.chunk_size;
	cio.bstart = (bstart / chsz) * chsz;
#else
	cio.bstart = bstart;
#endif
	cio.buffer = get_cache_ptr(p_hcache, p_ch);
	cio.p_udata = p_hcache->cinfo.p_udata;
	cio.bcount = p_hcache->cinfo.chunk_size;
	cio.handle = p_hcache;
	if((r = p_hcache->cinfo.p_ccb(CIOCTL_READ, &cio))) {
		p_ch->bstart = cio.bstart;
		p_ch->bcount = r;
		p_ch->cstate |= CBSTATE_PRESENT;
		set_bstate(p_ch, p_ch->bstart, r, CBSTATE_PRESENT);
		DBG("cache read whole chunk(0x%h)  %d blocks, lba=%u-->ch.bstart=%u,ch.bcount=%u\r\n", 
				p_ch, r, cio.bstart, p_ch->bstart, p_ch->bcount);
	} else {
		cio.buffer = 0;
		DBG("ERROR: read_whole_chunk(0x%h)\r\n",p_ch);
	}

	return r;
}

_u32 cCache::update_chunk(_cache_entry_t *p_hcache, _chunk_hdr_t *p_ch) {
	_u32 r = 0;
	
	_cache_io_t cio;

	cio.bstart = p_ch->bstart + p_ch->bcount;
	cio.p_udata = p_hcache->cinfo.p_udata;
	cio.handle = p_hcache;
	
	_u32 ch_nblocks = p_hcache->cinfo.chunk_size;
	_u32 ch_max = p_ch->bstart + ch_nblocks;

	if(cio.bstart >= p_ch->bstart && cio.bstart < ch_max) {
		_u8 *buf = get_cache_ptr(p_hcache, p_ch);
		cio.buffer = buf+(p_ch->bcount * p_hcache->cinfo.block_size);
		cio.bcount = ch_nblocks - p_ch->bcount;
		DBG("update_chunk ch.bstart=%u, ch.bcount=%u --> %u, %u\r\n", 
				p_ch->bstart, p_ch->bcount, cio.bstart, cio.bcount);
		r = p_hcache->cinfo.p_ccb(CIOCTL_READ, &cio);
		p_ch->bcount += r;
		p_ch->cstate |= CBSTATE_PRESENT;
		set_bstate(p_ch, cio.bstart, r, CBSTATE_PRESENT);
		DBG("Updated with %u blocks -->ch.bstart=%u, ch.bcount=%u\r\n", r, p_ch->bstart, p_ch->bcount);
	}

	return r;
}

_u8 *cCache::get_chunk_buffer(_cache_entry_t *p_hcache, _chunk_hdr_t *p_ch, _u64 bstart, _u32 *p_bcount) {
	_u8 *r = 0;

	_u32 ch_nblocks = p_hcache->cinfo.chunk_size;
	_u32 ch_max = p_ch->bstart + ch_nblocks;

	if(bstart >= p_ch->bstart && bstart < ch_max) {
		_u8 *buf = get_cache_ptr(p_hcache, p_ch);
		_u32 cbn = bstart - p_ch->bstart; // chunk block number
		r = buf + (cbn * p_hcache->cinfo.block_size);
		if(cbn < p_ch->bcount)
			*p_bcount = ch_max - cbn;
		else
			*p_bcount = 0;
	} else
		*p_bcount = 0;

	return r;
}

// lock the selected chunk and return number of posible blocks for i/o operation
_u32 cCache::cache_buffer_lock(_cache_io_t *cio) {
	_u32 r = 0;
	_cache_entry_t *p_hcache = (_cache_entry_t *)cio->handle;
	_chunk_hdr_t *p_ch = select_chunk(p_hcache, cio->bstart);

	if(p_ch) {
		cio->hlock = lock(p_ch);
		cio->plock = p_ch;

		if(!(p_ch->cstate & CBSTATE_PRESENT)) {
_read_whole_chunk_:
			if((r = read_whole_chunk(p_hcache, p_ch, cio->bstart))) {
				cio->buffer = get_cache_ptr(p_hcache, p_ch);
				cio->p_udata = p_hcache->cinfo.p_udata;
				cio->bcount = r;
				cio->bsize = r * p_hcache->cinfo.block_size;
#ifdef CBA
				goto _verify_chunk_;
#endif
			}
		} else {
_verify_chunk_:
			_u32 bcount = cio->bcount;
			_u8 *buf = get_chunk_buffer(p_hcache, p_ch, cio->bstart, &bcount);
			if(buf && bcount) {
				cio->buffer = buf;
				cio->bcount = r = bcount;
				cio->bsize = r * p_hcache->cinfo.block_size;
			} else {
				if(!buf && !bcount) {
					DBG("Failed to get chunk buffer\r\n","");
					goto _read_whole_chunk_;
				} else {
					if(!bcount) {
						if((r = update_chunk(p_hcache, p_ch)))
							goto _verify_chunk_;
					}
				}
			}
		}
		if(!r || cio->buffer == 0)
			cache_buffer_unlock(cio);
	} else {
		DBG("ERROR:cache select chunk for cache_buffer_lock\r\n","");
	}

	return r;
}
_u32 cCache::cache_dirty(_cache_io_t *cio) {
	_u32 r = 0;
	_cache_entry_t *p_hcache = (_cache_entry_t *)cio->handle;
	_chunk_hdr_t *p_ch = (_chunk_hdr_t *)cio->plock;
	if(p_ch == select_chunk(p_hcache, cio->bstart, cio->bcount)) {
		set_bstate(p_ch, cio->bstart, cio->bcount, CBSTATE_PRESENT|CBSTATE_DIRTY);
		p_ch->cstate |= CBSTATE_DIRTY;
	} else {
		DBG("ERROR: cache select chunk for 'cache_dirty'\r\n","");
	}
	return r;
}
_u32 cCache::cache_buffer_unlock(_cache_io_t *cio) {
	_u32 r = 0;
	_cache_entry_t *p_hcache = (_cache_entry_t *)cio->handle;
	_chunk_hdr_t *p_ch = (_chunk_hdr_t *)cio->plock;
	if(p_ch) {
		if(p_ch == select_chunk(p_hcache, cio->bstart, cio->bcount))
			unlock(p_ch, cio->hlock);
		else {
			DBG("ERROR: cache select chunk for 'cache_buffer_unlock'\r\n","");
		}
	}
	return r;
}
_u32 cCache::cache_io(_cache_io_t *cio, bool write) {
	_u32 r = 0;
	_cache_io_t _cio;
	_u32 bcount = cio->bcount;
	_u32 bstart = cio->bstart;
	_cio.handle = cio->handle;
	_u32 offset = cio->boffset;
	_u8 *buffer = cio->buffer;
	_u32 rest = cio->bsize;

	while(bcount && rest) {
		_cio.bstart = bstart;
		_cio.bcount = bcount;
		_cio.buffer = 0;
		_cio.plock = 0;
		_cio.hlock = 0;

		_u32 nblocks = 0;
		if((nblocks = cache_buffer_lock(&_cio))) {
			if(_cio.buffer) {
				_u32 nbytes = (rest < _cio.bsize)?rest:_cio.bsize;
				if(write)
					mem_cpy(_cio.buffer + offset, buffer, nbytes);
				else
					mem_cpy(buffer, _cio.buffer + offset, nbytes);
				buffer += nbytes;
				rest -= nbytes;
				bstart += nblocks;
				if(nblocks > bcount)
					bcount = 0;
				else
					bcount -= nblocks;
				r += nbytes;
				offset = 0;
				if(write)
					cache_dirty(&_cio);
			}
			cache_buffer_unlock(&_cio);
		} else
			break;
	}
	return r;
}
void cCache::cache_flush_chunk(_cache_entry_t *hc, _chunk_hdr_t *p_ch) {
	if(p_ch->cstate & CBSTATE_DIRTY) {
		DBG("cache_flush_chunk(0x%h, ch_start = %u, ch_count = %d)\r\n", p_ch, p_ch->bstart, p_ch->bcount);
		_cache_io_t cio;
		_u64 bs = p_ch->bstart;
		cio.bstart = bs;
		cio.bcount = 0;
		cio.p_udata = hc->cinfo.p_udata;
		cio.handle = hc;
		_u8 bstate = 0;

		while(bs >= p_ch->bstart && bs < (p_ch->bstart + p_ch->bcount)) {
			bstate = get_bstate(p_ch, bs);
			if((bstate & CBSTATE_DIRTY) && (bstate & CBSTATE_PRESENT))
				cio.bcount++;
			else {
				if(cio.bcount) {
_flush_buffer_:
					cio.buffer = get_cache_ptr(hc, p_ch) + 
						((cio.bstart - p_ch->bstart) * hc->cinfo.block_size);
					_u32 nb = hc->cinfo.p_ccb(CIOCTL_WRITE, &cio);
					DBG("flish ...%d blocks from lba:%u\r\n",cio.bcount, cio.bstart);
					set_bstate(p_ch, cio.bstart, nb, CBSTATE_PRESENT);
				}
				cio.bstart = bs;
				cio.bcount = 0;
			}
			
			bs++;
		}
		
		if(cio.bcount)
			goto _flush_buffer_;

		p_ch->cstate &= ~CBSTATE_DIRTY;
	}
}
void cCache::cache_flush(HCACHE chandle) {
	_cache_entry_t *p_hcache = (_cache_entry_t *)chandle;
	_u32 nc = p_hcache->cinfo.num_chunks;
	for(_u32 i = 0; i < nc; i++) {
		_chunk_hdr_t *p_ch = get_chunk(p_hcache, i);
		if(p_ch) {
			HMUTEX hlock = lock(p_ch);
			cache_flush_chunk(p_hcache, p_ch);
			unlock(p_ch, hlock);
		}
	}
}

void cCache::cache_flush(void) {
	HMUTEX hlock = lock();
	HCACHE hc = m_p_vector->first(hlock);
	do {
		cache_flush(hc);
	} while((hc = m_p_vector->next(hlock)));
	unlock(hlock);
}

_u32 cCache::cache_ioctl(_u32 cmd, _cache_io_t *cio) {
	_u32 r = 0;
	
	switch(cmd) {
		case CIOCTL_READ:
			r = cache_io(cio);
			break;
		case CIOCTL_WRITE:
			r = cache_io(cio, true);
			break;
		case CIOCTL_FLUSH:
			cache_flush(cio->handle);
			break;
	}
	
	return r;
}
