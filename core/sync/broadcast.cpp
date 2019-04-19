#include "iSyncObject.h"
#include "iDataStorage.h"

class cBroadcast:public iBroadcast {
public:
	cBroadcast() {
		m_p_vector = 0;
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	~cBroadcast() {}

	DECLARE_BASE(cBroadcast);
	DECLARE_INIT();
	DECLARE_DESTROY();

	void add_handler(_msg_handler_t *p_hcb, void *p_data);
	void remove_handler(_msg_handler_t *p_hcb);
	void send_msg(_msg_t *p_msg);

protected:
	iVector	*m_p_vector;	
};

typedef struct {
	_msg_handler_t	*p_hcb;
	void		*p_data;
}_handler_t;

IMPLEMENT_BASE(cBroadcast, "cBroadcast", 0x0001);
IMPLEMENT_INIT(cBroadcast, repo) {
	bool r = false;
	REPOSITORY = repo;
	m_p_vector = (iVector *)OBJECT_BY_INTERFACE(I_VECTOR, RY_CLONE);
	if(m_p_vector) {
		m_p_vector->init(sizeof(_handler_t));
		r = true;
	}
	return r;
}
IMPLEMENT_DESTROY(cBroadcast) {
	if(m_p_vector) {
		m_p_vector->clr();
		RELEASE_OBJECT(m_p_vector);
		m_p_vector = 0;
	}
}

void cBroadcast::add_handler(_msg_handler_t *p_hcb, void *p_data) {
	_handler_t h={p_hcb, p_data};
	if(m_p_vector)
		m_p_vector->add(&h);
}
void cBroadcast::remove_handler(_msg_handler_t *p_hcb) {
	if(m_p_vector) {
		HMUTEX hlock = m_p_vector->lock();
_remove_handler_:
		_u32 cnt = m_p_vector->cnt(hlock);
		for(_u32 i = 0; i < cnt; i++) {
			_handler_t *ph = (_handler_t *)m_p_vector->get(i, hlock);
			if(ph) {
				if(ph->p_hcb == p_hcb) {
					m_p_vector->del(i, hlock);
					goto _remove_handler_;
				}
			}
		}
		m_p_vector->unlock(hlock);
	}
}
void cBroadcast::send_msg(_msg_t *p_msg) {
	if(m_p_vector) {
		_u32 cnt = m_p_vector->cnt();
		for(_u32 i = 0; i < cnt; i++) {
			_handler_t *ph = (_handler_t *)m_p_vector->get(i);
			if(ph)
				ph->p_hcb(p_msg, ph->p_data);
		}
	}
}

