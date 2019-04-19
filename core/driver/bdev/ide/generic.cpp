// this driver is developed with help of OSDev.org
// http://wiki.osdev.org/IDE

#include "iBlockDevice.h"
#include "str.h"
#include "iSystemLog.h"
#include "ide.h"
#include "io_port.h"
#include "iCPUHost.h"
#include "iSyncObject.h"
#include "intdef.h"
#include "ctrldef.h"

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


#define POLLING_WAIT_CYCLE	100
 
// interrupts
#define INT_PRIMARY	INT_IDE_CH1
#define INT_SECONDARY	INT_IDE_CH2

// IRQ
#define IRQ_PRIMARY	IRQ_IDE_CH1
#define IRQ_SECONDARY	IRQ_IDE_CH2

class cGenericIDEController:public iIDEController {
public:
	cGenericIDEController() {
		DRIVER_TYPE = DTYPE_CTRL;
		DRIVER_MODE = DMODE_BLOCK;
		DRIVER_CLASS = DCLASS_HDC;
		ERROR_CODE = 0;
		_ctrl_type_[0]='I';_ctrl_type_[1]='D';_ctrl_type_[2]='E';_ctrl_type_[3]=0;
		m_p_channel[0] = m_p_channel[1] = 0;
		_ide_pci_index_ = 0xffffffff;
		m_p_event = 0;
		m_p_syslog = 0;
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}
	//virtual ~cGenericIDEController() {}

	// iBase
	DECLARE_BASE(cGenericIDEController);
	DECLARE_INIT();
	DECLARE_DESTROY();
	
	DECLARE_DRIVER_INIT();
	DECLARE_DRIVER_UNINIT();

	_u8 read(_u8 channel, _u8 reg);
	_u32 read(_u8 channel, _u8 *p_buffer, _u32 sz);
	void write(_u8 channel, _u8 reg, _u8 data);
	_u32 write(_u8 channel, _u8 *p_buffer, _u32 sz);
	bool polling(_u8 channel);
	void wait(_u8 channel);
	void irq_on(_u8 channe);
	void irq_off(_u8 channel);
	void wait_irq(_u8 channel, _u32 timeout=EVENT_TIMEOUT_INFINITE);

protected:
	iSystemLog 	*m_p_syslog;
	iIDEChannel	*m_p_channel[2];
	iEvent		*m_p_event;
	
	bool setup_channels(void);
	void reset(_u8 channel);
	void log_error(_u8 channel);
	void setup_irq(_u8 channel);
	friend void isr_primary(iCPU *p_cpu, void *p_cpu_state, void *p_data);
	friend void isr_secondary(iCPU *p_cpu, void *p_cpu_state, void *p_data);
	void irq_event(_u8 channel);
};

IMPLEMENT_BASE(cGenericIDEController, IDE_CTRL_NAME_GENERIC, 0x0001);
IMPLEMENT_INIT(cGenericIDEController, rbase) {
	REPOSITORY = rbase;
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	m_p_event = (iEvent *)OBJECT_BY_INTERFACE(I_EVENT, RY_CLONE);
	if(m_p_event)
		m_p_event->init();
	return true;
}
IMPLEMENT_DRIVER_INIT(cGenericIDEController, host) {
	DRIVER_HOST = host;
	_snprintf(DRIVER_IDENT, sizeof(DRIVER_IDENT), (_str_t)"IDE-%d", _ctrl_index_);
	return setup_channels();
}
IMPLEMENT_DRIVER_UNINIT(cGenericIDEController) {
	iCPUHost *p_cpu_host = (iCPUHost *)OBJECT_BY_INTERFACE(I_CPU_HOST, RY_STANDALONE);
	if(p_cpu_host) {
		p_cpu_host->set_isr(INT_PRIMARY, 0, 0);
		p_cpu_host->set_isr(INT_SECONDARY, 0, 0);
		RELEASE_OBJECT(p_cpu_host);
	}

	RELEASE_OBJECT(m_p_channel[ATA_PRIMARY]);
	RELEASE_OBJECT(m_p_channel[ATA_SECONDARY]);
}
IMPLEMENT_DESTROY(cGenericIDEController) {
	RELEASE_OBJECT(m_p_syslog);
	m_p_syslog = 0;
	RELEASE_OBJECT(m_p_event);
	m_p_event = 0;
}
	
void cGenericIDEController::irq_event(_u8 channel) {
	DBG("IDE IRQ(%d)\r\n", channel+IRQ_PRIMARY);
	if(m_p_event)
		m_p_event->set(1<<channel);
}

void isr_primary(iCPU *p_cpu, void *p_cpu_state, void *p_data) {
	cGenericIDEController *p_ctrl = (cGenericIDEController *)p_data;
	p_ctrl->irq_event(ATA_PRIMARY);
	p_cpu->interrupt(INT_PRIMARY, p_cpu_state);
}

void isr_secondary(iCPU *p_cpu, void *p_cpu_state, void *p_data) {
	cGenericIDEController *p_ctrl = (cGenericIDEController *)p_data;
	p_ctrl->irq_event(ATA_SECONDARY);
	p_cpu->interrupt(INT_SECONDARY, p_cpu_state);
}

