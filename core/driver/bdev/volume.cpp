#include "iBlockDevice.h"
#include "iMemory.h"
#include "iSystemLog.h"
#include "iSyncObject.h"
#include "str.h"
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

#define PT_OFFSET		0x1be	// partition table offset in MBR

#define HDD_CACHE_CHUNKS	32
#define CDR_CACHE_CHUNKS	16
#define FDD_CACHE_CHUNKS	16
#define RDD_CACHE_CHUNKS	16
#define UMS_CACHE_CHUNKS	16

typedef struct { // partition table record
	_u8	reserved	:7;
	_u8	active		:1;
	struct {
		_u8	head;
		_u8	sector	:6;
		_u8	cyl_hi	:2;
		_u8	cyl_lo;
	}__attribute__((packed)) chs_start;
	_u8	type;
	struct {
		_u8	head;
		_u8	sector	:6;
		_u8	cyl_hi	:2;
		_u8	cyl_lo;
	} __attribute__((packed)) chs_end;
	struct {
		_u32	first;
		_u32	count;
	}__attribute__((packed)) lba;
} __attribute__((packed)) _pt_entry_t;

class cVolume: public iVolume {
public:
	cVolume() {
		DRIVER_TYPE = DTYPE_VOL;
		DRIVER_MODE = DMODE_BLOCK;
		m_p_syslog = 0;
		m_current_offset = 0;
		m_cache_handle = 0;
		m_p_cache = 0;
		m_p_ncbuf = 0;
		m_ncp_count = 0;
		m_p_pma = 0;
		m_io_enable = false;
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}
	//virtual ~cVolume() {}

	DECLARE_BASE(cVolume);
	DECLARE_INIT();
	DECLARE_DESTROY();

	DECLARE_DRIVER_INIT();
	DECLARE_DRIVER_UNINIT();

	_str_t name(void);
	_str_t serial(void);
	
	void driver_ioctl(_u32 cmd, _driver_io_t *p_dio);

protected:
	iSystemLog 	*m_p_syslog;
	_u64		m_current_offset;
	HCACHE		m_cache_handle;
	iCache 		*m_p_cache;
	iPMA		*m_p_pma;
	_u8		*m_p_ncbuf; // for non cacheable file IO
	_u32		m_ncp_count; // number of pages for non cacheable IO
	_u32		m_pma_page_size;
	bool		m_io_enable;
	HMUTEX		m_io_lock;
	iBroadcast	*m_p_broadcast;	

	void scan(void);
	bool verify(_u32 idx, _pt_entry_t *p_pte);

	HMUTEX lock(HMUTEX hlock=0);
	void unlock(HMUTEX hlock);
	void vol_seek(_u64 offset, _handle_t handle=0);
	_u32 vol_serial_io(bool write, _u8 *p_buffer, _u32 sz, _handle_t handle=0);
	_u32 vol_block_io(bool write, _u64 block, _u32 count, _u8 *p_buffer);
	// volume & filesystem I/O
	_u32 vol_file_io(bool write, _u64 offset, _u8 *p_buffer, _u32 sz, _handle_t handle=0);
	bool vol_cache_alloc(void);
	_u32 vol_cache_ioctl(_u32 cmd, _cache_io_t *p_cio);
	friend _u32 _vol_cache_cb(_u32 cmd, _cache_io_t *p_cio);
	void vol_calc_offset(_u64 offset, _u32 sz, _u64 *voffset, _u32 *nblocks, _u64 *fblock, _u16 *fboffset);
	_u8 *alloc_nc_io_buffer(_u32 nblocks);
	void release_nc_io_buffer(void);
	void msg_handler(_msg_t *p_msg);
	friend void _volume_msg_handler(_msg_t *p_msg, void *p_data);
};

void _volume_msg_handler(_msg_t *p_msg, void *p_data) {
	cVolume *p_obj = (cVolume *)p_data;
	p_obj->msg_handler(p_msg);
}

