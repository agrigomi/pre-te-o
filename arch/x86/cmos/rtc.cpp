#include "iRTC.h"
#include "str.h"
#include "iSyncObject.h"
#include "iCPUHost.h"
#include "iSystemLog.h"
#include "intdef.h"
#include "iScheduler.h"
#include "iCMOS.h"

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

#define MAX_RTC_CALLBACK	16

typedef struct {
	_rtc_callback_t *p_callback;
	void *p_udata;
}_rtc_cb_inf_t;

class cRTC:public iRTC {
public:
	cRTC() {
		DRIVER_TYPE = DTYPE_DEV;
		DRIVER_CLASS = DCLASS_SYS;
		str_cpy((_str_t)DRIVER_IDENT, (_str_t)"RTC", sizeof(DRIVER_IDENT));
		m_p_event = 0;
		m_p_cmos = 0;
		m_irq_count = 0;
		m_ready = false;
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}

	//virtual ~cRTC() {}

	// iBase
	DECLARE_BASE(cRTC);
	DECLARE_INIT();
	DECLARE_DESTROY();

	DECLARE_DRIVER_INIT();
	DECLARE_DRIVER_UNINIT();

	_u32 timestamp(void);
	bool enable_irq(bool enable=true);
	bool set_callback(_rtc_callback_t *p_callback, void *p_udata);
	void del_callback(_rtc_callback_t *p_callback);
	_u8 seconds(void);
	_u8 minutes(void);
	_u8 hours(void);
	_u8 day_of_week(void);
	_u8 day_of_month(void);
	_u8 month(void);
	_u8 year(void);
	void wait(void);
	_u32 count_irq(void);

protected:
	iEvent	*m_p_event;
	iSystemLog *m_p_syslog;
	iCMOS	*m_p_cmos;
	_u32	m_irq_count;
	_rtc_cb_inf_t m_cb[MAX_RTC_CALLBACK];
	bool	m_ready;

	_u8 read_reg(_u8 reg);
	void write_reg(_u8 reg, _u8 value);
	friend void rtc_irq_handler(iCPU *p_cpu, void *p_cpu_state, void *p_data);
	void rtc_event(iCPU *p_cpu, void *cpu_state);
};

IMPLEMENT_BASE(cRTC, "cRTC", 0x0001);
IMPLEMENT_INIT(cRTC, rbase) {
	bool r = false;
	REPOSITORY = rbase;
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	m_p_event = (iEvent *)OBJECT_BY_INTERFACE(I_EVENT, RY_CLONE);
	m_p_cmos = (iCMOS *)OBJECT_BY_INTERFACE(I_CMOS, RY_STANDALONE);
	if(m_p_event && m_p_cmos) {
		m_p_event->init();
		mem_set((_u8 *)m_cb, 0, sizeof(m_cb));
		enable_irq(false);
		REGISTER_DRIVER(); // self registration
		r = true;
	}
	return r;
}
IMPLEMENT_DESTROY(cRTC) {
	if(m_p_syslog) {
		RELEASE_OBJECT(m_p_syslog);
		m_p_syslog = 0;
	}
	if(m_p_event) {
		RELEASE_OBJECT(m_p_event);
		m_p_event = 0;
	}
	if(m_p_cmos) {
		RELEASE_OBJECT(m_p_cmos);
		m_p_cmos = 0;
	}
}

void rtc_irq_handler(iCPU *p_cpu, void *p_cpu_state, void *p_data) {
	cRTC *p_obj = (cRTC *)p_data;
	p_obj->rtc_event(p_cpu,p_cpu_state);
}

IMPLEMENT_DRIVER_INIT(cRTC, host) {
	bool r = false;

	DRIVER_HOST = host;
	iCPUHost *p_cpu_host = (iCPUHost *)OBJECT_BY_INTERFACE(I_CPU_HOST, RY_STANDALONE);
	if(p_cpu_host) {
		if(p_cpu_host->is_running()) {
			DRIVER_HOST = p_cpu_host;
			// init IRQ handler
			iCPU *p_cpu = p_cpu_host->get_boot_cpu();
			if((r = p_cpu_host->set_irq(IRQ_RTC, p_cpu->cpu_id(), INT_RTC)))
				p_cpu_host->set_isr(INT_RTC, rtc_irq_handler, this);

			_u8 b = m_p_cmos->read(0x8b);
			b &= ~0xc0; // enable clock update & disable IRQ
			m_p_cmos->write(0x8b, b);

			m_ready = r = true;
		}

		RELEASE_OBJECT(p_cpu_host);
	}

	return r;
}
IMPLEMENT_DRIVER_UNINIT(cRTC) {
	iCPUHost *p_cpu_host = (iCPUHost *)OBJECT_BY_INTERFACE(I_CPU_HOST, RY_STANDALONE);
	if(p_cpu_host)
		p_cpu_host->set_isr(INT_RTC, 0, 0);
}

