#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
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

#define FDEVICE	(_str_t)"../../pre-te-o.img"

_u8 *map_file(_str_t name,_u64 *sz, _u8 *p_fd) {
	_u8 *r = NULL;
	int fd = open(name,O_RDWR);
	unsigned long len;
	
	if(fd != -1) {
		len = (_u64)lseek(fd,0,SEEK_END);
		lseek(fd,0,SEEK_SET);
		r = (_u8 *)mmap(NULL,len,PROT_WRITE,MAP_SHARED,fd,0);
		*sz = len;
		*p_fd = fd;
	}
	
	return r;
}

class cFDevice:public iDriverBase {
public:
	cFDevice() {
		DRIVER_TYPE = DTYPE_DEV;
		DRIVER_MODE = DMODE_BLOCK;
		DRIVER_CLASS = DCLASS_FDD;
		_block_size_ = 512;
		_max_op_sectors_ = 64;
		str_cpy(DRIVER_IDENT, (_str_t)"fdd-0", sizeof(DRIVER_IDENT));
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}
	~cFDevice() {}


	// iBase
	DECLARE_BASE(cFDevice);
	DECLARE_INIT();
	DECLARE_DESTROY();
	
	DECLARE_DRIVER_INIT();
	DECLARE_DRIVER_UNINIT();

	void driver_ioctl(_u32 cmd, _driver_io_t *p_dio);

protected:
	iSystemLog	*m_p_syslog;
	_u8		*m_p_base;
	_u8		m_hdev;
	_u64		m_dev_sz;
	
	_u32 transfer(bool write, _u64 lba, _u32 count, _u8 *buffer);
	HMUTEX lock(HMUTEX hlock=0);
	void unlock(HMUTEX hlock);
};

IMPLEMENT_BASE(cFDevice, "cFDevice", 0x0001);
IMPLEMENT_INIT(cFDevice, rbase) {
	REPOSITORY = rbase;
	INIT_MUTEX();
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	return true;
}
IMPLEMENT_DESTROY(cFDevice) {
	RELEASE_MUTEX();
}
IMPLEMENT_DRIVER_INIT(cFDevice, host) {
	bool r = false;
	if((DRIVER_HOST = host)) {
		if((m_p_base = map_file(FDEVICE, &m_dev_sz, &m_hdev))) {
			_block_dev_size_ = m_dev_sz / _block_size_;
			iDriverRoot *p_droot = GET_DRIVER_ROOT();
			if(p_droot) {
				iVolume *p_volume = (iVolume *)OBJECT_BY_INTERFACE(I_VOLUME, RY_CLONE);
				if(p_volume) {
					p_volume->_vol_cidx_ = 0;
					p_volume->_vol_didx_ = 1;
					p_volume->_vol_pvd_ = 0;
					p_volume->_vol_pidx_ = 0;
					p_volume->_vol_offset_ = 0;
					p_volume->_vol_size_ = _block_dev_size_;
					p_volume->_vol_flags_ = VF_HOSTCTRL;
					p_droot->register_driver(p_volume, this);
				}
			}
			r = true;
		}
	}

	return r;
}

IMPLEMENT_DRIVER_UNINIT(cFDevice) {
	munmap(m_p_base, m_dev_sz);
	close(m_hdev);
}

HMUTEX cFDevice::lock(HMUTEX hlock) {
	HMUTEX r = 0;

	if(_d_mutex_)
		r = acquire_mutex(_d_mutex_, hlock);

	return r;
}

void cFDevice::unlock(HMUTEX hlock) {
	if(_d_mutex_)
		_d_mutex_->unlock(hlock);
}

_u32 cFDevice::transfer(bool write, _u64 lba, _u32 count, _u8 *buffer) {
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

void cFDevice::driver_ioctl(_u32 cmd, _driver_io_t *p_dio) {
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

