// this driver is developped with help of OSDev.org
// http://wiki.osdev.org/AHCI

#include "iBlockDevice.h"
#include "iSystemLog.h"
#include "iScheduler.h"
#include "ahci.h"
#include "str.h"
#include "addr.h"
#include "iMemory.h"
#include "iSyncObject.h"

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

#define GET_CONTROLLER()	(iAHCIController *)DRIVER_HOST

#define CMD_TABLE_LEN	1024
#define CMD_FIS_LEN	128
#define PRD_TABLE_LEN	(CMD_TABLE_LEN - CMD_FIS_LEN)
#define MAX_PRDT_COUNT	(PRD_TABLE_LEN / sizeof(_prdt_entry_t))
#define MAX_OP_SECTORS	16

#define MODE_D2H	0 // read
#define MODE_H2D	1 // write

class cAHCIDevice:public iAHCIDevice {
public:
	cAHCIDevice() {
		DRIVER_TYPE = DTYPE_DEV;
		DRIVER_MODE = DMODE_BLOCK;
		ERROR_CODE = 0;
		m_p_syslog = 0;
		_block_size_ = 0;
		m_p_cmd_tbl = 0;
		m_slots = 0;
		DRIVER_CLASS = 0;
		m_port_ready = false;
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}

	//virtual ~cAHCIDevice() {
	//	CALL_DESTROY();
	//}
	
	// iBase
	DECLARE_BASE(cAHCIDevice);
	DECLARE_INIT();
	DECLARE_DESTROY();
	// iDriverBase
	DECLARE_DRIVER_INIT();
	DECLARE_DRIVER_UNINIT();
	
	void driver_ioctl(_u32 cmd, _driver_io_t *p_dio);
	
protected:
	iSystemLog 	*m_p_syslog;
	_hba_port_t	*m_p_port;
	bool		m_port_ready;
	_u8		*m_p_cmd_tbl;
	_u32		m_slots;

	bool init_port(void);
	bool ident(void);
	void cmd_start(void);
	void cmd_stop(void);
	bool wait_busy(void);
	_cmd_hdr_t *alloc_cmd_slot(_u32 *_slot);
	_cmd_tbl_t *fill_cmd_table(_cmd_hdr_t *p_cmdh, _u8 *p_buffer, _u16 count);
	_fis_reg_h2d_t *fill_cmd_fis(_cmd_tbl_t *p_cmdtbl, _u8 cmd, _u64 block, _u16 count);
	void free_cmd_slot(_u32 slot);
	bool exec(_u32 slot, _u32 timeout);
	bool hdd_command(_u8 cmd, _u8 mode, _u64 block, _u8 *p_buffer, _u16 count, _u32 timeout=1000);
	bool cdr_command(_u8 mode, _u64 block, _u8 *p_buffer, _u16 count, _u32 timeout=1000);
	_u32 sata_rw(_u8 mode, _u64 block, _u32 count, _u8 *p_buffer);
	void ahci_eject(void);
};

