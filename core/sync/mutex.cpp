#include "iSyncObject.h"
#include "mutex_api.h"

class cMutex:public iMutex {
protected:
	_mutex_t	m_mutex;

public:
  	cMutex() {
		reset();
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}
	
	~cMutex() {}
	
	// iBase
	DECLARE_BASE(cMutex);
	bool object_init(iRepositoryBase *p_rb) {
		REPOSITORY = p_rb;
		mutex_reset(&m_mutex);
		return true;
	}

	// iMutex
	void init(void) {
		mutex_reset(&m_mutex);
	}
	HMUTEX try_lock(HMUTEX hm=0) {
		return mutex_try_lock(&m_mutex, hm);
	}
	HMUTEX lock(HMUTEX hm=0, _u32 timeout=MUTEX_TIMEOUT_INFINITE, _u8 flags=0) {
		return mutex_lock(&m_mutex, hm, timeout, flags);
	}
	void unlock(HMUTEX hm) {
		mutex_unlock(&m_mutex, hm);
	}
	void reset(void) {
		mutex_reset(&m_mutex);
	}
};

IMPLEMENT_BASE(cMutex, "cMutex", 0x0002);
