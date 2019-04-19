#include "iBlockDevice.h"
#include "str.h"
#include "iSystemLog.h"
#include "io_port.h"
#include "iCPUHost.h"
#include "iSyncObject.h"
#include "intdef.h"
#include "ahci.h"
#include "iMemory.h"
#include "addr.h"
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

#define MAX_CMD_COUNT		32
#define MAX_PORTS		32   // number of supported ports
#define CMDH_TBL_LEN		(MAX_CMD_COUNT * sizeof(_cmd_hdr_t)) // size of command header table per port
#define RECV_FIS_LEN		256  // size of received FIS

static _u16 _g_ahci_ctrl_index_=0;

class cGenericAHCIController:public iAHCIController {
public:
	cGenericAHCIController() {
		DRIVER_TYPE = DTYPE_CTRL;
		DRIVER_MODE = DMODE_BLOCK;
		DRIVER_CLASS = DCLASS_HDC;
		ERROR_CODE = 0;
		_ctrl_type_[0]='A';_ctrl_type_[1]='H';_ctrl_type_[2]='C';_ctrl_type_[3]='I';_ctrl_type_[4]=0;
		_ahci_pci_index_ = 0xffffffff;
		m_p_event = 0;
		m_p_syslog = 0;
		m_p_hba = 0;
		m_pi = 0;
		m_p_cmdh_tbl = 0;
		m_p_rfis_tbl = 0;
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}
	//virtual ~cGenericAHCIController() {}

	// iBase
	DECLARE_BASE(cGenericAHCIController);
	DECLARE_INIT();
	DECLARE_DESTROY();
	
	DECLARE_DRIVER_INIT();
	DECLARE_DRIVER_UNINIT();

protected:
	iSystemLog 	*m_p_syslog;
	iEvent		*m_p_event;
	_u8		m_int;
	_hba_mem_t	*m_p_hba;
	_u32		m_pi; // storage of last configured PI mask
	_u8		*m_p_cmdh_tbl; // command headers table for all ports
	_u8		*m_p_rfis_tbl; // received FIS table for all ports

	friend void isr_ahci(iCPU *p_cpu, void *p_cpu_state, void *p_data);
	void setup_irq(void);
	bool setup_mem(void);
	void setup_devices(void);
	void irq_handler(void);
	void wait_irq(_u32 timeout=EVENT_TIMEOUT_INFINITE);
	_u32 read(_u8 reg); // read HBA register
	void write(_u8 reg, _u32 value); // write HBA register
	void init_port(_hba_port_t *p_port, _u8 nport);
	void cmd_stop(_hba_port_t *p_port);
	void cmd_start(_hba_port_t *p_port);
};


IMPLEMENT_BASE(cGenericAHCIController, AHCI_CTRL_NAME_GENERIC, 0x0001);
IMPLEMENT_INIT(cGenericAHCIController, rbase) {
	REPOSITORY = rbase;
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	m_p_event = (iEvent *)OBJECT_BY_INTERFACE(I_EVENT, RY_CLONE);
	return true;
}
IMPLEMENT_DRIVER_INIT(cGenericAHCIController, host) {
	bool r = false;
	DRIVER_HOST = host;
	_snprintf(DRIVER_IDENT, sizeof(DRIVER_IDENT), (_str_t)"AHCI-%d", _ctrl_index_);
	if(DRIVER_HOST) {
		m_p_hba = (_hba_mem_t *)_ahci_abar_;
		if(setup_mem()) {
			setup_irq();
			setup_devices();
			_g_ahci_ctrl_index_++;
			r = true;
		}
	}

	return r;
}
IMPLEMENT_DRIVER_UNINIT(cGenericAHCIController) {
	iCPUHost *p_cpu_host = (iCPUHost *)OBJECT_BY_INTERFACE(I_CPU_HOST, RY_STANDALONE);
	if(p_cpu_host) {
		p_cpu_host->set_isr(m_int, 0, 0);
		iPMA *p_pma = (iPMA *)OBJECT_BY_INTERFACE(I_PMA, RY_STANDALONE);
		if(p_pma) {
			_u16 page_size = p_pma->get_page_size();
			_u32 npages = (MAX_PORTS * CMDH_TBL_LEN)/page_size;
			if(m_p_cmdh_tbl) {
				p_pma->free(m_p_cmdh_tbl, npages);
				m_p_cmdh_tbl = 0;
			}
			if(m_p_rfis_tbl) {
				npages = (MAX_PORTS * RECV_FIS_LEN)/page_size;
				p_pma->free(m_p_rfis_tbl, npages);
				m_p_rfis_tbl = 0;
			}
			RELEASE_OBJECT(p_pma);
		}

		RELEASE_OBJECT(p_cpu_host);
	}
}
IMPLEMENT_DESTROY(cGenericAHCIController) {
	RELEASE_OBJECT(m_p_syslog);
	m_p_syslog = 0;
	RELEASE_OBJECT(m_p_event);
	m_p_event = 0;
}

void cGenericAHCIController::irq_handler(void) {
	DBG("AHCI IRQ(%d)\r\n", _ahci_irq_);
	if(m_p_event)
		m_p_event->set(1);
}

void isr_ahci(iCPU *p_cpu, void *p_cpu_state, void *p_data) {
	cGenericAHCIController *p_ctrl = (cGenericAHCIController *)p_data;
	p_ctrl->irq_handler();
	p_cpu->interrupt(p_ctrl->m_int, p_cpu_state);
}

