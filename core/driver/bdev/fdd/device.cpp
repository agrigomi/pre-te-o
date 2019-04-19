#include "iBlockDevice.h"
#include "str.h"
#include "iSystemLog.h"
#include "io_port.h"
#include "iSyncObject.h"
#include "intdef.h"
#include "floppy.h"
#include "iScheduler.h"
#include "iDMA.h"
#include "iTimer.h"

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

#define CONTROLLER	((iFDController *)DRIVER_HOST)

class cFDDevice:public iFDDevice {
public:
	cFDDevice() {
		DRIVER_TYPE = DTYPE_DEV;
		DRIVER_MODE = DMODE_BLOCK;
		DRIVER_CLASS = DCLASS_FDD;
		ERROR_CODE = 0;
		m_p_dma = 0;
		m_p_timer = 0;
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}

	//virtual ~cFDDevice() {}

	// iBase
	DECLARE_BASE(cFDDevice);
	DECLARE_INIT();
	DECLARE_DESTROY();
	
	DECLARE_DRIVER_INIT();
	DECLARE_DRIVER_UNINIT();
	
	void driver_ioctl(_u32 cmd, _driver_io_t *p_dio);

protected:
	iSystemLog 	*m_p_syslog;
	iDMA		*m_p_dma;
	iTimer		*m_p_timer;
	bool		m_motor_on;
	bool		m_reset;
	HTIMER		m_htimer;
	_u8		m_status[7];
	_u8		m_state_sz;
	_u8		m_track;
	_u8		m_sr0;
	_u8		m_motor_timeout;

	void block2hts(_u32 block,_u32 *head,_u32 *track,_u32 *sector);
	void motor_on(void);
	void motor_off(void);
	bool command(_u8 cmd, _u32 timeout=300);
	bool wait_complete(_u32 timeout, bool sensei);
	_u32 transfer(bool write, _u64 block, _u32 count, _u8 *p_buffer);
	void reset(void);
	void recalibrate(void);
	bool seek(_u8 track);
	bool get_byte(_u8 *p_buffer, _u32 timeout=300);
	friend void cbfd_timer(void *p);
	void motor_timeout(void);
};

IMPLEMENT_BASE(cFDDevice, "cFDDevice", 0x0001);
IMPLEMENT_INIT(cFDDevice, rbase) {
	REPOSITORY = rbase;
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	return true;
}

void cbfd_timer(void *p) {
	cFDDevice *p_obj = (cFDDevice *)p;
	p_obj->motor_timeout();
}

IMPLEMENT_DRIVER_INIT(cFDDevice, host) {
	bool r = false;
	DRIVER_HOST = host;

	if(DRIVER_HOST) {
		_snprintf(DRIVER_IDENT, sizeof(DRIVER_IDENT), "fdd-%d", _fdd_index_);
		LOG("\t\t\t'%s': %s\r\n", DRIVER_IDENT, _fdd_ident_);
		_block_size_ = 512;
		_block_dev_ = _fdd_index_;
		_block_dev_size_ = _fdd_sectors_;
		_max_op_sectors_ = _fdd_spt_;
		m_motor_on = false;
		m_reset = true;
		m_track = 0xff;

		iDriverRoot *p_droot = GET_DRIVER_ROOT();
		if(p_droot) {
			_driver_request_t dr;
			dr.flags = DRF_INTERFACE;
			dr.DRIVER_INTERFACE = I_DMA;
			m_p_dma = (iDMA *)p_droot->driver_request(&dr);
			
			dr.DRIVER_INTERFACE = I_TIMER;
			m_p_timer = (iTimer *)p_droot->driver_request(&dr);
			
			if(m_p_timer && m_p_dma) {
				m_htimer = m_p_timer->create(1000, TF_SUSPEND|TF_PERIODIC, cbfd_timer, this);
				iVolume *p_volume = (iVolume *)OBJECT_BY_INTERFACE(I_VOLUME, RY_CLONE);
				if(p_volume) {
					p_volume->_vol_cidx_ = DRIVER_HOST->_ctrl_index_;
					p_volume->_vol_didx_ = _fdd_index_;
					p_volume->_vol_pidx_ = 0;
					p_volume->_vol_pvd_ = 0;
					p_volume->_vol_offset_ = 0;
					p_volume->_vol_size_ = _block_dev_size_;
					p_volume->_vol_flags_= VF_HOSTCTRL;
					p_droot->register_driver(p_volume, this);
				}
				r = true;
			} else {
				DBG("failed to init '%s' driver\r\n", DRIVER_IDENT);
			}
		}
	}

	return r;
}
IMPLEMENT_DRIVER_UNINIT(cFDDevice) {
	if(m_p_timer)
		m_p_timer->remove(m_htimer);
}
IMPLEMENT_DESTROY(cFDDevice) {
	RELEASE_OBJECT(m_p_syslog);
	m_p_syslog = 0;
}

