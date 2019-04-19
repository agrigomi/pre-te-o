#include <stdio.h>
#include "iDisplay.h"
#include "addr.h"
#include "str.h"
#include "iScheduler.h"
#include "iSerialDevice.h"

class cTdisp:public iTextDisplay {
public:
	cTdisp() {
		_d_type_ = DTYPE_DEV;
		_d_mode_ = DMODE_CHAR;
		_d_class_ = DCLASS_DISPLAY;
		str_cpy(_d_ident_, (_str_t)"text display", sizeof(_d_ident_));
		m_max_col = 80;
		m_max_row = 24;
		m_color = 7;
		m_ccol = m_crow = 0;
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}
	~cTdisp() {}
	
	DECLARE_BASE(cTdisp);
	DECLARE_INIT();

	void init(_u16 max_row, _u16 max_col) {
		m_max_col = max_col;
		m_max_row = max_row;
		m_ccol = m_crow = 0;
	}
	DECLARE_DRIVER_INIT();
	void driver_ioctl(_u32 cmd, _driver_io_t *p_dio);
	void driver_uninit() {}
	
	// iTextDisplay
	void color(_u8 color) { 
		m_color = color; 
	}
	
	void position(_u8 row, _u8 col) {
		m_crow = (row <= m_max_row)?row:m_max_row;
		m_ccol = (col <= m_max_col)?col:m_max_col;
	}
	
	void mode(_u8 mode) {}
	void scroll(void) {}
	void clear(void) {}
protected:
	_u16	m_err;
	_u8	m_color;
	_u8	m_max_row;
	_u8	m_max_col;
	_u8	m_crow;
	_u8	m_ccol;
};

IMPLEMENT_BASE(cTdisp, "cTextDisplayDriver", 0x0001);
IMPLEMENT_INIT(cTdisp, rbase) {
	REPOSITORY = rbase;
	INIT_MUTEX();
	REGISTER_DRIVER();
	return true;
}

IMPLEMENT_DRIVER_INIT(cTdisp, host) {
	bool r = false;
	
	if(!(DRIVER_HOST = host)) {
		iDriverRoot *p_dh = GET_DRIVER_ROOT();
		if(p_dh) {
			_driver_request_t dr;
			dr.flags=DRF_INTERFACE;
			dr.DRIVER_INTERFACE = I_SD_HOST;
			DRIVER_HOST = p_dh->driver_request(&dr);
		}
	}
	
	if(DRIVER_HOST)
		r = true;
	
	return r;
}
void cTdisp::driver_ioctl(_u32 cmd, _driver_io_t *p_dio) {
	switch(cmd) {
		case DIOCTL_SWRITE: {
				for(p_dio->result = 0; p_dio->result < p_dio->size; p_dio->result++)
					printf("%c", p_dio->buffer[p_dio->result]);
			}
			break;
		case DIOCTL_VFWRITE: {
				_s8 buf[4096];
				p_dio->result = _vsnprintf(buf, sizeof(buf), p_dio->fmt, *p_dio->args);
				printf("%s", buf);
			}
			break;
		default:
			p_dio->result = DIOCTL_ERRCMD;
	}
}
