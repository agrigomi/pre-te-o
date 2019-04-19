#include "iSerialDevice.h"

class cSDHost:public iSDHost {
public:
	cSDHost() {
		DRIVER_TYPE=DTYPE_VBUS;
		DRIVER_MODE=0;
		DRIVER_CLASS=DCLASS_SYS;
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	//virtual ~cSDHost() {}
	
	// iBase
	DECLARE_BASE(cSDHost);
	DECLARE_INIT();
	DECLARE_DRIVER_INIT();
	void driver_uninit(void) {}
};

IMPLEMENT_BASE(cSDHost, "cSDHost", 0x0001);
IMPLEMENT_INIT(cSDHost, rbase) {
	REPOSITORY = rbase;
	str_cpy(DRIVER_IDENT, (_str_t)"SD Host", sizeof(DRIVER_IDENT));
	REGISTER_DRIVER(); // self registration
	return true;
}
IMPLEMENT_DRIVER_INIT(cSDHost, host) {
	if(!(DRIVER_HOST = host))
		DRIVER_HOST = GET_DRIVER_ROOT();
	return true;
}
