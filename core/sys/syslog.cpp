#include "iMemory.h"
#include "str.h"
#include "iDriver.h"
#include "iScheduler.h"
#include "iSystemLog.h"
#include "iSerialDevice.h"
#include "iCPUHost.h"

#define DEFAULT_SYSLOG_PAGES	2

class cSysLog:public iSystemLog {
public:
	cSysLog() {
		m_npages = DEFAULT_SYSLOG_PAGES;
		m_p_console = 0;
		m_p_buffer = 0;
		m_sz_buffer = 0;
		m_p_mutex = 0;
		ERROR_CODE = 0;
		m_output = true;
		m_lock = true;
		m_cpos = 0;
		m_opos = 0;
		m_fpos = 0;
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	
	//~cSysLog() {
	//}
	
	// iBase
	DECLARE_BASE(cSysLog);
	DECLARE_INIT();
	DECLARE_DESTROY();
	
	void init(_u32 npages=DEFAULT_SYSLOG_PAGES) {
		m_npages = npages;
	}
	
	_u32 write(_cstr_t text, HMUTEX hlock=0);
	_u32 fwrite(_cstr_t format,...);
	_u32 fwrite(HMUTEX hlock, _cstr_t format,...);
	void enable_output(void) {
		m_output = true;
	}
	void disable_output(void) {
		m_output = false;
	}
	
	void disable_lock(void) { m_lock = false; }
	void enable_lock(void) { m_lock = true; }
	HMUTEX lock(HMUTEX hlock=0, _u32 timeout=MUTEX_TIMEOUT_INFINITE, _u8 flags=0);
	HMUTEX try_lock(HMUTEX hlock=0);
	void unlock(HMUTEX hlock);
	iMutex *get_mutex(void) {return m_p_mutex; }
	
protected:
	iConsole	*m_p_console; // output object
	iMutex		*m_p_mutex;
	_u32 		m_npages;
	_s8		*m_p_buffer; // pointer to log buffer
	_u32		m_sz_buffer; // buffer size
	_u32		m_cpos; // current write position (in log buffer)
	_u32		m_opos; // current ouput position (in log buffer)
	_u32		m_fpos; // current file output position (in log buffer)
	bool		m_output; // enable/disable console output
	bool		m_lock;
	
	bool query_console(void);
	void scroll_buffer(void);
};

#define _CR 0x0d
#define _LF 0x0a
#define FMT_BUFFER_SIZE	1024

IMPLEMENT_BASE(cSysLog, "cSysLog", 0x0001);

IMPLEMENT_DESTROY(cSysLog) {
	if(m_p_console) {
		RELEASE_OBJECT(m_p_console);
		m_p_console = 0;
	}
	
	if(m_p_mutex) {
		RELEASE_OBJECT(m_p_mutex);
		m_p_mutex = 0;
	}
	
	if(m_p_buffer) {
		iPMA *p_pma = (iPMA *)OBJECT_BY_INTERFACE(I_PMA, RY_STANDALONE);
		if(p_pma) {
			p_pma->free(m_p_buffer, m_npages);
			m_p_buffer = 0;
			m_sz_buffer = 0;
		}
	}
}

IMPLEMENT_INIT(cSysLog, p_rb) {
	bool r = true;
	
	STORE_REPOSITORY(p_rb);

	query_console();
	m_p_mutex = (iMutex *)OBJECT_BY_INTERFACE(I_MUTEX, RY_CLONE);
	iPMA *p_pma = (iPMA *)OBJECT_BY_INTERFACE(I_PMA, RY_STANDALONE);
	if(p_pma) {
		m_p_buffer = (_s8 *)p_pma->alloc(m_npages);
		if(m_p_buffer) {
			m_sz_buffer = p_pma->get_page_size() * m_npages;
			mem_set((_u8 *)m_p_buffer, 0, m_sz_buffer);
		} else	
			r = false;
	}
	
	return r;
}

bool cSysLog::query_console(void) {
	bool r = false;

	if(!m_p_console) {
		iDriverRoot *p_dr = GET_DRIVER_ROOT();
		if(p_dr) {
			_driver_request_t dr;
			dr.flags = DRF_INTERFACE;
			dr._d_interface_ = I_CONSOLE;
			m_p_console = (iConsole *)p_dr->driver_request(&dr);
			RELEASE_OBJECT(p_dr);
		}
	}
	
	if(m_p_console) {
		iDriverBase *p_out = m_p_console->get_output();
		if(p_out)
			r = true;
	}
	
	return r;
}

HMUTEX cSysLog::lock(HMUTEX hlock, _u32 timeout, _u8 flags) {
	HMUTEX r = 0;
	
	if(m_p_mutex && m_lock)
		r = m_p_mutex->lock(hlock, timeout, flags);
	
	return r;
}

HMUTEX cSysLog::try_lock(HMUTEX hlock) {
	HMUTEX r = 0;
	
	if(m_p_mutex && m_lock)
		r = m_p_mutex->try_lock(hlock);
	
	return r;
}

void cSysLog::unlock(HMUTEX hlock) {
	if(m_p_mutex)
		m_p_mutex->unlock(hlock);
}

_u32 cSysLog::write(_cstr_t text, HMUTEX hlock) {
	_u32 r = 0;

	if(m_p_buffer) {
		HMUTEX hm = lock(hlock);
		_u32 len = str_len((_str_t)text);
		
		while((m_sz_buffer - m_cpos - 1) < len)
			scroll_buffer();
		
		r = str_cpy(m_p_buffer + m_cpos, (_str_t)text, m_sz_buffer - m_cpos);
		
		if(m_output && query_console()) {
			_u32 nb=0;
			m_p_console->write((_u8 *)(m_p_buffer + m_opos), 0, &nb);
			m_opos += nb;
		}
		
		m_cpos += r;
		
		unlock(hm);
	}
	
	return r;
}

_u32 cSysLog::fwrite(_cstr_t format,...) {
	_u32 r = 0;
	
	va_list args;
	_s8 lb[FMT_BUFFER_SIZE];

	va_start(args, format);

	_vsnprintf(lb, FMT_BUFFER_SIZE, format, args);
	r = write(lb);
	
	va_end(args);
	
	return r;
}

_u32 cSysLog::fwrite(HMUTEX hlock, _cstr_t format,...) {
	_u32 r = 0;
	
	va_list args;
	_s8 lb[FMT_BUFFER_SIZE];

	HMUTEX hm = lock(hlock);
	va_start(args, format);

	_vsnprintf(lb, FMT_BUFFER_SIZE, format, args);
	r = write(lb, hm);
	
	va_end(args);
	unlock(hm);
	
	return r;
}

void cSysLog::scroll_buffer(void) {
	if(m_cpos) {
		bool blspc = false;
		_u32 bscroll = 0;

		for(bscroll = 0; bscroll < m_cpos; bscroll++) {
			if(m_p_buffer[bscroll] < 0x20)
				blspc = true;
			else {
				if(blspc)
					break;
			}
		}
		
		mem_cpy((_u8 *)m_p_buffer, (_u8 *)(m_p_buffer + bscroll), m_sz_buffer - bscroll);
		mem_set((_u8 *)(m_p_buffer + m_sz_buffer - bscroll), 0, bscroll);
		m_cpos -= bscroll;
		if(m_opos >= bscroll)
			m_opos -= bscroll;
		else
			m_opos = 0;
	}
}

