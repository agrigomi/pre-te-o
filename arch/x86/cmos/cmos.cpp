#include "iCMOS.h"
#include "io_port.h"

class cCMOS:public iCMOS {
public:
	cCMOS() {
		m_p_mutex = 0;
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}

	//virtual ~cCMOS() {}

	DECLARE_BASE(cCMOS);
	DECLARE_INIT();
	DECLARE_DESTROY();

	_u8 read(_u8 reg, bool _lock=true, HMUTEX hlock=0);
	void write(_u8 reg, _u8 value, bool _lock=true, HMUTEX hlock=0);
protected:
	iMutex	*m_p_mutex;

	HMUTEX lock(HMUTEX hlock=0, _u32 timeout=MUTEX_TIMEOUT_INFINITE);
	void unlock(HMUTEX hlock);
};

IMPLEMENT_BASE(cCMOS, "cCMOS", 0x0001);
IMPLEMENT_INIT(cCMOS, rbase) {
	bool r = false;
	REPOSITORY = rbase;
	m_p_mutex = (iMutex *)OBJECT_BY_INTERFACE(I_MUTEX, RY_CLONE);
	if(m_p_mutex)
		r = true;
	return r;
}
IMPLEMENT_DESTROY(cCMOS) {
	if(m_p_mutex) {
		RELEASE_OBJECT(m_p_mutex);
		m_p_mutex = 0;
	}
}

HMUTEX cCMOS::lock(HMUTEX hlock, _u32 timeout) {
	HMUTEX r = 0;
	if(m_p_mutex)
		r = m_p_mutex->lock(hlock, timeout);
	return r;
}
void cCMOS::unlock(HMUTEX hlock) {
	if(m_p_mutex)
		m_p_mutex->unlock(hlock);
}

_u8 cCMOS::read(_u8 reg, bool _lock, HMUTEX hlock) {
	_u8 r = 0;
	HMUTEX hm = 0;
	if(_lock)
		hm = lock(hlock,10000);
	outportb(CMOS_REG_SEL, reg);
	r = inportb(CMOS_REG_VALUE);
	if(_lock)
		unlock(hm);
	return r;
}

void cCMOS::write(_u8 reg, _u8 value, bool _lock, HMUTEX hlock) {
	HMUTEX hm = 0;
	if(_lock)
		hm = lock(hlock, 10000);
	outportb(CMOS_REG_SEL, reg);
	outportb(CMOS_REG_VALUE, value);
	if(_lock)
		unlock(hm);
}