IMPLEMENT_BASE(cAHCIDevice, "cAHCIDevice", 0x0001);
IMPLEMENT_INIT(cAHCIDevice, rbase) {
	REPOSITORY = rbase;
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	INIT_MUTEX();
	return true;
}
IMPLEMENT_DRIVER_INIT(cAHCIDevice, host) {
	bool r = false;

	DRIVER_HOST = host;

	if(host) {
		m_p_port = (_hba_port_t *)_sata_port_;
		
		iAHCIController *p_ctrl = (iAHCIController *)host;
		_block_dev_ = _sata_index_;
		_block_size_ = 512;

		switch(m_p_port->sig) {
			case SATA_SIG_ATA:
				DRIVER_CLASS = DCLASS_HDD;
				_snprintf(DRIVER_IDENT, sizeof(DRIVER_IDENT), "sata-%d.%d", p_ctrl->_ctrl_index_, _sata_index_);
				if(init_port())
					r = ident();
				break;
			case SATA_SIG_ATAPI:
				DRIVER_CLASS = DCLASS_CDROM;
				_snprintf(DRIVER_IDENT, sizeof(DRIVER_IDENT), "sata-%d.%d", p_ctrl->_ctrl_index_, _sata_index_);
				if(init_port())
					r = ident();
				break;
			case SATA_SIG_SEMB:
				_snprintf(DRIVER_IDENT, sizeof(DRIVER_IDENT), "semb-%d.%d", p_ctrl->_ctrl_index_, _sata_index_);
				r = true;
				break;
			case SATA_SIG_PM:
				_snprintf(DRIVER_IDENT, sizeof(DRIVER_IDENT), "pm-%d.%d", p_ctrl->_ctrl_index_, _sata_index_);
				r = true;
				break;
		}
		
		iDriverRoot *p_droot = GET_DRIVER_ROOT();
		if(p_droot && r) {
			iVolume *p_volume = (iVolume *)OBJECT_BY_INTERFACE(I_VOLUME, RY_CLONE);
			if(p_volume) {
				p_volume->_vol_cidx_ = DRIVER_HOST->_ctrl_index_;
				p_volume->_vol_didx_ = _sata_index_;
				p_volume->_vol_flags_= VF_HOSTCTRL;
				p_volume->_vol_pidx_ = 0;
				p_volume->_vol_pvd_ = 0;
				p_volume->_vol_offset_ = 0;
				p_volume->_vol_size_ = _block_dev_size_;
				p_droot->register_driver(p_volume, this);
			}
		}
	}

	return r;
}
IMPLEMENT_DRIVER_UNINIT(cAHCIDevice) {
	cmd_stop();
	if(m_p_cmd_tbl) {
		iPMA *p_pma = (iPMA *)OBJECT_BY_INTERFACE(I_PMA, RY_STANDALONE);
		if(p_pma) {
			_u16 page_size = p_pma->get_page_size();
			_u16 npages = (_sata_max_cmd_count_ * CMD_TABLE_LEN) / page_size;
			p_pma->free(m_p_cmd_tbl, npages);
			RELEASE_OBJECT(p_pma);
			m_p_cmd_tbl = 0;
			m_port_ready = false;
		}

	}
}
IMPLEMENT_DESTROY(cAHCIDevice) {
	RELEASE_MUTEX();
}

typedef enum {
	FIS_TYPE_REG_H2D	= 0x27,	// Register FIS - host to device
	FIS_TYPE_REG_D2H	= 0x34,	// Register FIS - device to host
	FIS_TYPE_DMA_ACT	= 0x39,	// DMA activate FIS - device to host
	FIS_TYPE_DMA_SETUP	= 0x41,	// DMA setup FIS - bidirectional
	FIS_TYPE_DATA		= 0x46,	// Data FIS - bidirectional
	FIS_TYPE_BIST		= 0x58,	// BIST activate FIS - bidirectional
	FIS_TYPE_PIO_SETUP	= 0x5F,	// PIO setup FIS - device to host
	FIS_TYPE_DEV_BITS	= 0xA1,	// Set device bits FIS - device to host
} FIS_TYPE;

bool cAHCIDevice::init_port(void) {
	if(!m_port_ready) {
		iPMA *p_pma = (iPMA *)OBJECT_BY_INTERFACE(I_PMA, RY_STANDALONE);
		if(p_pma) {
			_u16 page_size = p_pma->get_page_size();
			_u16 npages = (_sata_max_cmd_count_ * CMD_TABLE_LEN) / page_size;
			m_p_cmd_tbl = (_u8 *)p_pma->alloc(npages); // alloc command table
			if(m_p_cmd_tbl) {
				mem_set(m_p_cmd_tbl, 0, _sata_max_cmd_count_ * CMD_TABLE_LEN);
				DBG("'%s' alloc command table at 0x%h\r\n", DRIVER_IDENT, m_p_cmd_tbl);

				_u64_t addr;
				addr._ldw = m_p_port->clb;
				addr._hdw = m_p_port->clbu;

				_cmd_hdr_t *p_cmd_hdr = (_cmd_hdr_t *)p_to_k(addr._qw);
				addr._qw = k_to_p((_u64)m_p_cmd_tbl);

				for(_u32 i = 0; i < _sata_max_cmd_count_; i++) {
					p_cmd_hdr->ctba = addr._ldw;
					p_cmd_hdr->ctbau = addr._hdw;

					p_cmd_hdr++;
					addr._qw += CMD_TABLE_LEN;
				}

				_max_op_sectors_ = MAX_OP_SECTORS * MAX_PRDT_COUNT;
				DBG("'%s': max_op_sectors = %d\r\n", DRIVER_IDENT, _max_op_sectors_);

				cmd_start();
				m_port_ready = true;
			}
			RELEASE_OBJECT(p_pma);
		}
	}

	return m_port_ready;
}

