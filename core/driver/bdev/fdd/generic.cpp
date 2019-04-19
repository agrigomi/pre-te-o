#include "iBlockDevice.h"
#include "str.h"
#include "iSystemLog.h"
#include "io_port.h"
#include "iSyncObject.h"
#include "intdef.h"
#include "floppy.h"
#include "iScheduler.h"
#include "iCPUHost.h"
#include "ctrldef.h"

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


class cGenericFDController:public iFDController {
public:
	cGenericFDController() {
		DRIVER_TYPE = DTYPE_CTRL;
		DRIVER_MODE = DMODE_BLOCK;
		DRIVER_CLASS = DCLASS_FDC;
		ERROR_CODE = 0;
		_ctrl_type_[0]='F';_ctrl_type_[1]='D';_ctrl_type_[2]='C';_ctrl_type_[3]=0;
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}

	//virtual ~cGenericFDController() {}

	// iBase
	DECLARE_BASE(cGenericFDController);
	DECLARE_INIT();
	DECLARE_DESTROY();
	
	DECLARE_DRIVER_INIT();
	DECLARE_DRIVER_UNINIT();

	_u8 read_reg(_u32 reg);
	void write_reg(_u32 reg, _u8 value);
	bool wait_irq(_u32 timeout=EVENT_TIMEOUT_INFINITE);
	void enable_irq(bool eirq=true);

protected:
	iSystemLog 	*m_p_syslog;
	iEvent		*m_p_event;

	void setup_irq(void);
	void setup_devices(void);
	void irq_handler(void);
	void select(_u8 dev_idx);
	friend void isr_floppy(iCPU *p_cpu, void *p_cpu_state, void *p_data);
};

IMPLEMENT_BASE(cGenericFDController, FD_CTRL_NAME_GENERIC, 0x0001);
IMPLEMENT_INIT(cGenericFDController, rbase) {
	REPOSITORY = rbase;
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	m_p_event = (iEvent *)OBJECT_BY_INTERFACE(I_EVENT, RY_CLONE);
	INIT_MUTEX();
	return true;
}
IMPLEMENT_DRIVER_INIT(cGenericFDController, host) {
	bool r = false;
	DRIVER_HOST = host;

	if(DRIVER_HOST) {
		str_cpy(DRIVER_IDENT, (_str_t)"FDCtrl", sizeof(DRIVER_IDENT));
		setup_irq();
		setup_devices();
		r = true;
	}

	return r;
}
IMPLEMENT_DRIVER_UNINIT(cGenericFDController) {
	iCPUHost *p_cpu_host = (iCPUHost *)OBJECT_BY_INTERFACE(I_CPU_HOST, RY_STANDALONE);
	if(p_cpu_host)  { // release IRQ interrupt handler
		p_cpu_host->set_isr(INT_FLOPPY, 0, 0);
		RELEASE_OBJECT(p_cpu_host);
	}
}
IMPLEMENT_DESTROY(cGenericFDController) {
	RELEASE_OBJECT(m_p_syslog);
	RELEASE_OBJECT(m_p_event);
	m_p_event = 0;
	m_p_syslog = 0;
	RELEASE_MUTEX();
}

void cGenericFDController::irq_handler(void) {
	DBG("Floppy IRQ(%d)\r\n",IRQ_FLOPPY);
	if(m_p_event)
		m_p_event->set(1);
}

void isr_floppy(iCPU _UNUSED_ *p_cpu, void _UNUSED_ *p_cpu_state, void *p_data) {
	cGenericFDController *p_ctrl = (cGenericFDController *)p_data;
	p_ctrl->irq_handler();
}