void cFDDevice::motor_on(void) {
	if(!m_motor_on) {
		_fdc_dor_t dor;
		dor.value = CONTROLLER->read_reg(DIGITAL_OUTPUT_REGISTER);
		dor.value |= (0x10 << _fdd_index_);
		CONTROLLER->write_reg(DIGITAL_OUTPUT_REGISTER, dor.value);
		m_motor_on = true;
		sleep_systhread(500);
		m_p_timer->resume(m_htimer);
	}
	m_motor_timeout = 3;
}

void cFDDevice::motor_off(void) {
	m_motor_timeout = 0;
}

void cFDDevice::motor_timeout(void) {
	if(m_motor_on) {
		if(!m_motor_timeout) {
			_fdc_dor_t dor;
			dor.value = CONTROLLER->read_reg(DIGITAL_OUTPUT_REGISTER);
			dor.value &= ~(0x10 << _fdd_index_);
			CONTROLLER->write_reg(DIGITAL_OUTPUT_REGISTER, dor.value);
			m_motor_on = false;
			m_p_timer->suspend(m_htimer);
			DBG("FDD: '%s' motor OFF\r\n", DRIVER_IDENT);
		} else
			m_motor_timeout--;
	}
}

void cFDDevice::block2hts(_u32 block,_u32 *head,_u32 *track,_u32 *sector) {
	*track = block / (_fdd_heads_ * _fdd_spt_);
	*head = (block / _fdd_spt_) % _fdd_heads_;
	*sector = (block % _fdd_spt_)+1;
}

bool cFDDevice::command(_u8 cmd, _u32 timeout) {
	bool r = false;
	_u32 tmo = timeout;

	do {
		if((CONTROLLER->read_reg(MAIN_STATUS_REGISTER) & 0xc0) == 0x80)
			break;
		tmo--;
		sleep_systhread(1);
	}while(tmo);

	if(tmo) {
		CONTROLLER->write_reg(DATA_FIFO, cmd);
		r = true;
	}
#ifdef _DEBUG_
	else
		DBG("FDD: '%s' command %d timeout\r\n", DRIVER_IDENT, cmd);
#endif	

	return r;
}

bool cFDDevice::get_byte(_u8 *p_buffer, _u32 timeout) {
	bool r = false;
	_u32 tmo = timeout;

	do {
		if((CONTROLLER->read_reg(MAIN_STATUS_REGISTER) & 0xd0) == 0xd0)
			break;
		tmo--;
		sleep_systhread(1);
	}while(tmo);

	if(tmo) {
		*p_buffer = CONTROLLER->read_reg(DATA_FIFO);
		r = true;
	}
#ifdef _DEBUG_
	else
		DBG("FDD: '%s' get_byte timeout\r\n", DRIVER_IDENT);
#endif	

	return r;
}

