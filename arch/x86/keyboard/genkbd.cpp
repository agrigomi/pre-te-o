#include "iIOApic.h"
#include "io_port.h"
#include "iScheduler.h"
#include "intdef.h"
#include "iSerialDevice.h"
#include "str.h"
#include "iCPUHost.h"
#include "iSystemLog.h"

//#define _KBD_DEBUG_

#ifdef _KBD_DEBUG_ 
#define DBG(_fmt_, ...) \
	if(m_p_syslog) \
		m_p_syslog->fwrite(_fmt_, __VA_ARGS__) 
#else
#define	DBG(_fmt_, ...)
#endif // debug		

#define MAX_KBD_BUFFER	32
		
class cKeyboard:public iKeyboard {
public:
	cKeyboard() {
		m_wpos = m_rpos = 0;
		m_p_cpu_host = 0;
		DRIVER_TYPE = DTYPE_DEV;
		DRIVER_MODE = DMODE_CHAR;
		DRIVER_CLASS = DCLASS_KBD;
		str_cpy((_str_t)DRIVER_IDENT, (_str_t)"GKBD", sizeof(DRIVER_IDENT));
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	
	virtual ~cKeyboard() {
		CALL_DESTROY();
	}

	// iBase
	DECLARE_BASE(cKeyboard);
	DECLARE_INIT();
	DECLARE_DESTROY();
	
	// iDriver
	DECLARE_DRIVER_INIT();
	DECLARE_DRIVER_UNINIT();
	
	void driver_ioctl(_u32 cmd, _driver_io_t *p_dio);

protected:
	_u8 m_buffer[MAX_KBD_BUFFER];
	iCPUHost *m_p_cpu_host;
	_u32 m_wpos;
	_u32 m_rpos;
#ifdef _KBD_DEBUG_ 	
	iSystemLog *m_p_syslog;	
#endif
	_u32 kbd_read(_u8 *p_buffer, _u32 sz);
	void kbd_event(iCPU *p_cpu, void *cpu_state);
	friend void kbd_irq0(iCPU *p_cpu, void *p_cpu_state, void *p_data);
};

IMPLEMENT_BASE(cKeyboard, "cGenericKeyboard", 0x0001);
IMPLEMENT_INIT(cKeyboard, rb) {
	bool r = true;
	REPOSITORY = rb;
#ifdef _KBD_DEBUG_  	
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
#endif	
	INIT_MUTEX();
	REGISTER_DRIVER();
	return r;
}

void kbd_irq0(iCPU *p_cpu, void *p_cpu_state, void *p_data) {
	cKeyboard *p_obj = (cKeyboard *)p_data;
	p_obj->kbd_event(p_cpu,p_cpu_state);
}

IMPLEMENT_DRIVER_INIT(cKeyboard, host) {
	bool r = false;
	
	DRIVER_HOST = host;
	
	iDriverRoot *p_dr = GET_DRIVER_ROOT();
	if(p_dr) {
		_driver_request_t dr;
		mem_set((_u8 *)&dr, 0, sizeof(_driver_request_t));
		dr.flags = DRF_INTERFACE;
		
		dr.DRIVER_INTERFACE = I_IO_APIC;
		iIOApic *p_ioapic = (iIOApic *)p_dr->driver_request(&dr);
		
		if(p_ioapic) {
			dr.flags = DRF_INTERFACE;
			dr.DRIVER_INTERFACE = I_CPU_HOST;
			
			m_p_cpu_host = (iCPUHost *)(p_dr->driver_request(&dr));
			if(m_p_cpu_host) { // init IRQ handler
				iCPU *p_cpu = m_p_cpu_host->get_boot_cpu();
				if((r = m_p_cpu_host->set_irq(IRQ_KBD, p_cpu->cpu_id(), INT_KBD)))
					m_p_cpu_host->set_isr(INT_KBD, kbd_irq0, this);
			}
			
			RELEASE_OBJECT(p_ioapic);
		}
		
		dr.DRIVER_INTERFACE = I_SD_HOST;
		DRIVER_HOST = p_dr->driver_request(&dr);
		
		RELEASE_OBJECT(p_dr);
	}
	
	return r;
}

IMPLEMENT_DRIVER_UNINIT(cKeyboard) {
	if(m_p_cpu_host) {
		if(m_p_cpu_host->set_irq(1, 0, INT_KBD, IRQF_MASKED))
			m_p_cpu_host->set_isr(INT_KBD, 0, 0);
		
		RELEASE_OBJECT(m_p_cpu_host);
	}
}

IMPLEMENT_DESTROY(cKeyboard) {
	RELEASE_MUTEX();
}

void cKeyboard::kbd_event(iCPU _UNUSED_ *p_cpu, void _UNUSED_ *cpu_state) {
	_u8 scan_code = inportb(IO_KBD_INPUT);
	
	DBG("key (0x%x)-%d\r\n", (_u32)scan_code, m_wpos);
	m_buffer[m_wpos] = scan_code;
	m_wpos++;
	if(m_wpos >= MAX_KBD_BUFFER)
		m_wpos = 0;
}

_u32 cKeyboard::kbd_read(_u8 *p_buffer, _u32 sz) {
	_u32 r = 0;

	bool ints = enable_interrupts(false);
	HMUTEX hm = lock();
	
	while(r < sz && m_rpos != m_wpos) {
		*(p_buffer + r) = *(m_buffer + m_rpos);
		r++;
		m_rpos++;
		if(m_rpos >= MAX_KBD_BUFFER)
			m_rpos = 0;
	}
	
	unlock(hm);
	if(ints)
		enable_interrupts(true);
	
	return r;
}

void cKeyboard::driver_ioctl(_u32 cmd, _driver_io_t *p_dio) {
	switch(cmd) {
		case DIOCTL_SREAD:
			p_dio->result = kbd_read(p_dio->buffer, p_dio->size);
			break;
		default:
			p_dio->result = DIOCTL_ERRCMD;
	}
}