void cGenericFDController::setup_irq(void) {
	iCPUHost *p_cpu_host = (iCPUHost *)OBJECT_BY_INTERFACE(I_CPU_HOST, RY_STANDALONE);
	if(p_cpu_host) {
		iCPU *p_cpu = p_cpu_host->get_boot_cpu();
		p_cpu_host->set_irq(IRQ_FLOPPY, p_cpu->cpu_id(), INT_FLOPPY);
		p_cpu_host->set_isr(INT_FLOPPY, isr_floppy, this);
		if(m_p_event)
			m_p_event->init();
		RELEASE_OBJECT(p_cpu_host);
	}
}

bool cGenericFDController::wait_irq(_u32 timeout) {
	bool r = false;

	if(m_p_event) {
		if(wait_event(m_p_event, 1, timeout) == 1)
			r = true;
#ifdef _DEBUG_
		else
			DBG("FDC: wait_irq timeout\r\n","");
#endif
	}

	return r;
}

void cGenericFDController::enable_irq(bool eirq) {
	_fdc_dor_t dor;

	dor.value = read_reg(DIGITAL_OUTPUT_REGISTER);
	dor.value = (eirq)?1:0;
	write_reg(DIGITAL_OUTPUT_REGISTER, dor.value);
}

void cGenericFDController::select(_u8 dev_idx) {
	_fdc_dor_t dor;

	dor.value = read_reg(DIGITAL_OUTPUT_REGISTER);
	if(dor.dselect != dev_idx) {
		dor.dselect = dev_idx;
		write_reg(DIGITAL_OUTPUT_REGISTER, dor.value);
	}
}

typedef struct {
	_u8	id;
	_u8	tracks;
	_u8	spt;
	_u16	sectors;
	_u8	data_rate;
	_u8	gap;
	_cstr_t	text;
}_fdd_type_t;

_fdd_type_t _g_fdd_type_[]={
	{1,	40,	9,	720,	0,	0x2a,	"360kb 5.25in"},
	{2,	80,	15,	2400,	0,	0x1b,	"1.2mb 5.25in"},
	{3,	80,	9,	1440,	0,	0x2a,	"720kb 3.5in"},
	{4,	80,	18,	2880,	0,	0x1b,	"1.44mb 3.5in"},
	{5,	80,	36,	5760,	3,	0x1b,	"2.88mb 3.5in"},
	{0,	0,	0,	0,	0,	0,	0}
};

void cGenericFDController::setup_devices(void) {
	_u8 fdd_set[3]={0,0,0};

	fdd_set[0] = _fdc_detect_ >> 4;
	fdd_set[1] = _fdc_detect_ & 0x0f;

	iDriverRoot *p_dr = GET_DRIVER_ROOT();
	if(p_dr) {
		for(_u8 i = 0; i < 3; i++) {
			if(fdd_set[i]) {
				_u8 n = 0;
				while(_g_fdd_type_[n].id) {
					if(_g_fdd_type_[n].id == fdd_set[i]) {
						iFDDevice *p_fd = (iFDDevice *)OBJECT_BY_INTERFACE(I_FD_DEVICE, RY_CLONE);
						if(p_fd) {
							p_fd->_fdd_index_  = i;
							p_fd->_fdd_data_rate_ = _g_fdd_type_[n].data_rate;
							p_fd->_fdd_tracks_ = _g_fdd_type_[n].tracks;
							p_fd->_fdd_heads_ = 2;
							p_fd->_fdd_spt_ = _g_fdd_type_[n].spt;
							p_fd->_fdd_sectors_ = _g_fdd_type_[n].sectors;
							p_fd->_fdd_gap_ = _g_fdd_type_[n].gap;
							p_fd->_fdd_ident_ = _g_fdd_type_[n].text;
							p_dr->register_driver(p_fd, this);
							break;
						}
					}
					n++;
				}
			}
		}

		RELEASE_OBJECT(p_dr);
	}
}

_u8 cGenericFDController::read_reg(_u32 reg) {
	return inportb(_fdc_base_+reg);
}

void cGenericFDController::write_reg(_u32 reg, _u8 value) {
	outportb(_fdc_base_+reg, value);
}