_u32 cRTC::timestamp(void) {
	_u8 tm_sec = seconds();
	_u8 tm_hour = hours();
	_u8 tm_min = minutes();
	_u32 tm_year = year();
	_u8 tm_mon = month();
	_u8 tm_mday = day_of_month();

	if(m_p_cmos->read(11) & 0x04) { // convert BCD to binary
		tm_sec = (tm_sec & 0x0F) + ((tm_sec / 16) * 10);
		tm_min = (tm_min & 0x0F) + ((tm_min / 16) * 10);
		tm_hour = ((tm_hour & 0x0F) + (((tm_hour & 0x70) / 16) * 10) ) | (tm_hour & 0x80);
		tm_year = (tm_year & 0x0F) + ((tm_year / 16) * 10);
		tm_mon = (tm_mon & 0x0F) + ((tm_mon / 16) * 10);
		tm_mday = (tm_mday & 0x0F) + ((tm_mday / 16) * 10);
	}

	bool is_leap = (((tm_year % 4) == 0) && (((tm_year % 100) != 0) || ((tm_year % 400) == 0)));
	_u32 tm_yday = tm_mday;
	_u32 days_per_month[]={31, ((is_leap) ? 29 : 28), 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	for(_u32 i = 0; i < (_u32)(tm_mon-1); i++)
		tm_yday += days_per_month[i];

	return tm_sec + tm_min*60 + tm_hour*3600 + tm_yday*86400 +
		    (tm_year-70)*31536000 + ((tm_year-69)/4)*86400 -
		        ((tm_year-1)/100)*86400 + ((tm_year+299)/400)*86400;
}

bool cRTC::set_callback(_rtc_callback_t *p_callback, void *p_udata) {
	bool r = false;
	if(m_ready) {
		for(_u32 i = 0; i < MAX_RTC_CALLBACK; i++) {
			if(!m_cb[i].p_callback) {
				m_cb[i].p_callback = p_callback;
				m_cb[i].p_udata = p_udata;
				r = true;
				break;
			}
		}
	}
	return r;
}

void cRTC::del_callback(_rtc_callback_t *p_callback) {
	for(_u32 i = 0; i < MAX_RTC_CALLBACK; i++) {
		if(m_cb[i].p_callback == p_callback) {
			m_cb[i].p_callback = 0;
			m_cb[i].p_udata = 0;
			break;
		}
	}
}

bool cRTC::enable_irq(bool enable) {
	_u8 reg_b = m_p_cmos->read(0x8b);
	bool r = (reg_b & 0x40)?true:false;

	if(enable)
		reg_b |= 0x40;
	else
		reg_b &= ~0x40;
	m_p_cmos->write(0x8b, reg_b);

	return r;
}

void cRTC::rtc_event(iCPU _UNUSED_ *p_cpu, void _UNUSED_ *cpu_state) {
	DBG("RTC irq\r\n","");
	m_irq_count++;
	if(m_p_event) {
		if(m_p_cmos->read(0x8c) & 0xc0) {
			m_p_event->set(1);

			for(_u32 i = 0; i < MAX_RTC_CALLBACK; i++) {
				if(m_cb[i].p_callback)
					m_cb[i].p_callback(m_cb[i].p_udata);
			}
		}
	}
}

_u32 cRTC::count_irq(void) {
	_u32 r = m_irq_count;
	m_irq_count = 0;
	return r;
}

_u8 cRTC::read_reg(_u8 reg) {
	return m_p_cmos->read(reg);
}

void cRTC::write_reg(_u8 reg, _u8 value) {
	m_p_cmos->write(reg, value);
}

_u8 cRTC::seconds(void) {
	while(m_p_cmos->read(0x8a) & 0x80);
	return m_p_cmos->read(0);
}

_u8 cRTC::minutes(void) {
	while(m_p_cmos->read(0x8a) & 0x80);
	return m_p_cmos->read(2);
}
_u8 cRTC::hours(void) {
	while(m_p_cmos->read(0x8a) & 0x80);
	return m_p_cmos->read(4);
}
_u8 cRTC::day_of_week(void) {
	while(m_p_cmos->read(0x8a) & 0x80);
	return m_p_cmos->read(6);
}
_u8 cRTC::day_of_month(void) {
	while(m_p_cmos->read(0x8a) & 0x80);
	return m_p_cmos->read(7);
}
_u8 cRTC::month(void) {
	while(m_p_cmos->read(0x8a) & 0x80);
	return m_p_cmos->read(8);
}
_u8 cRTC::year(void) {
	while(m_p_cmos->read(0x8a) & 0x80);
	return m_p_cmos->read(9);
}

void cRTC::wait(void) {
	wait_event(m_p_event, 1);
}