void cGenericIDEController::setup_irq(_u8 channel) {
	iCPUHost *p_cpu_host = (iCPUHost *)OBJECT_BY_INTERFACE(I_CPU_HOST, RY_STANDALONE);
	if(p_cpu_host) {
		iCPU *p_cpu = p_cpu_host->get_boot_cpu();
		switch(channel) {
			case ATA_PRIMARY:
				p_cpu_host->set_irq(IRQ_PRIMARY, p_cpu->cpu_id(), INT_PRIMARY);
				p_cpu_host->set_isr(INT_PRIMARY, isr_primary, this);
				break;
			case ATA_SECONDARY:
				p_cpu_host->set_irq(IRQ_SECONDARY, p_cpu->cpu_id(), INT_SECONDARY);
				p_cpu_host->set_isr(INT_SECONDARY, isr_secondary, this);
				break;
		}

		RELEASE_OBJECT(p_cpu_host);
	}
}

bool cGenericIDEController::setup_channels(void) {
	bool r = false;

	if(DRIVER_HOST) {
		DBG("Init IDE channel %d\r\n", ATA_PRIMARY);
		// setup primary channel
		if((m_p_channel[ATA_PRIMARY] = (iIDEChannel *)OBJECT_BY_INTERFACE(I_IDE_CHANNEL, RY_CLONE))) {
			m_p_channel[ATA_PRIMARY]->_ch_index_    = ATA_PRIMARY;
			m_p_channel[ATA_PRIMARY]->_ch_reg_base_ = _ide_base_primary_; // Base
			m_p_channel[ATA_PRIMARY]->_ch_reg_ctrl_ = _ide_ctrl_primary_; // Ctrl
			m_p_channel[ATA_PRIMARY]->_ch_reg_bmide_= _ide_bus_master_; // Bus master IDE
			m_p_channel[ATA_PRIMARY]->_ch_ctrl_     = this;
			reset(ATA_PRIMARY);
			irq_off(ATA_PRIMARY);
			setup_irq(ATA_PRIMARY);
			m_p_channel[ATA_PRIMARY]->init();
		}

		DBG("Init IDE channel %d\r\n", ATA_SECONDARY);
		// setup secondary channel
		if((m_p_channel[ATA_SECONDARY] = (iIDEChannel *)OBJECT_BY_INTERFACE(I_IDE_CHANNEL, RY_CLONE))) {
			m_p_channel[ATA_SECONDARY]->_ch_index_    = ATA_SECONDARY;
			m_p_channel[ATA_SECONDARY]->_ch_reg_base_ = _ide_base_secondary_; // Base
			m_p_channel[ATA_SECONDARY]->_ch_reg_ctrl_ = _ide_ctrl_secondary_; // Ctrl
			m_p_channel[ATA_SECONDARY]->_ch_reg_bmide_= _ide_bus_master_ + 8; // Bus master IDE
			m_p_channel[ATA_SECONDARY]->_ch_ctrl_ 	  = this;
			reset(ATA_SECONDARY);
			irq_off(ATA_SECONDARY);
			setup_irq(ATA_SECONDARY);
			m_p_channel[ATA_SECONDARY]->init();
		}

		r = true;
	}

	return r;
}

void cGenericIDEController::log_error(_u8 channel) {
	if(m_p_channel[channel]) {
		_u8 err = read(channel, ATA_REG_ERROR);
		if(err == 0xff) 
			return;

		if(err & ATA_ER_AMNF)
			LOG("ATA(%d:%d:%d): 'No Address Mark Found'\r\n", 
				_ctrl_index_, channel, m_p_channel[channel]->_ch_ata_index_);
		if(err & (ATA_ER_TK0NF|ATA_ER_MCR|ATA_ER_MC))
			LOG("ATA(%d:%d:%d): 'No Media or Media Error'\r\n", 
				_ctrl_index_, channel, m_p_channel[channel]->_ch_ata_index_);
		if(err & ATA_ER_ABRT)
			LOG("ATA(%d:%d:%d): 'Command Aborted'\r\n", 
				_ctrl_index_, channel, m_p_channel[channel]->_ch_ata_index_);
		if(err & ATA_ER_IDNF)
			LOG("ATA(%d:%d:%d): 'ID mark not Found'\r\n", 
				_ctrl_index_, channel, m_p_channel[channel]->_ch_ata_index_);
		if(err & ATA_ER_UNC)
			LOG("ATA(%d:%d:%d): 'Uncorrectable Data Error'\r\n", 
				_ctrl_index_, channel, m_p_channel[channel]->_ch_ata_index_);
		if(err & ATA_ER_BBK)
			LOG("ATA(%d:%d:%d): 'Bad Sectors'\r\n", _ctrl_index_, channel, m_p_channel[channel]->_ch_ata_index_);
	}
}