IMPLEMENT_BASE(cVolume, "cVolume", 0x0001);
IMPLEMENT_INIT(cVolume, repository) {
	REPOSITORY = repository;
	INIT_MUTEX();
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	m_p_cache = (iCache *)OBJECT_BY_INTERFACE(I_CACHE, RY_STANDALONE);
	m_p_pma = (iPMA *)OBJECT_BY_INTERFACE(I_PMA, RY_STANDALONE);
	mem_set(_vol_ident_, 0, sizeof(MAX_VOLUME_IDENT));
	if((m_p_broadcast = (iBroadcast *)OBJECT_BY_INTERFACE(I_BROADCAST, RY_STANDALONE)))
		m_p_broadcast->add_handler(_volume_msg_handler, this);
	return true;
}
IMPLEMENT_DESTROY(cVolume) {
	if(m_p_cache) {
		RELEASE_OBJECT(m_p_cache);
		m_p_cache = 0;
	}
	if(m_p_broadcast)
		m_p_broadcast->remove_handler(_volume_msg_handler);
	RELEASE_MUTEX();
}

#define FIRST_DRIVE	'a'

IMPLEMENT_DRIVER_INIT(cVolume, host) {
	bool r = false;
	
	if((DRIVER_HOST = host)) {
		_vol_block_size_ = DRIVER_HOST->_block_size_;
		DRIVER_CLASS = host->DRIVER_CLASS;
		switch(DRIVER_CLASS) {
			case DCLASS_HDD:
				if(_vol_pidx_) {
					_vol_flags_ |= VF_NOCACHE;
					_snprintf(DRIVER_IDENT, sizeof(DRIVER_IDENT), "vhdd%d%c%d", 
							_vol_cidx_, _vol_didx_+FIRST_DRIVE, _vol_pidx_);
				} else
					_snprintf(DRIVER_IDENT, sizeof(DRIVER_IDENT), "vhdd%d%c", 
							_vol_cidx_, _vol_didx_+FIRST_DRIVE);
				scan();
				break;
			case DCLASS_CDROM:
				_vol_flags_ |= VF_READONLY;
				_snprintf(DRIVER_IDENT, sizeof(DRIVER_IDENT), "vcdr%d%c", 
						_vol_cidx_, _vol_didx_+FIRST_DRIVE);
				break;
			case DCLASS_FDD:
				_snprintf(DRIVER_IDENT, sizeof(DRIVER_IDENT), "vfdd%d%c", 
					  _vol_cidx_, _vol_didx_+FIRST_DRIVE);
				break;
			case DCLASS_RDD:
				_snprintf(DRIVER_IDENT, sizeof(DRIVER_IDENT), "vrdd%d", _vol_didx_);
				break;
			case DCLASS_UMS: // USB Mmass Storage
				if(_vol_pidx_) {
					_vol_flags_ |= VF_NOCACHE;
					_snprintf(DRIVER_IDENT, sizeof(DRIVER_IDENT), "vums%d%c%d", 
							_vol_cidx_, _vol_didx_+FIRST_DRIVE, _vol_pidx_);
				} else
					_snprintf(DRIVER_IDENT, sizeof(DRIVER_IDENT), "vums%d%c", 
							_vol_cidx_, _vol_didx_+FIRST_DRIVE);
				scan();
				break;
		}
		vol_cache_alloc();
		m_io_enable = r = true;
	}

	return r;
}
IMPLEMENT_DRIVER_UNINIT(cVolume) {
	if(m_cache_handle) {
		if(m_p_cache) {
			m_p_cache->free(m_cache_handle);
			m_cache_handle = 0;
		}
	}

	release_nc_io_buffer();
}