_cmd_tbl_t *cAHCIDevice::fill_cmd_table(_cmd_hdr_t *p_cmdh, _u8 *p_buffer, _u16 count) {
	_u64_t addr;
	_u16 _count = (count) ? count : 1;
	
	// PRDT entries count
	p_cmdh->prdtl = (_u32)((_count-1)>>4)+1;

	// get pointer to command table
	addr._ldw = p_cmdh->ctba;
	addr._hdw = p_cmdh->ctbau;
	_cmd_tbl_t *p_cmdtbl = (_cmd_tbl_t *)p_to_k(addr._qw);
	mem_set((_u8 *)p_cmdtbl, 0, sizeof(_cmd_tbl_t)+(p_cmdh->prdtl-1)*sizeof(_prdt_entry_t));
	
	addr._qw = k_to_p((_u64)p_buffer);
	_u16 i = 0;

	// 8K bytes (16 sectors) per PRDT entry
	for(i = 0; i < p_cmdh->prdtl - 1; i++) {
		p_cmdtbl->prdt_entry[i].dba = addr._ldw;
		p_cmdtbl->prdt_entry[i].dbau= addr._hdw;
		p_cmdtbl->prdt_entry[i].dbc = 8192; // 8K
		p_cmdtbl->prdt_entry[i].i = 1;

		addr._qw += 8192;
		_count -= 16;
	}

	// last entry
	p_cmdtbl->prdt_entry[i].dba = addr._ldw;
	p_cmdtbl->prdt_entry[i].dbau= addr._hdw;
	p_cmdtbl->prdt_entry[i].i = 1;
	p_cmdtbl->prdt_entry[i].dbc = (_count << 9); // 512 bytes per sector

	return p_cmdtbl;
}

_fis_reg_h2d_t *cAHCIDevice::fill_cmd_fis(_cmd_tbl_t *p_cmdtbl, _u8 cmd, _u64 block, _u16 count) {
	_fis_reg_h2d_t *p_cmd_fis = (_fis_reg_h2d_t *)(&p_cmdtbl->cfis[0]);
	p_cmd_fis->fis_type = FIS_TYPE_REG_H2D;
	p_cmd_fis->c = 1; // command mode
	p_cmd_fis->command = cmd;

	if(count) {	
		_lba_t lba;
		lba._qw = block;

		p_cmd_fis->lba0 = (_u8)lba._ldw;
		p_cmd_fis->lba1 = (_u8)(lba._ldw>>8);
		p_cmd_fis->lba2 = (_u8)(lba._ldw>>16);
		p_cmd_fis->device = (1<<6); // LBA mode
		p_cmd_fis->lba3 = (_u8)(lba._ldw>>24);
		p_cmd_fis->lba4 = (_u8)lba._hdw;
		p_cmd_fis->lba5 = (_u8)(lba._hdw>>8);
	}

	p_cmd_fis->countl = (_u8)count;
	p_cmd_fis->counth = (_u8)(count>>8);

	return p_cmd_fis;
}

bool cAHCIDevice::exec(_u32 slot, _u32 timeout) {
	bool r = false;
	if(wait_busy()) {
		m_p_port->ci |= (1 << slot); // execute command

		// wait for completion
		_u32 tmo = timeout * 1000;
		iBlender *p_blender = get_blender();
		if(p_blender) {
			if(p_blender->is_running())
				tmo = timeout;
		}
		
		while((m_p_port->ci & (1 << slot)) && tmo) {
			tmo--;
			if(p_blender)
				p_blender->sleep_systhread(1);
		} 

		if(!(m_p_port->ci & (1 << slot)) && tmo) {
			r = true;
		} else {
			bool irqs = enable_interrupts(false);
			LOG("ERROR: '%s' I/O\r\n", DRIVER_IDENT);
			enable_interrupts(irqs);
		}
	} else {
		bool irqs = enable_interrupts(false);
		LOG("ERROR: '%s' not responding\r\n", DRIVER_IDENT);
		enable_interrupts(irqs);
	}
	
	return r;
}