bool cGenericIDEController::polling(_u8 channel) {
	bool r = false;

	if(m_p_channel[channel]) {
		for(_u8 i = 0; i < 5; i++)
			read(channel, ATA_REG_ALTSTATUS);

		wait(channel);

		_u8 state = read(channel, ATA_REG_STATUS);
		if(!(state & (ATA_SR_ERR|ATA_SR_DF)) && (state & ATA_SR_DRQ))
			r = true;
		else
			log_error(channel);
	}

	return r;
}

void cGenericIDEController::wait_irq(_u8 channel,_u32 timeout) {
	wait_event(m_p_event, 1<<channel, timeout);
}

void cGenericIDEController::wait(_u8 channel) {
	_u32 tmout=POLLING_WAIT_CYCLE;
	iCPU *p_cpu = get_cpu();
	while((read(channel, ATA_REG_STATUS) & ATA_SR_BSY) && tmout ) {
		if(p_cpu)
			p_cpu->switch_context();
		tmout--;
	}
}

void cGenericIDEController::reset(_u8 channel) {
	if(m_p_channel[channel]) {
		outportb(m_p_channel[channel]->_ch_reg_ctrl_ + 2, 0x0c);
		outportb(m_p_channel[channel]->_ch_reg_ctrl_ + 2, 0x08);
		wait(channel);
	}
}

_u8 cGenericIDEController::read(_u8 channel, _u8 reg) {
	_u8 r = 0;

	if(m_p_channel[channel]) {
		if(reg > 0x07 && reg < 0x0C)
			write(channel, (_u8)ATA_REG_CONTROL, (_u8)(0x80 | m_p_channel[channel]->_ch_int_));
		if(reg < 0x08)
			r = inportb(m_p_channel[channel]->_ch_reg_base_  + reg - 0x00);
		else if(reg < 0x0C)
			r = inportb(m_p_channel[channel]->_ch_reg_base_  + reg - 0x06);
		else if(reg < 0x0E)
			r = inportb(m_p_channel[channel]->_ch_reg_ctrl_  + reg - 0x0A);
		else if(reg < 0x16)
			r = inportb(m_p_channel[channel]->_ch_reg_bmide_ + reg - 0x0E);
		if(reg > 0x07 && reg < 0x0C)
			write(channel, (_u8)ATA_REG_CONTROL, m_p_channel[channel]->_ch_int_);
	}

	return r;
}

void cGenericIDEController::write(_u8 channel, _u8 reg, _u8 data) {
	if(m_p_channel[channel]) {
		if(reg > 0x07 && reg < 0x0C)
			write(channel, (_u8)ATA_REG_CONTROL, (_u8)(0x80 | m_p_channel[channel]->_ch_int_));
		if(reg < 0x08)
			outportb(m_p_channel[channel]->_ch_reg_base_  + reg - 0x00, data);
		else if(reg < 0x0C)
			outportb(m_p_channel[channel]->_ch_reg_base_  + reg - 0x06, data);
		else if(reg < 0x0E)
			outportb(m_p_channel[channel]->_ch_reg_ctrl_  + reg - 0x0A, data);
		else if(reg < 0x16)
			outportb(m_p_channel[channel]->_ch_reg_bmide_ + reg - 0x0E, data);
		if(reg > 0x07 && reg < 0x0C)
			write(channel, (_u8)ATA_REG_CONTROL, m_p_channel[channel]->_ch_int_);
	}
}

_u32 cGenericIDEController::read(_u8 channel, _u8 *p_buffer, _u32 sz) {
	_u32 r = 0;
	
	if(m_p_channel[channel]) {
		_u32 *p = (_u32 *)p_buffer;
		_u32 _sz = sz / sizeof(_u32);
		_u32 _r = 0;
		bool irqs = enable_interrupts(false);
		while(_r < _sz) {
			*(p + _r) = inportl(m_p_channel[channel]->_ch_reg_base_  +  ATA_REG_DATA);
			_r++;
		}
		r = _r*sizeof(_u32);
		enable_interrupts(irqs);
	}

	return r;
}

_u32 cGenericIDEController::write(_u8 channel, _u8 *p_buffer, _u32 sz) {
	_u32 r = 0;
	
	if(m_p_channel[channel]) {
		_u32 *p = (_u32 *)p_buffer;
		_u32 _sz = sz / sizeof(_u32);
		_u32 _r = 0;
		bool irqs = enable_interrupts(false);
		while(_r < _sz) {
			 outportl(m_p_channel[channel]->_ch_reg_base_  +  ATA_REG_DATA, *(p + _r));
			_r++;
		}
		r = _r*sizeof(_u32);
		enable_interrupts(irqs);
	}

	return r;
}

void cGenericIDEController::irq_on(_u8 channel) {
	if(m_p_channel[channel])
		write(channel, ATA_REG_CONTROL, (m_p_channel[channel]->_ch_int_ &= ~2));
}

void cGenericIDEController::irq_off(_u8 channel) {
	if(m_p_channel[channel])
		write(channel, ATA_REG_CONTROL, (m_p_channel[channel]->_ch_int_ |= 2));
}