HMUTEX cVolume::lock(HMUTEX hlock) {
	HMUTEX r = 0;
	if(_d_mutex_)
		r = acquire_mutex(_d_mutex_, hlock);
	return r;
}
void cVolume::unlock(HMUTEX hlock) {
	if(_d_mutex_)
		_d_mutex_->unlock(hlock);
}

void cVolume::msg_handler(_msg_t *p_msg) {
	switch(p_msg->msg) {
		case MSG_DRIVER_UNINIT: {
				iDriverBase *p_drv = (iDriverBase *)p_msg->data;
				if(p_drv == this) {
					HMUTEX hlock = lock();
					m_io_enable = false;
					unlock(hlock);
				}
			}
			break;
	}
}

_u8 *cVolume::alloc_nc_io_buffer(_u32 nblocks) {
	_u8 *r = 0;
	if(m_p_pma) {
		_u32 pgsz = m_p_pma->get_page_size();
		_u32 nbytes = (nblocks * DRIVER_HOST->_block_size_);
		_u32 npages = nbytes / pgsz;
		npages += (nbytes % pgsz)?1:0;
		if(npages > m_ncp_count) {
			release_nc_io_buffer();
			if((m_p_ncbuf = (_u8 *)m_p_pma->alloc(npages))) {
				m_ncp_count = npages;
				r = m_p_ncbuf;
			}
		} else
			r = m_p_ncbuf;
	}
	return r;
}

void cVolume::release_nc_io_buffer(void) {
	if(m_p_ncbuf) {
		if(m_p_pma) {
			m_p_pma->free(m_p_ncbuf, m_ncp_count);
			m_p_ncbuf = 0;
			m_ncp_count = 0;
		}
	}
}

_str_t cVolume::serial(void) {
	return (_str_t)_vol_ident_;
}

_str_t cVolume::name(void) {
	return (_str_t)DRIVER_IDENT;
}

bool cVolume::verify(_u32 idx, _pt_entry_t *p_pte) {
	bool r = false;
	iDriverRoot *p_droot = GET_DRIVER_ROOT();

	if(p_droot) {
		_driver_request_t dr;

		dr.flags = DRF_TYPE|DRF_INDEX|DRF_INTERFACE;
		dr.DRIVER_INTERFACE = I_VOLUME;
		dr.DRIVER_TYPE = DTYPE_VOL;
		dr.DRIVER_INDEX = 0;
		
		iVolume *p_vol = 0;
		while((p_vol = (iVolume *)p_droot->driver_request(&dr, DRIVER_HOST))) {
			if(p_vol->_vol_pidx_ && p_vol->_vol_pidx_ == idx) {
				if(p_pte->lba.first && p_pte->lba.count) {
					p_vol->_vol_offset_ = p_pte->lba.first;
					p_vol->_vol_size_ = p_pte->lba.count;
				} else {
					// remove volume
					iDriverRoot *p_droot = GET_DRIVER_ROOT();
					if(p_droot)
						p_droot->remove_driver(p_vol);
				}
				r = true;
				break;
			}

			dr.DRIVER_INDEX++;
		}
	}
	
	return r;
}

