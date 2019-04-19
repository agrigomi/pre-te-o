#include "iDriver.h"
#include "str.h"

#define _OBJECT_NAME_		"cMyDriverName"
#define _OBJECT_CLASS_		cMyDriverClass
#define _OBJECT_INTERFACE_	iDriverBase
#define _OBJECT_TYPE_		RY_ONCE // or RY_CLONEABLE
#define _OBJECT_ID_		RY_NO_OBJECT_ID // or ID
#define _OBJECT_VERSION_	0x0001

#define _DRIVER_TYPE_		DTYPE_DEV // see iDriver.h
#define _DRIVER_MODE_		DMODE_CHAR
#define _DRIVER_CLASS_		DCLASS_PORT
#define _DRIVER_IDENT_		"MyDriver"

class _OBJECT_CLASS_:public _OBJECT_INTERFACE_ {
private:
	// TODO private members
protected:
	// TODO protected members	
public:
	_OBJECT_CLASS_() {
		DRIVER_TYPE = _DRIVER_TYPE_;
		DRIVER_MODE = _DRIVER_MODE_;
		DRIVER_CLASS= _DRIVER_CLASS_;
		str_cpy(DRIVER_IDENT, (_str_t)_DRIVER_IDENT_, sizeof(DRIVER_IDENT));

		// TODO initialization of class members

		REGISTER_OBJECT(_OBJECT_TYPE_, _OBJECT_ID_);
	}
	~_OBJECT_CLASS_() {
	}

	// declare base members
	DECLARE_BASE(_OBJECT_CLASS_);
	
	bool object_init(iRepositoryBase *p_rb) {
		REPOSITORY = p_rb;
		// TODO object initialization here and return true if success
		REGISTER_DRIVER();
		return true;
	}

	bool driver_init(iDriverBase *p_host) {
		if(!(DRIVER_HOST = p_host)) {
			// use 'driver_request' to find host for this driver
		} 
		return iDriverBase::driver_init(DRIVER_HOST);
	}

	void driver_ioctl(_u32 cmd, _driver_io_t *p_dio) {
		// TODO driver I/O code here (see iDriver.h)
	}
	
	void driver_uninit(void) {
		// TODO driver deactivation code here
	}

	void object_destroy(void) {
		// TODO object destroy code here
	}
};

IMPLEMENT_BASE(_OBJECT_CLASS_, _OBJECT_NAME_, _OBJECT_VERSION_);
