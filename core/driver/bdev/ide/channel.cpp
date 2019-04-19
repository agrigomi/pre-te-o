// this driver is developed with help of OSDev.org
// http://wiki.osdev.org/IDE

#include "iBlockDevice.h"
#include "iScheduler.h"
#include "iSystemLog.h"
#include "str.h"
#include "ide.h"

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

#define MODE_CHS	0
#define MODE_LBA28	1
#define MODE_LBA48	2

// Directions:
#define ATA_READ      0x00
#define ATA_WRITE     0x01

class cIDEChannel:public iIDEChannel {
public:
	cIDEChannel() {
		ERROR_CODE = 0;
		_ch_reg_base_ = 0;
		_ch_reg_ctrl_ = 0;
		_ch_reg_bmide_ = 0;
		_ch_int_ = 0;
		_ch_ctrl_ = 0;
		_ch_ata_index_ = ATA_MASTER;
		m_p_mutex = 0;
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}
	//virtual ~cIDEChannel() {}

	// iBase
	DECLARE_BASE(cIDEChannel);
	DECLARE_INIT();
	DECLARE_DESTROY();

	bool init(void);
	HMUTEX lock(HMUTEX hlock=0);
	void unlock(HMUTEX hlock);
	_u32 read_sector(_u8 ata_idx, _u64 lba, _u8 *p_buffer, _u16 count, HMUTEX hlock=0);
	_u32 write_sector(_u8 ata_idx, _u64 lba, _u8 *p_data, _u16 count, HMUTEX hlock=0);
	void eject(_u8 ata_idx);

protected:
	iSystemLog 	*m_p_syslog;
	iMutex		*m_p_mutex;
	iIDEDevice	*m_p_dev[2];
	
	bool select(_u8 mode, _u8 index, _u8 head=0, HMUTEX hlock=0);
	_u32 ata_rw_sector(_u8 op,		// I/O operation 0-read/1-write
			_u8 ata_idx, 		// master or slave
			_u64 lba,		// LBA first sector number
			_u16 count, 		// sector count
			_u8 *p_buffer, 		// pointer to I/O buffer
			HMUTEX hlock=0);
	_u32 atapi_read_sector(_u8 ata_idx,	// master or slave
			_u64 lba,		// LBA first sector number
			_u16 count, 		// sector count
			_u8 *p_buffer, 		// pointer to I/O buffer
			HMUTEX hlock=0);
};

IMPLEMENT_BASE(cIDEChannel, "cIDEChannel", 0x0001);
IMPLEMENT_INIT(cIDEChannel, rbase) {
	REPOSITORY = rbase;
	m_p_mutex = (iMutex *)OBJECT_BY_INTERFACE(I_MUTEX, RY_CLONE);
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	return true;
}
IMPLEMENT_DESTROY(cIDEChannel) {
	RELEASE_OBJECT(m_p_mutex);
	RELEASE_OBJECT(m_p_syslog);
}