bool cAHCIDevice::hdd_command(_u8 cmd, _u8 mode, _u64 block, _u8 *p_buffer, _u16 count, _u32 timeout) {
	bool r = false;
	_u32 _slot=0;

	if(count <= _max_op_sectors_) {
		_cmd_hdr_t *p_cmdh = alloc_cmd_slot(&_slot);
		if(p_cmdh) {
			p_cmdh->w = mode; // read|write

			_cmd_tbl_t *p_cmdtbl = fill_cmd_table(p_cmdh, p_buffer, count);
			
			p_cmdh->a = 0; // HDD command
			m_p_port->cmd |= HBAP_CMD_FRE; // enable to receive FIS
			
			fill_cmd_fis(p_cmdtbl, cmd, block, count);

			// fis len (cfl)
			p_cmdh->cfl = sizeof(_fis_reg_h2d_t)/sizeof(_u32);
		
			r = exec(_slot, timeout);
			free_cmd_slot(_slot); // free CMD slot
		}
	}

	return r;
}

bool cAHCIDevice::cdr_command(_u8 mode, _u64 block, _u8 *p_buffer, _u16 count, _u32 timeout) {
	bool r = false;
	_u32 _slot=0;

	_cmd_hdr_t *p_cmdh = alloc_cmd_slot(&_slot);
	if(p_cmdh && count <= _max_op_sectors_) {
		p_cmdh->w = mode; // read|write

		_cmd_tbl_t *p_cmdtbl = fill_cmd_table(p_cmdh, p_buffer, count);
		
		p_cmdh->a = 1; // ATAPI command
		m_p_port->cmd |= HBAP_CMD_FRE; // enable to receive FIS
		
		fill_cmd_fis(p_cmdtbl, SATA_CMD_PACKET, block, count);

		// fis len (cfl)
		p_cmdh->cfl = sizeof(_fis_reg_h2d_t)/sizeof(_u32);
		
		mem_set((_u8 *)p_cmdtbl->acmd, 0, SATA_PACKET_SIZE);
		p_cmdtbl->acmd[0] = (mode == MODE_H2D)?SATAPI_CMD_WRITE:SATAPI_CMD_READ;
		p_cmdtbl->acmd[2] = (block >> 24) & 0xFF;
		p_cmdtbl->acmd[3] = (block >> 16) & 0xFF;
		p_cmdtbl->acmd[4] = (block >> 8) & 0xFF;
		p_cmdtbl->acmd[5] = block & 0xff;
		p_cmdtbl->acmd[6] = (count >> 24) & 0xFF;
		p_cmdtbl->acmd[7] = (count >> 16) & 0xFF;
		p_cmdtbl->acmd[8] = (count >> 8) & 0xFF;
		p_cmdtbl->acmd[9] = count & 0xFF;
		
		r = exec(_slot, timeout);
		free_cmd_slot(_slot);
	}

	return r;
}

bool cAHCIDevice::ident(void) {
	bool r = false;
	static _u8 ident_pack[512];
	_u8 cmd = SATA_CMD_IDENTIFY;
	_u32 tmo = 3000;

	if(DRIVER_CLASS == DCLASS_CDROM) { 
		cmd = SATA_CMD_IDENTIFY_PACKET;
		tmo = 6000;
	}

	mem_set(ident_pack, 0, sizeof(ident_pack));
	DBG("'%s': Ident\r\n", DRIVER_IDENT);
	if((r = hdd_command(cmd, MODE_D2H, 0, ident_pack, 0, tmo))) {
		_u32 *p32 = (_u32 *)(&ident_pack[SATA_IDENT_COMMANDSETS]);
		_sata_command_sets_ = *p32;

		if(_sata_command_sets_ & (1<<26)) 
			// Device uses 48-Bit Addressing:
			p32 = (_u32 *)(&ident_pack[SATA_IDENT_MAX_LBA_EXT]);
		else 
			// Device uses CHS or 28-bit Addressing:
			p32 = (_u32 *)(&ident_pack[SATA_IDENT_MAX_LBA]);
		
		_block_dev_size_ = *p32;
		
		_u32 k = 0;
		for(k = 0; k < 40; k += 2) {
			_sata_model_[k] = ident_pack[SATA_IDENT_MODEL + k + 1];
			_sata_model_[k+1] = ident_pack[SATA_IDENT_MODEL + k];
		}
		_sata_model_[k+1] = 0;
		LOG("\t\t\t'%s': %s\r\n", DRIVER_IDENT, _sata_model_);
		LOG("\t\t\t'%s': %l sectors, %l MB\r\n", DRIVER_IDENT, _block_dev_size_, 
				(_block_dev_size_ * _block_size_) / (1024*1024));
	}

	return r;
}