void cVolume::scan(void) {
	if(!_vol_pidx_ && DRIVER_HOST->DRIVER_CLASS == DCLASS_HDD && 
			DRIVER_HOST->DRIVER_TYPE == DTYPE_DEV && 
			DRIVER_HOST->DRIVER_MODE == DMODE_BLOCK) { // scan partition table
		iHeap *p_heap = (iHeap *)OBJECT_BY_INTERFACE(I_HEAP, RY_STANDALONE);
		if(p_heap) {
			// alloc memory for MBR
			_u8 *p_mbr = (_u8 *)p_heap->alloc(DRIVER_HOST->_block_size_);
			if(p_mbr) {
				mem_set(p_mbr, 0, DRIVER_HOST->_block_size_);
				// read MBR
				_u8 tmo = 3;
				while(vol_block_io(false, 0, 1, p_mbr) != 1 && tmo--);
				if(tmo) {
					_pt_entry_t *p_pt = (_pt_entry_t *)(p_mbr + PT_OFFSET);
					DBG("Scan patrition table on '%s' ...\r\n", DRIVER_IDENT);
					_u32 idx = 0;
					do {
						if(verify(idx+1, p_pt) == false) {
							if(p_pt->lba.first && p_pt->lba.count) {
								LOG("\t\t\t'%s' partition %d  %l MB %s\r\n", DRIVER_IDENT, idx+1, 
								   ((_u64)p_pt->lba.count * DRIVER_HOST->_block_size_)/(1024*1024), 
								   (p_pt->active==1) ? "bootable":" " );
								iVolume *p_vd = (iVolume *)OBJECT_BY_INTERFACE(I_VOLUME, RY_CLONE);
								if(p_vd) {
									p_vd->_vol_cidx_ = _vol_cidx_;
									p_vd->_vol_didx_ = _vol_didx_;
									p_vd->_vol_flags_ = VF_NOCACHE;
									p_vd->_vol_block_size_ = DRIVER_HOST->_block_size_;
									p_vd->_vol_offset_ = p_pt->lba.first;
									p_vd->_vol_size_ = p_pt->lba.count;
									p_vd->_vol_pidx_ = idx+1;
									p_vd->_vol_pvd_ = this;

									iDriverRoot *p_dr = GET_DRIVER_ROOT();
									if(p_dr)
										p_dr->register_driver(p_vd, DRIVER_HOST);
								}
							}
						}
						p_pt++;
						idx++;
					}while(idx < 4);
				}
				
				p_heap->free(p_mbr, DRIVER_HOST->_block_size_);
			}
		}
	}
}

void cVolume::vol_seek(_u64 offset, _handle_t _UNUSED_ handle) {
	m_current_offset = offset;
}

_u32 cVolume::vol_serial_io(bool write, _u8 *p_buffer, _u32 sz, _handle_t handle) {
	_u32 r = 0;
	r = vol_file_io(write, m_current_offset, p_buffer, sz, handle);
	m_current_offset += r;
	return r;
}

_u32 _vol_cache_cb(_u32 cmd, _cache_io_t *p_cio) {
	cVolume *p_obj = (cVolume *)p_cio->p_udata;
	return p_obj->vol_cache_ioctl(cmd, p_cio);
}

_u32 cVolume::vol_cache_ioctl(_u32 cmd, _cache_io_t *p_cio) {
	_u32 r = 0;
	if(p_cio->handle == m_cache_handle) {
		_driver_io_t dio;
		
		dio.lba = p_cio->bstart;
		dio.count = p_cio->bcount;
		dio.buffer = p_cio->buffer;
		
		switch(cmd) {
			case CIOCTL_READ:
				DRIVER_HOST->driver_ioctl(DIOCTL_BREAD, &dio);
				r = dio.result;
				DBG("'%s': read %d blocks lba:%l in cache\r\n", DRIVER_IDENT, r, dio.lba);
				break;
			case CIOCTL_WRITE:
				DRIVER_HOST->driver_ioctl(DIOCTL_BWRITE, &dio);
				r = dio.result;
				break;
		}
	}
	return r;
}

