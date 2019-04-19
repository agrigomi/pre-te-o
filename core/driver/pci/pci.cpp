#include "iSystemLog.h"
#include "iScheduler.h"
#include "io_port.h"
#include "iSystemBus.h"
#include "str.h"
#include "iCPUHost.h"
#include "iDataStorage.h"
#include "io_port.h"

typedef union {
	_u32 address;
	struct {
		_u32		:2; // 00
		_u32	reg	:6; // register number
		_u32	fn	:3; // function number
		_u32	slot	:5; // device number
		_u32	bus	:8; // bus nmber
		_u32		:7; // reserved
		_u32	enable	:1; // enable bit
	}__attribute__((packed));
}__attribute__((packed)) _pci_addr_t;

class cPCI:public iPCI {
public:
	cPCI() {
		m_v_pci = 0;
		DRIVER_TYPE = DTYPE_BUS;
		DRIVER_MODE = 0;
		DRIVER_CLASS = DCLASS_SYS;
		str_cpy(DRIVER_IDENT, (_str_t)"PCI Bus", sizeof(DRIVER_IDENT));
		_bus_type_[0]='P';_bus_type_[1]='C';_bus_type_[2]='I';_bus_type_[3]=0;
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	~cPCI() {
		CALL_DESTROY();
	}
	
	// base
	DECLARE_BASE(cPCI);
	DECLARE_INIT();
	DECLARE_DESTROY();
	
	// DriverBase
	DECLARE_DRIVER_INIT();
	DECLARE_DRIVER_UNINIT();
	
	_u32 count(void);
	bool device(_u32 index, _pci_hdr_t *p_dev);
	_u32 read(_u32 index, _u8 reg);
	void write(_u32 index, _u8 reg, _u32 value);
protected:
	iVector	*m_v_pci;
	
	bool get_device_info(_u8 bus, 
				     _u8 device, 
				     _u8 function, 
				     _pci_hdr_t *p_info);
	_u32 pci_scan(void);
	_u32 pci_read(_u8 bus, _u8 slot, _u8 func, _u8 reg, iCPU *p_cpu=0);
	void pci_write(_u8 bus, _u8 slot, _u8 func, _u8 reg, _u32 value, iCPU *p_cpu=0);
};

static iSystemLog *_g_p_syslog_=0;

//#define _DEBUG_

#ifdef _DEBUG_ 
#define DBG(_fmt_, ...) \
	if(_g_p_syslog_) \
		_g_p_syslog_->fwrite(_fmt_, __VA_ARGS__) 
#else
#define	DBG(_fmt_, ...)	;
#endif // debug		

#define LOG(_fmt_, ...) \
	if(_g_p_syslog_) \
		_g_p_syslog_->fwrite(_fmt_, __VA_ARGS__) 

typedef struct {
	_u8	bus;
	_u8	slot;
	_u8	func;
}__attribute__((packed)) _pci_entry_t;
		
IMPLEMENT_BASE(cPCI, "cPCIBus", 0x0001);
IMPLEMENT_INIT(cPCI, rb) {
	bool r = false;
	
	REPOSITORY = rb;
	INIT_MUTEX();
	_g_p_syslog_ = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	m_v_pci = (iVector *)OBJECT_BY_INTERFACE(I_VECTOR, RY_CLONE);
	
	if(_g_p_syslog_ && m_v_pci) {
		m_v_pci->init(sizeof(_pci_entry_t));
		REGISTER_DRIVER();
		r = true;
	}
	
	return r;
}
IMPLEMENT_DRIVER_INIT(cPCI, host) {
	bool r = false;
	
	if(!(DRIVER_HOST = host)) {
		iDriverRoot *p_dh = GET_DRIVER_ROOT();
		if(p_dh)
			DRIVER_HOST = p_dh;
	}
	
	if(DRIVER_HOST) {
		pci_scan();
		r = true;
	}
	
	return r;
}
IMPLEMENT_DRIVER_UNINIT(cPCI) {
}
IMPLEMENT_DESTROY(cPCI) {
	if(m_v_pci) {
		m_v_pci->clr();
		RELEASE_OBJECT(m_v_pci);
	}
	
	RELEASE_OBJECT(_g_p_syslog_);
}

_u32 cPCI::pci_read(_u8 bus, _u8 slot, _u8 func, _u8 reg, iCPU *p_cpu) {
	_u32 r = 0;
	_pci_addr_t addr;
	
	addr.address = 0;
	
	addr.bus = bus;
	addr.slot = slot;
	addr.fn  = func;
	addr.reg = reg;
	addr.enable = 1;
	
	bool irqs = false;
	
	if(p_cpu) 
		irqs = p_cpu->enable_interrupts(false);
	
	HMUTEX hlock = lock();
	outportl(PCI_CONFIG_ADDRESS, addr.address);
	r = inportl(PCI_CONFIG_DATA);
	outportl(PCI_CONFIG_ADDRESS, 0x00);
	unlock(hlock);
	
	if(irqs && p_cpu)
		p_cpu->enable_interrupts(true);
	
	return r;
}

void cPCI::pci_write(_u8 bus, _u8 slot, _u8 func, _u8 reg, _u32 value, iCPU *p_cpu) {
	_pci_addr_t addr;
	
	addr.address = 0;
	
	addr.bus = bus;
	addr.slot = slot;
	addr.fn  = func;
	addr.reg = reg;
	addr.enable = 1;
	
	bool irqs = false;
	
	if(p_cpu) 
		irqs = p_cpu->enable_interrupts(false);
	
	HMUTEX hlock = lock();
	outportl(PCI_CONFIG_ADDRESS, addr.address);
	outportl(PCI_CONFIG_DATA, value);
	outportl(PCI_CONFIG_ADDRESS, 0x00);
	unlock(hlock);
	
	if(irqs && p_cpu)
		p_cpu->enable_interrupts(true);
}

_u32 cPCI::read(_u32 index, _u8 reg) {
	_u32 r = 0;
	_pci_entry_t *p_entry = (_pci_entry_t *)m_v_pci->get(index);

	if(p_entry) {
		iCPU *p_cpu = get_cpu();
		r = pci_read(p_entry->bus, p_entry->slot, p_entry->func, reg, p_cpu);
	}
	
	return r;
}

void cPCI::write(_u32 index, _u8 reg, _u32 value) {
	_pci_entry_t *p_entry = (_pci_entry_t *)m_v_pci->get(index);

	if(p_entry) {
		iCPU *p_cpu = get_cpu();
		pci_write(p_entry->bus, p_entry->slot, p_entry->func, reg, value, p_cpu);
	}
}

bool cPCI::get_device_info(_u8 bus, 
			   _u8 device, 
			   _u8 function, 
			   _pci_hdr_t *p_info) {
	bool r = false;
	iCPU *p_cpu = get_cpu();

	p_info->vd = pci_read(bus, device, function, 0x00, p_cpu);
	if(p_info->vendor_id != 0xffff) {
		_u32 *p = (_u32 *)p_info;
		for(_u8 i = 1; i < 16; i++)
			*(p+i) = pci_read(bus, device, function, i, p_cpu);
		
		r = true;
	}
	
	return r;
}

_u32 cPCI::pci_scan(void) {
	_u32 r = 0;
	_pci_hdr_t pci;

	m_v_pci->clr();

	LOG("Scan PCI bus: ","");
	for(_u32 bus = 0; bus < MAX_PCI_BUS; bus++) {
		for(_u32 dev = 0; dev < MAX_PCI_DEVICE; dev++) {
			for(_u32 fun = 0; fun < MAX_PCI_FUNCTION; fun++) {
				pci.vd = pci_read(bus, dev, fun, 0x00);
				if(pci.vendor_id != 0xffff) {
					_pci_entry_t entry;
					
					LOG(".","");
					entry.bus = bus;
					entry.slot = dev;
					entry.func = fun;
					
					m_v_pci->add(&entry);
					r++;
				}
			}
		}
	}

	LOG("\r\n","");

	return r;
}

_u32 cPCI::count(void) {
	return m_v_pci->cnt();
}

bool cPCI::device(_u32 index, _pci_hdr_t *p_dev) {
	bool r = false;
	
	_pci_entry_t *p_entry = (_pci_entry_t *)m_v_pci->get(index);
	if(p_entry)
		r = get_device_info(p_entry->bus, p_entry->slot, p_entry->func, p_dev);
	
	return r;
}

