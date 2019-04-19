#include "iSyncObject.h"
#include "atom.h"

class cEvent:public iEvent {
public:
	cEvent() {
		m_state = 0;
		m_p_wait = 0;
		REGISTER_OBJECT(RY_CLONEABLE|RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	
	~cEvent() {}
	
	void init(_t_event_wait *p_cb_wait=0) {
		m_state = 0;
		m_p_wait = p_cb_wait;
	}
	_u32 check(_u32 mask=EVENT_DEFAULT_MASK) {
		_u32 r = m_state & ~mask;
		ATOM_EXCHANGE_L(r, m_state);
		if(mask)
			r &= mask;
		return r;
	}
	_u32 wait(_u32 mask=EVENT_DEFAULT_MASK, _u32 timeout=EVENT_TIMEOUT_INFINITE) {
		_u32 r = 0;
		_u32 _timeout = timeout;
		while(_timeout) {
			if((r = check(mask)))
				break;
			else {
				if(timeout != EVENT_TIMEOUT_INFINITE)
					_timeout--;
				if(m_p_wait)
					m_p_wait();
			}
		}
		return r;
	}
	void set(_u32 mask) {
		_u32 r = mask | m_state;
		ATOM_EXCHANGE_L(r, m_state);
	}
	
	// iBase
	DECLARE_BASE(cEvent);

	bool object_init(iRepositoryBase *p_rb) {
		REPOSITORY = p_rb;
		m_state = 0;
		return true;
	}

protected:
	volatile _u32 m_state;
	_t_event_wait *m_p_wait;
};

IMPLEMENT_BASE(cEvent, "cEvent", 0x0002);
