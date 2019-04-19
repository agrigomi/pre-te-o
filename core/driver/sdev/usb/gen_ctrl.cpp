#include "iSerialDevice.h"
#include "ctrldef.h"
#include "iSystemLog.h"
#include "intdef.h"
#include "iCPUHost.h"
#include "usb.h"

#define _DEBUG_
#include "debug.h"

#ifdef _DEBUG_
#define	LOG(_fmt_, ...) \
	DBG(_fmt_, __VA_ARGS__)
#else
#define LOG(_fmt_, ...) \
	if(m_p_syslog) \
		m_p_syslog->fwrite(_fmt_, __VA_ARGS__) 
#endif

static _u16 _g_usb_ctrl_index_=0;

class cGenericUSBHostController: public iUSBController {
public:
	cGenericUSBHostController() {
		DRIVER_TYPE = DTYPE_CTRL;
		DRIVER_MODE = DMODE_CHAR;
		DRIVER_CLASS = DCLASS_USB;
		_ctrl_type_[0]='U';_ctrl_type_[1]='S';_ctrl_type_[2]='B';_ctrl_type_[3]=0;
		m_p_syslog = 0;
		m_int = 0;
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}
	~cGenericUSBHostController() {}

	DECLARE_BASE(cGenericUSBHostController);
	DECLARE_INIT();
	DECLARE_DRIVER_INIT();
	DECLARE_DESTROY();
	DECLARE_DRIVER_UNINIT();

protected:
	iSystemLog	*m_p_syslog;
	_u8		m_int;
	
	void setup_irq(void);
	void irq_handler(void);
	friend void _isr_usb_(iCPU *p_cpu, void *p_cpu_state, void *p_data);
};

IMPLEMENT_BASE(cGenericUSBHostController, USB_CTRL_NAME_GENERIC, 0x0001);
IMPLEMENT_INIT(cGenericUSBHostController, rbase) {
	REPOSITORY = rbase;
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	INIT_MUTEX();
	return true;
}
IMPLEMENT_DESTROY(cGenericUSBHostController) {
	RELEASE_MUTEX();
}
IMPLEMENT_DRIVER_INIT(cGenericUSBHostController, host) {
	bool r = false;
	if((DRIVER_HOST = host)) {
		_s8 ct[10]="";
		switch(_usb_prog_if_) {
			case USB_PI_UHCI:
				str_cpy(ct, (_str_t)"UHCI", sizeof(ct));
				break;
			case USB_PI_OHCI:
				str_cpy(ct, (_str_t)"OHCI", sizeof(ct));
				break;
			case USB_PI_EHCI:
				str_cpy(ct, (_str_t)"EHCI", sizeof(ct));
				break;
			case USB_PI_EXHCI:
				str_cpy(ct, (_str_t)"ExHCI", sizeof(ct));
				break;
			default:
				str_cpy(ct, (_str_t)"????", sizeof(ct));
				break;
		}

		_snprintf(DRIVER_IDENT, sizeof(DRIVER_IDENT), (_str_t)"%s-%d", ct, _ctrl_index_);

		setup_irq();
		_g_usb_ctrl_index_++;
		r = true;
	}

	return r;
}
IMPLEMENT_DRIVER_UNINIT(cGenericUSBHostController) {
	iCPUHost *p_cpu_host = (iCPUHost *)OBJECT_BY_INTERFACE(I_CPU_HOST, RY_STANDALONE);
	if(p_cpu_host && m_int) {
		p_cpu_host->set_isr(m_int, 0, 0);
		RELEASE_OBJECT(p_cpu_host);
	}
}

void cGenericUSBHostController::irq_handler(void) {
	DBG("USB IRQ(%d)\r\n", _usb_irq_);
	//???
}

void _isr_usb_(iCPU *p_cpu, void *p_cpu_state, void *p_data) {
	cGenericUSBHostController *p_ctrl = (cGenericUSBHostController *)p_data;
	p_ctrl->irq_handler();
	p_cpu->interrupt(p_ctrl->m_int, p_cpu_state);
}

void cGenericUSBHostController::setup_irq(void) {
	iCPUHost *p_cpu_host = (iCPUHost *)OBJECT_BY_INTERFACE(I_CPU_HOST, RY_STANDALONE);
	if(p_cpu_host) {
		iCPU *p_cpu = p_cpu_host->get_boot_cpu();
		m_int = INT_USB + _g_usb_ctrl_index_;
		LOG("\t\t\tUSB(%d): assign IRQ(%d) INT(%d)\r\n", _ctrl_index_, _usb_irq_, m_int);
		p_cpu_host->set_irq(_usb_irq_, p_cpu->cpu_id(), m_int);
		p_cpu_host->set_isr(m_int, _isr_usb_, this);
		RELEASE_OBJECT(p_cpu_host);
	}
}

