#include "iDisplay.h"
#include "console.h"
#include "str.h"

//#define _DEBUG_

#define FMT_BUFFER_SIZE	1024

static _u32 _g_tty_count_=0;

IMPLEMENT_BASE(cConsole, "cTextConsole", 0x0001);

IMPLEMENT_DESTROY(cConsole) {
	if(REPOSITORY) {
		RELEASE_OBJECT(m_p_output);
		RELEASE_OBJECT(m_p_input);
	}
}

cConsole::~cConsole() {
	object_destroy();
}

IMPLEMENT_INIT(cConsole, p_rb) {
	REPOSITORY = p_rb;

#ifdef _DEBUG_
	m_p_output = (iTextDisplay *)OBJECT_BY_INTERFACE(I_TEXT_DISPLAY, RY_STANDALONE);
#endif	

	REGISTER_DRIVER();
	
	return true;
}

IMPLEMENT_DRIVER_INIT(cConsole, host) {
	bool r = false;

	if(!(DRIVER_HOST = host)) {
		iDriverRoot *p_dh = GET_DRIVER_ROOT();
		if(p_dh) {
			_driver_request_t dr;
			dr.flags = DRF_INTERFACE;
			dr._d_interface_ = I_SD_HOST;
			_snprintf(DRIVER_IDENT, sizeof(DRIVER_IDENT), "tty%d", _g_tty_count_);
			if((DRIVER_HOST = p_dh->driver_request(&dr)))
				_g_tty_count_++;
			RELEASE_OBJECT(p_dh);
		}
	}
	
	if(DRIVER_HOST)
		r = true;
	
	return r;
}
IMPLEMENT_DRIVER_UNINIT(cConsole) {
}

void cConsole::init(iDriverBase *p_input, // requires an initialized object of iKeyboard
			iDriverBase *p_output // requires an initialized object of iTextDisplay
		   ) {
	
	m_p_input = p_input;
	m_p_output = p_output;
}

_u32 cConsole::tty_read(_u8 *p_buffer, _u32 sz_buffer) {
	_u32 r = 0;

	if(m_p_input)
		m_p_input->read(p_buffer,sz_buffer, &r);
	
	return r;
}

_u32 cConsole::tty_write(_u8 *p_data, _u32 sz) {
	_u32 r = 0;
	
	if(m_p_output)
		m_p_output->write(p_data, sz, &r);
	
	return r;
}

_u32 cConsole::tty_write(_str_t text) {
	_u32 r = 0;
	
	if(m_p_output)
		m_p_output->write((_u8 *)text, str_len(text), &r);
	
	return r;
}

_u32 cConsole::tty_vfwrite(_str_t format, va_list args) {
	_u32 r = 0;
	_s8 lb[FMT_BUFFER_SIZE];
	_vsnprintf(lb, FMT_BUFFER_SIZE, format, args);
	r = tty_write(lb);
	return r;
}

void cConsole::hexdump(_u8 *p_buffer, _u32 sz) {
	_u32 i = 0;
	
	while(i < sz) {
		m_p_output->fwrite((_str_t)"%x ",*(p_buffer + i));
		i++;
	}
}

void cConsole::driver_ioctl(_u32 cmd, _driver_io_t *p_dio) {
	p_dio->error = -1;
	switch(cmd) {
		case DIOCTL_SWRITE:
			if(p_dio->size)
				p_dio->result = tty_write(p_dio->buffer, p_dio->size);
			else
				p_dio->result = tty_write((_str_t)p_dio->buffer);
			p_dio->error = 0;
			break;
		case DIOCTL_VFWRITE:
			p_dio->result = tty_vfwrite((_str_t)p_dio->fmt, *p_dio->args);
			p_dio->error = 0;
			break;
		case DIOCTL_SREAD:
			p_dio->result = tty_read(p_dio->buffer, p_dio->size);
			p_dio->error = 0;
			break;
		default:
			p_dio->result = DIOCTL_ERRCMD;
	}
}