void cGenericAHCIController::setup_irq(void) {
	iCPUHost *p_cpu_host = (iCPUHost *)OBJECT_BY_INTERFACE(I_CPU_HOST, RY_STANDALONE);
	if(p_cpu_host) {
		iCPU *p_cpu = p_cpu_host->get_boot_cpu();
		m_int = INT_AHCI + _g_ahci_ctrl_index_;
		p_cpu_host->set_irq(_ahci_irq_, p_cpu->cpu_id(), m_int);
		p_cpu_host->set_isr(m_int, isr_ahci, this);
		if(m_p_event)
			m_p_event->init();
		RELEASE_OBJECT(p_cpu_host);
	}
}

void cGenericAHCIController::wait_irq(_u32 timeout) {
	if(m_p_event)
		wait_event(m_p_event, 1, timeout);
}

_u32 cGenericAHCIController::read(_u8 reg) { // read HBA register
	_u32 *p = (_u32 *)m_p_hba;
	return *(p + reg); 
}

void cGenericAHCIController::write(_u8 reg, _u32 value) { // write HBA register
	_u32 *p = (_u32 *)m_p_hba;
	*(p + reg) = value;
}

bool cGenericAHCIController::setup_mem(void) {
	bool r = false;
	iVMM *p_vmm = get_cpu()->get_vmm();
	if(p_vmm) {
		if(!p_vmm->test(_ahci_abar_)) {
			_vaddr_t addr[2];

			addr[0] = _ahci_abar_;
			addr[1] = _ahci_abar_ + 4096;
			p_vmm->map(addr, 2, _ahci_abar_, VMMF_NON_CACHEABLE);
			DBG("VMM map ahci ABAR at 0x%h\r\n", _ahci_abar_);
		}
		
		iPMA *p_pma = (iPMA *)OBJECT_BY_INTERFACE(I_PMA, RY_STANDALONE);
		if(p_pma) {
			_u16 page_size = p_pma->get_page_size();
			_u16 npages = (MAX_PORTS * CMDH_TBL_LEN)/page_size;
			// alloc memory for command header tabes
			if((m_p_cmdh_tbl = (_u8 *)p_pma->alloc(npages))) {
				DBG("'%s' CMDH table address 0x%h\r\n", DRIVER_IDENT, m_p_cmdh_tbl);
				mem_set(m_p_cmdh_tbl, 0, MAX_PORTS * CMDH_TBL_LEN);
				// alloc memory for received FIS
				npages = (MAX_PORTS * RECV_FIS_LEN)/page_size;
				if((m_p_rfis_tbl = (_u8 *)p_pma->alloc(npages))) {
					DBG("'%s' RECV FIS table address 0x%h\r\n", DRIVER_IDENT, m_p_rfis_tbl);
					mem_set(m_p_rfis_tbl, 0, MAX_PORTS * RECV_FIS_LEN);
					r = true;
				}
			}
			RELEASE_OBJECT(p_pma);
		}
	}

	return r;
}

void cGenericAHCIController::cmd_stop(_hba_port_t *p_port) {
	_u8 tmo = 10;
	
	p_port->cmd &= ~HBAP_CMD_ST;
	
	while((p_port->cmd & HBAP_CMD_FR) && (p_port->cmd & HBAP_CMD_CR) && tmo--)
		switch_context();
	
	p_port->cmd &= ~HBAP_CMD_FRE;
}

void cGenericAHCIController::cmd_start(_hba_port_t *p_port) {
	_u8 tmo = 10;

	while((p_port->cmd & HBAP_CMD_CR) && tmo--)
		switch_context();

	p_port->cmd |= HBAP_CMD_FRE;
	p_port->cmd |= HBAP_CMD_ST;
}

void cGenericAHCIController::init_port(_hba_port_t *p_port, _u8 nport) {
	cmd_stop(p_port);
	
	_u64_t addr;
	_u8 *p_cmdh = m_p_cmdh_tbl + (nport * CMDH_TBL_LEN); // command list base address
	_u8 *p_rfis = m_p_rfis_tbl + (nport * RECV_FIS_LEN); // received FIS base address

	addr._qw = k_to_p((_u64)p_cmdh);
	p_port->clb = addr._ldw;
	p_port->clbu= addr._hdw;

	addr._qw = k_to_p((_u64)p_rfis);
	p_port->fb = addr._ldw;
	p_port->fbu= addr._hdw;
}

void cGenericAHCIController::setup_devices(void) {
	iDriverRoot *p_dr = GET_DRIVER_ROOT();
	if(p_dr) {
		_u8 bit = 0;
		while(bit < 32) {
			if(((1 << bit) & m_p_hba->pi) && !((1 << bit) & m_pi)) {
				// detect device type
				_hba_port_t *p_port = (_hba_port_t *)(_ahci_abar_ + 0x100 + (bit * 0x80));
				_u32 ssts = p_port->ssts;
				_u8 ipm = (_u8)((ssts >> 8) & 0x0f);
				_u8 det = (_u8)(ssts & 0x0f);

				if(ipm == 1 /*active state*/ && det == 3 /*present*/) {
					iAHCIDevice *p_dev = (iAHCIDevice *)OBJECT_BY_INTERFACE(I_AHCI_DEVICE, RY_CLONE);
					if(p_dev) {
						init_port(p_port, bit);
						p_dev->_sata_index_ = bit;
						p_dev->_sata_port_ = (_vaddr_t)p_port;
						p_dev->_sata_max_cmd_count_ = MAX_CMD_COUNT;
						// register SATA device driver
						p_dr->register_driver(p_dev, this);
					}
				}
			}
			
			bit++;
		}
	}

	// store PI bits
	m_pi = m_p_hba->pi;
	
	RELEASE_OBJECT(p_dr);
}

