#include "iBlockDevice.h"
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

class cRamDevice:public iRamDevice {
public:
	cRamDevice() {
		DRIVER_TYPE = DTYPE_DEV;
		DRIVER_MODE = DMODE_BLOCK;
		DRIVER_CLASS = DCLASS_RDD;
		_block_size_ = 512;
		_max_op_sectors_ = 64;
		_ramd_size_ = 0;
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}
	~cRamDevice() {
	}

	// iBase
	DECLARE_BASE(cRamDevice);
	DECLARE_INIT();
	DECLARE_DESTROY();
	
	DECLARE_DRIVER_INIT();
	DECLARE_DRIVER_UNINIT();

	void driver_ioctl(_u32 cmd, _driver_io_t *p_dio);

protected:
	iSystemLog	*m_p_syslog;
	_u8		*m_p_base;
	_u32 		m_npages;

	_u32 transfer(bool write, _u64 lba, _u32 count, _u8 *buffer);
	HMUTEX lock(HMUTEX hlock=0);
	void unlock(HMUTEX hlock);
};
IMPLEMENT_BASE(cRamDevice, "cRamDevice", 0x0001);
IMPLEMENT_INIT(cRamDevice, rbase) {
	REPOSITORY = rbase;
	INIT_MUTEX();
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	return true;
}
IMPLEMENT_DESTROY(cRamDevice) {
	RELEASE_MUTEX();
}
IMPLEMENT_DRIVER_INIT(cRamDevice, host) {
	bool r = false;
	if((DRIVER_HOST = host)) {
		_block_dev_size_ = _ramd_size_ / _block_size_;
		_snprintf(DRIVER_IDENT, sizeof(DRIVER_IDENT), "ramd-%d", _ramd_idx_);
		LOG("\t\t\t'%s': ram disk %u MB / %u sectors\r\n", DRIVER_IDENT, _ramd_size_ / (1024 * 1024), _block_dev_size_);
		iDriverRoot *p_droot = GET_DRIVER_ROOT();
		if(p_droot) {
			iPMA *p_pma = (iPMA *)OBJECT_BY_INTERFACE(I_PMA, RY_STANDALONE);
			if(p_pma) {
				m_npages = _ramd_size_ / p_pma->get_page_size();
				if((m_p_base = (_u8 *)p_pma->alloc(m_npages))) {
					iVolume *p_volume = (iVolume *)OBJECT_BY_INTERFACE(I_VOLUME, RY_CLONE);
					if(p_volume) {
						p_volume->_vol_cidx_ = 0;
						p_volume->_vol_didx_ = _ramd_idx_;
						p_volume->_vol_pvd_ = 0;
						p_volume->_vol_pidx_ = 0;
						p_volume->_vol_offset_ = 0;
						p_volume->_vol_size_ = _block_dev_size_;
						p_volume->_vol_flags_ = VF_HOSTCTRL;
						p_droot->register_driver(p_volume, this);
					}
					r = true;
				}
				RELEASE_OBJECT(p_pma);
			}
			RELEASE_OBJECT(p_droot);
		}
	}

	return r;
}
IMPLEMENT_DRIVER_UNINIT(cRamDevice) {
	iPMA *p_pma = (iPMA *)OBJECT_BY_INTERFACE(I_PMA, RY_STANDALONE);
	if(p_pma) {
		p_pma->free(m_p_base, m_npages);
		RELEASE_OBJECT(p_pma);
	}
}

HMUTEX cRamDevice::lock(HMUTEX hlock) {
	HMUTEX r = 0;

	if(_d_mutex_)
		r = acquire_mutex(_d_mutex_, hlock);

	return r;
}

void cRamDevice::unlock(HMUTEX hlock) {
	if(_d_mutex_)
		_d_mutex_->unlock(hlock);
}

_u32 cRamDevice::transfer(bool write, _u64 lba, _u32 count, _u8 *buffer) {
	_u32 r = 0;
	_u64 offset = lba * _block_size_;
	_u32 size = ((lba + count) <= _block_dev_size_) ? (count * _block_size_) 
							: ((_block_dev_size_ - lba) * _block_size_);

	if(lba < _block_dev_size_) {
		HMUTEX hlock = lock();

		if(write)
			mem_cpy(m_p_base + offset, buffer, size);
		else
			mem_cpy(buffer, m_p_base + offset, size);

		r = size / _block_size_;

		unlock(hlock);
	}

	return r;
}

void cRamDevice::driver_ioctl(_u32 cmd, _driver_io_t *p_dio) {
	switch(cmd) {
		case DIOCTL_BREAD:
			p_dio->result = transfer(false, p_dio->lba, p_dio->count, p_dio->buffer);
			break;
		case DIOCTL_BWRITE:
			p_dio->result = transfer(true, p_dio->lba, p_dio->count, p_dio->buffer);
			break;
		default:
			p_dio->result = DIOCTL_ERRCMD;
	}
}