bool cVolume::vol_cache_alloc(void) {
	bool r = false;
	
	if(!(_vol_flags_ & VF_NOCACHE)) {
		if(!m_cache_handle) {
			if(m_p_cache) {
				_cache_info_t ci;
				ci.block_size = DRIVER_HOST->_block_size_;
				ci.chunk_size = DRIVER_HOST->_max_op_sectors_;
				switch(DRIVER_HOST->DRIVER_CLASS) {
					case DCLASS_HDD:
						ci.num_chunks = HDD_CACHE_CHUNKS;
						break;
					case DCLASS_CDROM:
						ci.num_chunks = CDR_CACHE_CHUNKS;
						break;
					case DCLASS_FDD:
						ci.num_chunks = FDD_CACHE_CHUNKS;
						break;
					case DCLASS_RDD:
						ci.num_chunks = RDD_CACHE_CHUNKS;
						break;
					case DCLASS_UMS:
						ci.num_chunks = UMS_CACHE_CHUNKS;
						break;
				}
				ci.p_ccb = _vol_cache_cb;
				ci.p_udata = this;
				LOG("\t\t\tcache size: %u KB\r\n",
					(ci.block_size * ci.chunk_size * ci.num_chunks)/1024);
				if((m_cache_handle = m_p_cache->alloc(&ci))) {
					DBG("'%s' cache ready\r\n", DRIVER_IDENT);
					r= true;
				}
			}
		} else
			r = true;
	}	
	return r;
}

_u32 cVolume::vol_block_io(bool write, _u64 block, _u32 count, _u8 *p_buffer) {
	_u32 r = 0;

	if(write && (_vol_flags_ & VF_READONLY))
		return r;

	DBG("'%s' block size = %d\r\n", DRIVER_IDENT, _vol_block_size_);
	DBG("'%s' offset = %u\r\n", DRIVER_IDENT, _vol_offset_);

	if(_vol_flags_ & VF_NOCACHE) {
_vol_direct_io_:
		_driver_io_t dio;
		
		dio.lba = block+_vol_offset_;
		dio.count = count;
		dio.buffer = p_buffer;
		if(_vol_flags_ & VF_HOSTCTRL) {
			DBG("NO cache: %s:lba=%u-->%s:lba=%u\r\n", DRIVER_IDENT,block,DRIVER_HOST->DRIVER_IDENT, dio.lba);
			DRIVER_HOST->driver_ioctl((write)?DIOCTL_BWRITE:DIOCTL_BREAD, &dio);
		} else {
			cVolume *p_pv = (cVolume *)_vol_pvd_;
			if(p_pv) {
				DBG("NO cache: %s:lba=%u-->%s:lba=%u\r\n", DRIVER_IDENT,block,p_pv->DRIVER_IDENT, dio.lba);
				p_pv->driver_ioctl((write)?DIOCTL_BWRITE:DIOCTL_BREAD, &dio);
			}
		}
		r = dio.result;
		if(write && !block)
			scan();
	} else { // cache request
		if(!vol_cache_alloc())
			goto _vol_direct_io_;
		
		_cache_io_t cio;
		cio.bstart = block+_vol_offset_;
		cio.boffset = 0;
		cio.bcount = count;
		cio.bsize = count * _vol_block_size_;
		cio.buffer = p_buffer;
		cio.handle = m_cache_handle;
		_u32 cio_bytes = m_p_cache->cache_ioctl((write)?CIOCTL_WRITE:CIOCTL_READ, &cio);
		DBG("'%s':%s %d cached bytes  lba=%u\r\n", DRIVER_IDENT, (write)?"write":"read", cio_bytes, cio.bstart);
		// return number of blocks
		r = cio_bytes / _vol_block_size_;
		r += (cio_bytes % _vol_block_size_)?1:0;
		if(write) {
			if(!block)
				scan();
		}
	}

	return r;
}

void cVolume::vol_calc_offset(_u64 offset, _u32 sz, _u64 *voffset, _u32 *nblocks, _u64 *fblock, _u16 *fboffset) {
	*nblocks = sz  / DRIVER_HOST->_block_size_;
	*nblocks += (sz % DRIVER_HOST->_block_size_) ? 1 : 0;
	*voffset = offset + (_vol_offset_ * DRIVER_HOST->_block_size_);
	*fblock = *voffset / DRIVER_HOST->_block_size_;
	*fboffset = *voffset % DRIVER_HOST->_block_size_;
}