bool cIDEChannel::init(void) {
	bool r = false;

	if(_ch_ctrl_) {
		if(str_cmp((_str_t)_ch_ctrl_->interface_name(), (_str_t)I_IDE_CONTROLLER) == 0) {
			// setup devices
			for(_u8 didx = 0; didx < 2; didx++) {
				if(!select(0, didx))
					continue;
				LOG("\t\t\tATA(%d:%d:%d): ", _ch_ctrl_->_ctrl_index_, _ch_index_, _ch_ata_index_);
				_ch_ctrl_->write(_ch_index_, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
				_u8 st = _ch_ctrl_->read(_ch_index_, ATA_REG_STATUS);
				if(st) {
					bool ata = false;
					_u16 type = IDE_ATA;

					while(1) {
						st = _ch_ctrl_->read(_ch_index_, ATA_REG_STATUS);
						if(st & ATA_SR_ERR)
							// No ATA device
							break;
						if(!(st & ATA_SR_BSY) && (st & ATA_SR_DRQ)) {
							ata = true;
							break;
						}
					}

					if(!ata) {
						// test for IDE_ATAPI
						_u8 cl = _ch_ctrl_->read(_ch_index_, ATA_REG_LBA1);
						_u8 ch = _ch_ctrl_->read(_ch_index_, ATA_REG_LBA2);

						if(cl == 0x14 && ch == 0xeb)
							type = IDE_ATAPI;
						else if(cl == 0x69 && ch == 0x96)
							type = IDE_ATAPI;
						else {
							LOG("'Unknown device type'\r\n", "");
							continue;
						}

						_ch_ctrl_->write(_ch_index_, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
						_ch_ctrl_->wait(_ch_index_);
					}

					_u8 ident_pack[512];
					mem_set(ident_pack, 0, sizeof(ident_pack));
					_ch_ctrl_->read(_ch_index_, ident_pack, sizeof(ident_pack));

					if((m_p_dev[didx] = (iIDEDevice *)OBJECT_BY_INTERFACE(I_IDE_DEVICE, RY_CLONE))) {
						_u16 *p16;
						_u32 *p32;

						m_p_dev[didx]->_ata_channel_ = this;
						m_p_dev[didx]->_ata_index_ = didx;
						m_p_dev[didx]->_ata_type_ = type;
						p16 = (_u16 *)(&ident_pack[ATA_IDENT_DEVICETYPE]);
						m_p_dev[didx]->_ata_signature_ = *p16;
						p16 = (_u16 *)(&ident_pack[ATA_IDENT_CAPABILITIES]);
						m_p_dev[didx]->_ata_features_ = *p16;
						p32 = (_u32 *)(&ident_pack[ATA_IDENT_COMMANDSETS]);
						m_p_dev[didx]->_ata_command_sets_ = *p32;

						if(m_p_dev[didx]->_ata_command_sets_ & (1<<26)) {
							// Device uses 48-Bit Addressing:
							p32 = (_u32 *)(&ident_pack[ATA_IDENT_MAX_LBA_EXT]);
							m_p_dev[didx]->_ata_size_ = *p32;
						} else {
							// Device uses CHS or 28-bit Addressing:
							p32 = (_u32 *)(&ident_pack[ATA_IDENT_MAX_LBA]);
							m_p_dev[didx]->_ata_size_ = *p32;
						}

						_u32 k = 0;
						for(k = 0; k < 40; k += 2) {
							m_p_dev[didx]->_ata_model_[k] = ident_pack[ATA_IDENT_MODEL + k + 1];
							m_p_dev[didx]->_ata_model_[k+1] = ident_pack[ATA_IDENT_MODEL + k];
						}
						m_p_dev[didx]->_ata_model_[k+1] = 0;
						LOG("%s\r\n", m_p_dev[didx]->_ata_model_);
						iDriverRoot *p_droot = GET_DRIVER_ROOT();
						if(p_droot) { // attach driver to IDE controller
							p_droot->register_driver(m_p_dev[didx], _ch_ctrl_);
							RELEASE_OBJECT(p_droot);
						}
					} else
						LOG("Failed to create ATA object\r\n","");
				} else
					LOG("No device\r\n","");
			}
			r = true;
		}
	}
	
	return r;
}

HMUTEX cIDEChannel::lock(HMUTEX hlock) {
	HMUTEX r = 0;
	
	if(m_p_mutex)
		r = acquire_mutex(m_p_mutex, hlock);

	return r;
}

void cIDEChannel::unlock(HMUTEX hlock) {
	if(m_p_mutex)
		m_p_mutex->unlock(hlock);
}

bool cIDEChannel::select(_u8 mode, _u8 index, _u8 head, HMUTEX hlock) {
	bool r = false;
	
	if(_ch_ctrl_) {
		DBG("IDE channel %d select device %d\r\n", _ch_index_, index);
		_u8 mask = 0xe0;
		if(mode == MODE_CHS) 
			// use head number in CHS mode only
			mask = 0xa0|head;

		mask |= (index<<4);

		HMUTEX _hlock = lock(hlock);
		_ch_ctrl_->write(_ch_index_, ATA_REG_HDDEVSEL, mask);
		_ch_ctrl_->wait(_ch_index_);
		_ch_ata_index_ = index;
		r = true;
		unlock(_hlock);
	}

	return r;
}

_u32 cIDEChannel::ata_rw_sector(_u8 op,	// I/O operation 0-read/1-write
			_u8 ata_idx, 	// master or slave
			_u64 lba,	// LBA first sector number
			_u16 count, 	// sector count
			_u8 *p_buffer, 	// pointer to I/O buffer
			HMUTEX hlock) {
	_u32 r = 0;
	_u8 lba_mode = 0;
	_u8 lba_io[6];
	_u8 head, sect;
	_u8 cmd;
	_u16 cyl;
	bool dma = false;

	_ch_ctrl_->irq_off(_ch_index_);
	HMUTEX _hlock = lock(hlock);

	if(lba >= 0x10000000) {
		// use LBA48
		DBG("LBA48 lba=%l count=%d\r\n", lba, count);
		lba_mode = MODE_LBA48;
		lba_io[0] = (lba & 0x000000FF) >> 0;
		lba_io[1] = (lba & 0x0000FF00) >> 8;
		lba_io[2] = (lba & 0x00FF0000) >> 16;
		lba_io[3] = (lba & 0xFF000000) >> 24;
		lba_io[4] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
		lba_io[5] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
		head      = 0; // Lower 4-bits of HDDEVSEL are not used here.
	} else if(m_p_dev[ata_idx]->_ata_features_ & 0x200) {
		// Drive support LBA (use LBA28)
		DBG("LBA28 lba=%l count=%d\r\n", lba, count);
		lba_mode = MODE_LBA28;
		lba_io[0] = (lba & 0x000000FF) >> 0;
		lba_io[1] = (lba & 0x0000FF00) >> 8;
		lba_io[2] = (lba & 0x00FF0000) >> 16;
		lba_io[3] = 0;
		lba_io[4] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
		lba_io[5] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
		head      = 0; // Lower 4-bits of HDDEVSEL are not used here.
	} else {
		// use CHS
		DBG("CHS lba=%l count=%d\r\n", lba, count);
		lba_mode = MODE_CHS;
		lba_mode  = 0;
		sect      = (lba % 63) + 1;
		cyl       = (lba + 1  - sect) / (16 * 63);
		lba_io[0] = sect;
		lba_io[1] = (cyl >> 0) & 0xFF;
		lba_io[2] = (cyl >> 8) & 0xFF;
		lba_io[3] = 0;
		lba_io[4] = 0;
		lba_io[5] = 0;
		head      = (lba + 1  - sect) % (16 * 63) / (63); // Head number is written to HDDEVSEL lower 4-bits.
	}

	_ch_ctrl_->wait(_ch_index_);
	if(select(lba_mode, ata_idx, head, _hlock)) {
		if (lba_mode == MODE_LBA48) {
			_ch_ctrl_->write(_ch_index_, ATA_REG_SECCOUNT1,  (_u8)count>>8);
			_ch_ctrl_->write(_ch_index_, ATA_REG_LBA3,   lba_io[3]);
			_ch_ctrl_->write(_ch_index_, ATA_REG_LBA4,   lba_io[4]);
			_ch_ctrl_->write(_ch_index_, ATA_REG_LBA5,   lba_io[5]);
		}
		_ch_ctrl_->write(_ch_index_, ATA_REG_SECCOUNT0,  (_u8)count);
		_ch_ctrl_->write(_ch_index_, ATA_REG_LBA0,   lba_io[0]);
		_ch_ctrl_->write(_ch_index_, ATA_REG_LBA1,   lba_io[1]);
		_ch_ctrl_->write(_ch_index_, ATA_REG_LBA2,   lba_io[2]);

		if(lba_mode == MODE_CHS   && dma == false && op == 0) cmd = ATA_CMD_READ_PIO;
		if(lba_mode == MODE_LBA28 && dma == false && op == 0) cmd = ATA_CMD_READ_PIO;
		if(lba_mode == MODE_LBA48 && dma == false && op == 0) cmd = ATA_CMD_READ_PIO_EXT;   
		if(lba_mode == MODE_CHS   && dma == true  && op == 0) cmd = ATA_CMD_READ_DMA;
		if(lba_mode == MODE_LBA28 && dma == true  && op == 0) cmd = ATA_CMD_READ_DMA;
		if(lba_mode == MODE_LBA48 && dma == true  && op == 0) cmd = ATA_CMD_READ_DMA_EXT;
		if(lba_mode == MODE_CHS   && dma == false && op == 1) cmd = ATA_CMD_WRITE_PIO;
		if(lba_mode == MODE_LBA28 && dma == false && op == 1) cmd = ATA_CMD_WRITE_PIO;
		if(lba_mode == MODE_LBA48 && dma == false && op == 1) cmd = ATA_CMD_WRITE_PIO_EXT;
		if(lba_mode == MODE_CHS   && dma == true  && op == 1) cmd = ATA_CMD_WRITE_DMA;
		if(lba_mode == MODE_LBA28 && dma == true  && op == 1) cmd = ATA_CMD_WRITE_DMA;
		if(lba_mode == MODE_LBA48 && dma == true  && op == 1) cmd = ATA_CMD_WRITE_DMA_EXT;
		
		_ch_ctrl_->write(_ch_index_, ATA_REG_COMMAND, cmd);    // Send the Command.
		
		if(dma) {
			//???
		} else {
			// PIiO
			for(_u8 i = 0; i < count; i++) {
				if(_ch_ctrl_->polling(_ch_index_)) {
					if(op == 0) // PIO read
						r += _ch_ctrl_->read(_ch_index_, p_buffer + r, m_p_dev[ata_idx]->_block_size_);
					else // PIO write
						r += _ch_ctrl_->write(_ch_index_, p_buffer + r, m_p_dev[ata_idx]->_block_size_);
				} else
					break;
			}
			if(op == ATA_WRITE) { // cache flush
				switch(lba_mode) {
					case MODE_CHS:
					case MODE_LBA28:
						cmd = ATA_CMD_CACHE_FLUSH;
						break;
					case MODE_LBA48:
						cmd = ATA_CMD_CACHE_FLUSH_EXT;
						break;
				}
				_ch_ctrl_->write(_ch_index_, ATA_REG_COMMAND, cmd);
				_ch_ctrl_->polling(_ch_index_);
			}
		}
	}

	unlock(_hlock);
	return r;
}

_u32 cIDEChannel::atapi_read_sector(_u8 ata_idx,// master or slave
				_u64 lba,	// LBA first sector number
				_u16 count, 	// sector count
				_u8 *p_buffer, 	// pointer to I/O buffer
				HMUTEX hlock) {
	_u32 r = 0;
	_u8 atapi_packet[12];
	_u8 i;

	// Setup SCSI Packet:
	atapi_packet[ 0] = ATAPI_CMD_READ;
	atapi_packet[ 1] = 0x0;
	atapi_packet[ 2] = (lba >> 24) & 0xFF;
	atapi_packet[ 3] = (lba >> 16) & 0xFF;
	atapi_packet[ 4] = (lba >> 8) & 0xFF;
	atapi_packet[ 5] = (lba >> 0) & 0xFF;
	atapi_packet[ 6] = 0x0;
	atapi_packet[ 7] = 0x0;
	atapi_packet[ 8] = 0x0;
	atapi_packet[ 9] = (_u8)count;
	atapi_packet[10] = 0x0;
	atapi_packet[11] = 0x0;

	HMUTEX _hlock = lock(hlock);

	if(select(MODE_LBA48, ata_idx, 0, _hlock)) {
		// Wait 400 ns
		for(i = 0; i < 4; i++)
			_ch_ctrl_->read(_ch_index_, ATA_REG_ALTSTATUS);

		// enable IRQ
		_ch_ctrl_->irq_on(_ch_index_);
		
		// Inform the Controller that we use PIO mode:
		_ch_ctrl_->write(_ch_index_, ATA_REG_FEATURES, 0);

		// Tell the Controller the size of buffer:
		_ch_ctrl_->write(_ch_index_, ATA_REG_LBA1, (m_p_dev[ata_idx]->_block_size_ & 0xFF)); // Lower Byte of Sector Size.
		_ch_ctrl_->write(_ch_index_, ATA_REG_LBA2, (m_p_dev[ata_idx]->_block_size_ >> 8));   // Upper Byte of Sector Size.

		// Send the Packet Command:
		_ch_ctrl_->write(_ch_index_, ATA_REG_COMMAND, ATA_CMD_PACKET);
		if(_ch_ctrl_->polling(_ch_index_)) {
			_ch_ctrl_->write(_ch_index_, atapi_packet, 12);
			for(i = 0; i < count; i++) {
				_ch_ctrl_->wait_irq(_ch_index_, 300);
				if(_ch_ctrl_->polling(_ch_index_))
					r += _ch_ctrl_->read(_ch_index_, p_buffer+r, m_p_dev[ata_idx]->_block_size_);
				else
					break;
			}

			if(i == count) {
				_ch_ctrl_->wait_irq(_ch_index_, 300);
				while (_ch_ctrl_->read(_ch_index_, ATA_REG_STATUS) & (ATA_SR_BSY | ATA_SR_DRQ));
			}
		}

		// disable IRQ
		_ch_ctrl_->irq_off(_ch_index_);
	}

	unlock(_hlock);
	return r;
};

_u32 cIDEChannel::read_sector(_u8 ata_idx, _u64 lba, _u8 *p_buffer, _u16 count, HMUTEX hlock) {
	_u32 r = 0;
	if(m_p_dev[ata_idx]->_ata_type_ == IDE_ATA)
		r = ata_rw_sector(ATA_READ, ata_idx, lba, count, p_buffer, hlock);
	else if(m_p_dev[ata_idx]->_ata_type_ == IDE_ATAPI)
		r = atapi_read_sector(ata_idx, lba, count, p_buffer, hlock);
	return r;
}

_u32 cIDEChannel::write_sector(_u8 ata_idx, _u64 lba, _u8 *p_data, _u16 count, HMUTEX hlock) {
	_u32 r = 0;
	if(m_p_dev[ata_idx]->_ata_type_ == IDE_ATA)
		r = ata_rw_sector(ATA_WRITE, ata_idx, lba, count, p_data, hlock);
	return r;
}

void cIDEChannel::eject(_u8 ata_idx) {
	_u8 atapi_packet[12];
	_u8 i;

	// Setup SCSI Packet:
	atapi_packet[ 0] = ATAPI_CMD_EJECT;
	atapi_packet[ 1] = 0x00;
	atapi_packet[ 2] = 0x00;
	atapi_packet[ 3] = 0x00;
	atapi_packet[ 4] = 0x02;
	atapi_packet[ 5] = 0x00;
	atapi_packet[ 6] = 0x00;
	atapi_packet[ 7] = 0x00;
	atapi_packet[ 8] = 0x00;
	atapi_packet[ 9] = 0x00;
	atapi_packet[10] = 0x00;
	atapi_packet[11] = 0x00;

	HMUTEX _hlock = lock();

	if(select(MODE_LBA48, ata_idx, 0, _hlock)) {
		// Wait 400 ns
		for(i = 0; i < 4; i++)
			_ch_ctrl_->read(_ch_index_, ATA_REG_ALTSTATUS);
		
		// enable IRQ
		_ch_ctrl_->irq_on(_ch_index_);

		// Send the Packet Command:
		_ch_ctrl_->write(_ch_index_, ATA_REG_COMMAND, ATA_CMD_PACKET);
		if(_ch_ctrl_->polling(_ch_index_)) {
			_ch_ctrl_->write(_ch_index_, atapi_packet, 12);
			_ch_ctrl_->wait_irq(_ch_index_, 300);
			_ch_ctrl_->polling(_ch_index_);
		}

		// disable IRQ
		_ch_ctrl_->irq_off(_ch_index_);
	}
	
	unlock(_hlock);
}