void cAHCIDevice::cmd_start(void) {
	_u8 tmo = 10;

	while((m_p_port->cmd & HBAP_CMD_CR) && tmo--)
		switch_context();

	m_p_port->cmd |= HBAP_CMD_FRE;
	m_p_port->cmd |= HBAP_CMD_ST;
}

void cAHCIDevice::cmd_stop(void) {
	_u8 tmo = 10;
	
	m_p_port->cmd &= ~HBAP_CMD_ST;
	
	while((m_p_port->cmd & HBAP_CMD_FR) && (m_p_port->cmd & HBAP_CMD_CR) && tmo--)
		switch_context();
	
	m_p_port->cmd &= ~HBAP_CMD_FRE;
}

bool cAHCIDevice::wait_busy(void) {
	bool r = false;
	_u8 tmout=10;

	while((m_p_port->tfd & (SATA_DEV_BUSY | SATA_DEV_DRQ)) && tmout-- )
		sleep_systhread(1);

	if(tmout)
		r = true;

	return r;
}

_cmd_hdr_t *cAHCIDevice::alloc_cmd_slot(_u32 *_slot) {
	_cmd_hdr_t *r = 0;
	_u64_t addr;
	_u32 tmo = 300;
	
	while(!r && tmo) {
		_u32 slot = m_p_port->sact | m_p_port->ci | m_slots;
		_u32 mask = 1;
		HMUTEX hlock = acquire_mutex(DRIVER_MUTEX);
		for(_u8 i = 0; i < _sata_max_cmd_count_; i++) {
			if(!(mask & slot)) {
				addr._ldw = m_p_port->clb;
				addr._hdw = m_p_port->clbu;
				r = (_cmd_hdr_t *)p_to_k(addr._qw);
				r += i; // adjust pointer
				*_slot = i;
				m_slots |= (1 << i); // use this slot
				break;
			}

			mask <<= 1;
		}

		unlock(hlock);
		
		tmo--;
		if(!r)
			sleep_systhread(1);
	}

	return r;
}

void cAHCIDevice::free_cmd_slot(_u32 slot) {
	HMUTEX hlock = acquire_mutex(DRIVER_MUTEX);
	m_slots &= ~(1<<slot);
	unlock(hlock);
}

_u32 cAHCIDevice::sata_rw(_u8 mode, _u64 block, _u32 count, _u8 *p_buffer) {
	_u32 r = 0;
	bool cmdr = false;

	switch(mode) {
		case MODE_D2H: // read
			if(DRIVER_CLASS == DCLASS_HDD)
				cmdr = hdd_command(SATA_CMD_READ_DMA_EX, mode, block, p_buffer, count);
			else {
				if(DRIVER_CLASS == DCLASS_CDROM) {
					DBG("SATAPI('%s'): read block: %u count:%d\r\n", DRIVER_IDENT, block, count);
					cmdr = cdr_command(mode, block, p_buffer, count);
				}
			}
			break;
		case MODE_H2D: // write
			if(DRIVER_CLASS == DCLASS_CDROM) {
				DBG("SATAPI('%s'): write block: %u count:%d\r\n", DRIVER_IDENT, block, count);
				cmdr = cdr_command(mode, block, p_buffer, count);
			} else {
				if(DRIVER_CLASS == DCLASS_HDD) {
					if((cmdr = hdd_command(SATA_CMD_WRITE_DMA_EX, mode, block, p_buffer, count)))
						hdd_command(SATA_CMD_CACHE_FLUSH_EX, MODE_H2D, 0, 0, 0);
				}
			}
			break;
	}

	if(cmdr)
		r = count;

	return r;
}

void cAHCIDevice::ahci_eject(void) {
	//???
}

void cAHCIDevice::driver_ioctl(_u32 cmd, _driver_io_t *p_dio) {
	switch(cmd) {
		case DIOCTL_BREAD:
			p_dio->result = sata_rw(MODE_D2H, p_dio->lba, p_dio->count, p_dio->buffer);
			break;
		case DIOCTL_BWRITE:
			p_dio->result = sata_rw(MODE_H2D, p_dio->lba, p_dio->count, p_dio->buffer);
			break;
		case DIOCTL_EJECT:
			ahci_eject();
			break;
		default:
			p_dio->result = DIOCTL_ERRCMD;
	}
}

