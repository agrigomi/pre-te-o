// dummy driver

#include "iBlockDevice.h"
#include "str.h"

class cBDCtrlHost:public iBDCtrlHost {
public:
	cBDCtrlHost() {
		DRIVER_TYPE = DTYPE_VBUS;
		DRIVER_MODE = 0;
		DRIVER_CLASS = DCLASS_SYS;
		_bus_type_[0]='S';_bus_type_[1]='Y';_bus_type_[2]='S';_bus_type_[3]=0;
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	//virtual ~cBDCtrlHost() {} 
	
	// iBase
	DECLARE_BASE(cBDCtrlHost);
	DECLARE_INIT();
	DECLARE_DRIVER_INIT();
	void driver_uninit(void) {}

protected:
};

IMPLEMENT_BASE(cBDCtrlHost, "cBDCtrlHost", 0x0001);
IMPLEMENT_INIT(cBDCtrlHost, rbase) {
	REPOSITORY = rbase;
	str_cpy(DRIVER_IDENT, (_str_t)"BDC Host", sizeof(DRIVER_IDENT));
	REGISTER_DRIVER(); // self registration
	return true;
}
IMPLEMENT_DRIVER_INIT(cBDCtrlHost, host) {
	if(!(DRIVER_HOST = host))
		DRIVER_HOST = GET_DRIVER_ROOT();
	return true;
}