_u32 cVolume::vol_file_io(bool write, _u64 offset, _u8 *p_buffer, _u32 sz, _handle_t handle) {
	_u32 r = 0;
	
	if(write && (_vol_flags_ & VF_READONLY))
		return r;

	_u64 voffset;
	_u32 nblocks;
	_u64 fblock;
	_u16 fboffset;

	vol_calc_offset(offset, sz, &voffset, &nblocks, &fblock, &fboffset);

	if(!(_vol_flags_ & VF_NOCACHE)) {
		if(vol_cache_alloc()) {
			_cache_io_t cio;
			cio.handle = m_cache_handle;
			cio.bstart = fblock;
			cio.boffset = fboffset;
			cio.bcount = nblocks;
			cio.bsize = sz;
			cio.buffer = p_buffer;
			// return number of bytes
			if((r = m_p_cache->cache_ioctl((write)?CIOCTL_WRITE:CIOCTL_READ, &cio))) {
				if(write) {
					if(!fblock)
						scan();
				}
			}
		}
	} else {
		cVolume *p_pv = (cVolume *)_vol_pvd_;
		if(p_pv)
			r = p_pv->vol_file_io(write, voffset, p_buffer, sz, handle);
		else {
			DBG("'%s': Non cacheable file io %d bytes, lba=%u\r\n", DRIVER_IDENT, sz, fblock);
			// non cacheable file I/O
			HMUTEX hlock = lock(m_io_lock);

			_u8 *buf = alloc_nc_io_buffer(nblocks);
			if(buf) {
				_u8 *ptr = buf;
				_u32 block = fblock;
				while(block < (fblock + nblocks)) {
					_u32 _nbio = 0;
					DRIVER_HOST->read(block, nblocks, ptr, &_nbio);
					block += _nbio;
					ptr += _nbio * DRIVER_HOST->_block_size_;
				}

				if(!write) {
					mem_cpy(p_buffer, buf + fboffset, sz);
					r = sz;
				} else {
					mem_cpy(buf + fboffset, p_buffer, sz);
					block = 0;
					while(block < (fblock + nblocks)) {
						_u32 _nbio = 0;
						DRIVER_HOST->write(block, nblocks, ptr, &_nbio);
						block += _nbio;
						r += _nbio * DRIVER_HOST->_block_size_;
						ptr += _nbio * DRIVER_HOST->_block_size_;
					}
				}
			} else
				LOG("'%s' failed to alloc I/O buffer\r\n", DRIVER_IDENT);			

			unlock(hlock);
		}
	}
	return r;
}

void cVolume::driver_ioctl(_u32 cmd, _driver_io_t *p_dio) {
	m_io_lock = lock(m_io_lock);
	p_dio->error = p_dio->result = 0;
	if(m_io_enable) {
		switch(cmd) {
			case DIOCTL_FSEEK:
				vol_seek(p_dio->offset, p_dio->handle);
				break;
			case DIOCTL_SREAD:
				p_dio->result = vol_serial_io(false, p_dio->buffer, p_dio->size, p_dio->handle);
				break;
			case DIOCTL_SWRITE:
				p_dio->result = vol_serial_io(true, p_dio->buffer, p_dio->size, p_dio->handle);
				break;
			case DIOCTL_BREAD:
				p_dio->result = vol_block_io(false,p_dio->lba, p_dio->count, p_dio->buffer);
				break;
			case DIOCTL_BWRITE:
				p_dio->result = vol_block_io(true,p_dio->lba, p_dio->count, p_dio->buffer);
				break;
			case DIOCTL_FREAD:
				p_dio->result = vol_file_io(false, p_dio->offset, p_dio->buffer, p_dio->size, p_dio->handle);
				break;
			case DIOCTL_FWRITE:
				p_dio->result = vol_file_io(true, p_dio->offset, p_dio->buffer, p_dio->size, p_dio->handle);
				break;
			default:
				p_dio->error = DIOCTL_ERRCMD;
		}
	}
	unlock(m_io_lock);
}

