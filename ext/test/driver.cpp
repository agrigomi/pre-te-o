#include "iDriver.h"
#include "str.h"
#include "iSystemLog.h"
#include "iSerialDevice.h"

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

#define _OBJECT_NAME_		"cDriverTest"
#define _OBJECT_CLASS_		cDriverTest
#define _OBJECT_INTERFACE_	iDriverBase
#define _OBJECT_TYPE_		RY_ONCE
#define _OBJECT_ID_		RY_NO_OBJECT_ID // or ID
#define _OBJECT_VERSION_	0x0001

#define _DRIVER_TYPE_		DTYPE_DEV // see iDriver.h
#define _DRIVER_MODE_		DMODE_CHAR
#define _DRIVER_CLASS_		DCLASS_PORT
#define _DRIVER_IDENT_		"TDriver"

class _OBJECT_CLASS_:public _OBJECT_INTERFACE_ {
private:
	// TODO private members
protected:
	// TODO protected members	
	iSystemLog *m_p_syslog;
public:
	_OBJECT_CLASS_() {
		DRIVER_TYPE = _DRIVER_TYPE_;
		DRIVER_MODE = _DRIVER_MODE_;
		DRIVER_CLASS= _DRIVER_CLASS_;
		str_cpy(DRIVER_IDENT, (_str_t)_DRIVER_IDENT_, sizeof(DRIVER_IDENT));

		m_p_syslog = 0;

		REGISTER_OBJECT(_OBJECT_TYPE_, _OBJECT_ID_);
	}
	~_OBJECT_CLASS_() {
	}

	// declare base members
	DECLARE_BASE(_OBJECT_CLASS_);
	
	bool object_init(iRepositoryBase *p_rb) {
		REPOSITORY = p_rb;
		m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_ONCE);
		LOG("init object '%s'\r\n", _OBJECT_NAME_);
		REGISTER_DRIVER();
		return true;
	}

	bool driver_init(iDriverBase *p_host) {
		bool r = false;
		if(!(DRIVER_HOST = p_host)) {
			// use 'driver_request' to find a host for this driver
			_driver_request_t dr;
			dr.flags = DRF_INTERFACE;
			dr.DRIVER_INTERFACE = I_SD_HOST;
			iDriverRoot *p_droot = GET_DRIVER_ROOT();
			if(p_droot) {
				iSDHost *p_sdh = (iSDHost *)p_droot->driver_request(&dr);
				if(p_sdh) {
					LOG("%s: attach to '%s'\r\n", DRIVER_IDENT, p_sdh->DRIVER_IDENT);
					DRIVER_HOST = p_sdh;
					r = true;
				}
			} else
				LOG("No driver root\r\n","");
		} 
		return r;
	}

	void driver_ioctl(_u32 cmd, _driver_io_t *p_dio) {
		switch(cmd) {
			case DIOCTL_SREAD:
				LOG("%s: read from device %d bytes\r\n", DRIVER_IDENT, p_dio->size);
				break;
		}
	}
	
	void driver_uninit(void) {
		// TODO driver deactivation code here
	}

	void object_destroy(void) {
		// TODO object destroy code here
	}
};

IMPLEMENT_BASE(_OBJECT_CLASS_, _OBJECT_NAME_, _OBJECT_VERSION_);
