#include "iDisplay.h"
#include "addr.h"
#include "str.h"
#include "iScheduler.h"
#include "iSerialDevice.h"

#define VIDEO_BASE 	(_u8 *)0xb8000

class cTextDisplay:public iTextDisplay {
public:
	cTextDisplay() {
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
	
	//virtual ~cTextDisplay();

	DECLARE_BASE(cTextDisplay);
	DECLARE_INIT();

	void init(_u16 max_row, _u16 max_col);
	DECLARE_DRIVER_INIT();
	void driver_uninit(void) {}
	void driver_ioctl(_u32 cmd, _driver_io_t *p_dio);
	
	// iTextDisplay
	void color(_u8 color) { 
		m_color = color; 
	}
	
	void position(_u8 row, _u8 col) {
		m_crow = (row <= m_max_row)?row:m_max_row;
		m_ccol = (col <= m_max_col)?col:m_max_col;
	}
	
	void mode(_u8 mode);
	void scroll(void);
	void clear(void);

protected:
	_u16	m_err;
	_u8	m_color;
	_u8	m_max_row;
	_u8	m_max_col;
	_u8	m_crow;
	_u8	m_ccol;
	bool 	m_bdestroy_mutex;

	_u32 disp_write(_u8 *p_buffer, _u32 sz_buffer);
	_u32 disp_vfwrite(_cstr_t format, va_list args);
};

#define FMT_BUFFER_SIZE	1024

IMPLEMENT_BASE(cTextDisplay, "cTextDisplayDriver", 0x0001);

//cTextDisplay::~cTextDisplay() {
//	object_destroy();
//}

void cTextDisplay::init(_u16 max_row, _u16 max_col) {
	m_max_col = max_col;
	m_max_row = max_row;
	m_ccol = m_crow = 0;
}

IMPLEMENT_INIT(cTextDisplay, rbase) {
	REPOSITORY = rbase;
	INIT_MUTEX();
	REGISTER_DRIVER();
	return true;
}

IMPLEMENT_DRIVER_INIT(cTextDisplay, host) {
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

_u32 cTextDisplay::disp_write(_u8 *p_buffer, _u32 sz_buffer) {
	_u32 r = 0;
	_u32 _sz = sz_buffer;
	_u8 *base = (_u8 *)p_to_k(VIDEO_BASE);

#ifndef _DEBUG_
	bool irqs = enable_interrupts(false);
#endif	
	
	HMUTEX hm = lock();
	while(_sz) {
		if(*(p_buffer + r) == 0x0d) {
			m_ccol = 0;
		} else {
			if(*(p_buffer + r) == 0x0a) {
				m_crow++;
				if(m_crow > m_max_row) {
					scroll();
					m_crow = m_max_row;
				}
			} else {
				_u8 *vptr = (_u8 *)(base + (((m_crow * m_max_col) + m_ccol) * 2));
				if(*(p_buffer + r) != '\t') {
					*vptr = *(p_buffer + r);
					*(vptr + 1) = m_color;
				}
				m_ccol++;
				if(m_ccol >= m_max_col) {
					m_ccol = 0;
					m_crow++;
					if(m_crow > m_max_row) {
						scroll();
						m_crow = m_max_row;
					}
				}
			}
		}
		
		_sz--;
		r++;
	}
	unlock(hm);

	#ifndef _DEBUG_
	enable_interrupts(irqs);
#endif	
	
	return r;
}

_u32 cTextDisplay::disp_vfwrite(_cstr_t format, va_list args) {
	_s8 lb[FMT_BUFFER_SIZE];
	_u32 sz = _vsnprintf(lb, FMT_BUFFER_SIZE, format, args);
	return disp_write((_u8 *)lb, sz);
}

void cTextDisplay::mode(_u8 mode) {
	switch(mode) {
		case 1:
			break;
		case 2:
			break;
	}
}

void cTextDisplay::scroll(void) {
	_u8 *base = (_u8 *)p_to_k(VIDEO_BASE);
	_u8 *src = (_u8 *)(base + (m_max_col  * 2));
	_u8 *dst = (_u8 *)base;
	_u32 sz = (((m_max_row + 1) * m_max_col) * 2);
	
	// scroll video buffer
	mem_cpy(dst,src,sz);
}

void cTextDisplay::clear(void) {
	_u8 *base = (_u8 *)p_to_k(VIDEO_BASE);
	_u16 *ptr = (_u16 *)base;
	_u32 sz = m_max_row * m_max_col;
	_u16 pattern = m_color;
	
	pattern = (pattern << 8)|0x20;
	
	while(sz) {
		*ptr++ = pattern;
		sz--;
	}
	
	m_crow = m_ccol = 0;
}

void cTextDisplay::driver_ioctl(_u32 cmd, _driver_io_t *p_dio) {
	switch(cmd) {
		case DIOCTL_SWRITE:
			p_dio->result = disp_write(p_dio->buffer, p_dio->size);
			break;
		case DIOCTL_VFWRITE:
			p_dio->result = disp_vfwrite(p_dio->fmt, *p_dio->args);
			break;
		default:
			p_dio->result = DIOCTL_ERRCMD;
	}
}