bool cFDDevice::wait_complete(_u32 timeout, bool sensei) {
	bool r = false;

	r = CONTROLLER->wait_irq(timeout);
	m_state_sz=0;
	while(m_state_sz < 7 && (CONTROLLER->read_reg(MAIN_STATUS_REGISTER) & (1<<4))) {
		if(get_byte(&m_status[m_state_sz], 300))
			m_state_sz++;
	}

	if(sensei) {
		if((r = command(SENSE_INTERRUPT))) {
			get_byte(&m_sr0);
			get_byte(&m_track);
		}
	}

	return r;
}

void cFDDevice::reset(void) {
	motor_timeout();
	CONTROLLER->enable_irq(false);
	CONTROLLER->write_reg(DATARATE_SELECT_REGISTER, _fdd_data_rate_);
	CONTROLLER->enable_irq(true);
	command(SPECIFY);
	command(0xdf); // SRT = 3ms, HUT = 240ms
	command(0x02); // HLT = 16ms, ND = 0

	recalibrate();
	m_reset = false;
}

void cFDDevice::recalibrate(void) {
	motor_on();
	command(RECALIBRATE);
	command(0);
	wait_complete(300, true);
	motor_off();
}

bool cFDDevice::seek(_u8 track) {
	bool r = false;

	if(track != m_track) {
		command(SEEK);
		command(0);
		command(track);

		r = wait_complete(300, true);
		DBG("FDD: track %d\r\n", m_track);
	} else
		r = true;

	return r;
}

_u32 cFDDevice::transfer(bool write, _u64 block, _u32 count, _u8 *p_buffer) {
	_u32 r = 0;
	_u32 _block = block;
	_u32 head, track, sector;
	HMUTEX hlock = acquire_mutex(CONTROLLER->DRIVER_MUTEX);

	CONTROLLER->select(_fdd_index_);
	if(m_reset)
		reset();
	_u8 tmo=3;
	HDMA hdma=0;
	while(tmo-- && !(hdma = m_p_dma->open(FDC_DMA_CHANNEL, count * _block_size_)))
		sleep_systhread(1);

	if(hdma != INVALID_DMA_HANDLE) {
		_u8 *bdma = m_p_dma->buffer(hdma);
		motor_on();

		if(write)
			mem_cpy(bdma, p_buffer, count*_block_size_);

		while(r < count) {
			block2hts(_block, &head, &track, &sector);
			DBG("FDD: %s H=%d, T=%d, S=%d\r\n", (write) ? "write":"read", head, track, sector);
			if(seek(track)) {
				_u8 dma_mode = DMA_MODE_AUTO_RESET|DMA_MODE_SINGLE;
				if(write)
					dma_mode |= DMA_MODE_MEM_TO_DEV;
				else
					dma_mode |= DMA_MODE_DEV_TO_MEM;

				m_p_dma->start(hdma, dma_mode, _block_size_, bdma+(r*_block_size_));

				command((write)?(0xc0|WRITE_DATA):(0xe0|READ_DATA));
				command((head<<2)|_fdd_index_);
				command(track);
				command(head);
				command(sector);
				command(2);
				command(_fdd_spt_);
				command(_fdd_gap_);
				command(0xff);

				if(wait_complete(300, false)) {
					_block++;
					r++;
				} else
					break;
			} else
				break;
		}

		if(!write)
			mem_cpy(p_buffer, bdma, r*_block_size_);

		m_p_dma->close(hdma);
	}
	CONTROLLER->unlock(hlock);
	return r;
}

void cFDDevice::driver_ioctl(_u32 cmd, _driver_io_t *p_dio) {
	switch(cmd) {
		case DIOCTL_BREAD:
			p_dio->result = transfer(false,p_dio->lba, p_dio->count, p_dio->buffer);
			break;
		case DIOCTL_BWRITE:
			p_dio->result = transfer(true,p_dio->lba, p_dio->count, p_dio->buffer);
			break;
		default:
			p_dio->result = DIOCTL_ERRCMD;
	}
}
