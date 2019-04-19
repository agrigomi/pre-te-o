// this driver is developed with help of OSDev.org
// http://wiki.osdev.org/IDE

#include "iBlockDevice.h"
#include "iSystemLog.h"
#include "ide.h"
#include "str.h"
#include "iMemory.h"

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

class cIDEDevice:public iIDEDevice {
public:
	cIDEDevice() {
		DRIVER_TYPE = DTYPE_DEV;
		DRIVER_MODE = DMODE_BLOCK;
		ERROR_CODE = 0;
		m_p_syslog = 0;
		_ata_channel_ = 0;
		_ata_size_ = 0;
		_ata_type_ = IDE_ATA;
		_ata_index_ = ATA_MASTER;
		_max_op_sectors_ = 256;
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}
	//virtual ~cIDEDevice() {}

	// iBase
	DECLARE_BASE(cIDEDevice);
	DECLARE_INIT();
	DECLARE_DESTROY();
	
	DECLARE_DRIVER_INIT();
	DECLARE_DRIVER_UNINIT();
	
	void driver_ioctl(_u32 cmd, _driver_io_t *p_dio);

protected:
	iSystemLog	*m_p_syslog;
	_u32 ide_read(_u64 block, _u32 count, _u8 *p_buffer);
	_u32 ide_write(_u64 block, _u32 count, _u8 *p_data);
	void ide_eject(void);
};

IMPLEMENT_BASE(cIDEDevice, "cIDEDevice", 0x0001);
IMPLEMENT_INIT(cIDEDevice, rbase) {
	REPOSITORY = rbase;
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	return true;
}
IMPLEMENT_DRIVER_INIT(cIDEDevice, host) {
	bool r = false;

	DRIVER_HOST = host;
	if(DRIVER_HOST && 
			str_cmp((_str_t)(DRIVER_HOST->interface_name()), (_str_t)I_IDE_CONTROLLER) == 0 &&
			_ata_channel_ &&
			str_cmp((_str_t)(_ata_channel_->interface_name()), (_str_t)I_IDE_CHANNEL) == 0) {
		_block_dev_ = _ata_index_;
		_block_dev_size_ = _ata_size_;
		if(_ata_type_ == IDE_ATA) {
			DRIVER_CLASS = DCLASS_HDD;
			_block_size_ = 512;
			_snprintf(DRIVER_IDENT, sizeof(DRIVER_IDENT), "ata-%d.%d.%d", _ata_channel_->_ch_ctrl_->_ctrl_index_,
											_ata_channel_->_ch_index_,
											_ata_index_);
			LOG("\t\t\t'%s': %u MB\r\n", DRIVER_IDENT,(_block_dev_size_ * _block_size_) / (1024 * 1024));
		} else if(_ata_type_ == IDE_ATAPI) {
			DRIVER_CLASS = DCLASS_CDROM;
			_block_size_ = 2048;
			_snprintf(DRIVER_IDENT, sizeof(DRIVER_IDENT),"atapi-%d.%d.%d", _ata_channel_->_ch_ctrl_->_ctrl_index_,
											_ata_channel_->_ch_index_,
											_ata_index_);
		}
		
		static _u8 _ide_vol_idx_ = 0;

		iDriverRoot *p_droot = GET_DRIVER_ROOT();
		if(p_droot) {
			iVolume *p_volume = (iVolume *)OBJECT_BY_INTERFACE(I_VOLUME, RY_CLONE);
			if(p_volume) {
				p_volume->_vol_cidx_ = DRIVER_HOST->_ctrl_index_;
				p_volume->_vol_didx_ = _ide_vol_idx_;
				p_volume->_vol_pidx_ = 0;
				p_volume->_vol_pvd_ = 0;
				p_volume->_vol_offset_ = 0;
				p_volume->_vol_size_ = _block_dev_size_;
				p_volume->_vol_flags_= VF_HOSTCTRL;
				p_droot->register_driver(p_volume, this);
				_ide_vol_idx_++;
			}
		}
		
		r = true;
	}
	
	return r;
}
IMPLEMENT_DRIVER_UNINIT(cIDEDevice) {
}
IMPLEMENT_DESTROY(cIDEDevice) {
	RELEASE_OBJECT(m_p_syslog);
}

_u32 cIDEDevice::ide_read(_u64 block, _u32 count, _u8 *p_buffer) {
	_u32 r = 0;

	if(_ata_channel_) {
		HMUTEX _hlock = _ata_channel_->lock();
		_u32 _r = _ata_channel_->read_sector(_ata_index_, block, p_buffer, count, _hlock);
		r = _r / _block_size_;
		_ata_channel_->unlock(_hlock);
	}

	return r;
}

_u32 cIDEDevice::ide_write(_u64 block, _u32 count, _u8 *p_data) {
	_u32 r = 0;

	if(_ata_channel_) {
		HMUTEX _hlock = _ata_channel_->lock();
		_u32 _r = _ata_channel_->write_sector(_ata_index_, block, p_data, count, _hlock);
		r = _r / _block_size_;
		_ata_channel_->unlock(_hlock);
	}

	return r;
}

void cIDEDevice::ide_eject(void) {
	if(_ata_type_ == IDE_ATAPI) 
		_ata_channel_->eject(_ata_index_);
}

void cIDEDevice::driver_ioctl(_u32 cmd, _driver_io_t *p_dio) {
	switch(cmd) {
		case DIOCTL_BREAD:
			p_dio->result = ide_read(p_dio->lba, p_dio->count, p_dio->buffer);
			break;
		case DIOCTL_BWRITE:
			p_dio->result = ide_write(p_dio->lba, p_dio->count, p_dio->buffer);
			break;
		case DIOCTL_EJECT:
			ide_eject();
			break;
		default:
			p_dio->result = DIOCTL_ERRCMD;
	}
}
