#include "iSerialDevice.h"
#include "str.h"

#define DEFAULT_COLOR	0x07
#define VIDEO_ADDR	0xb8000
#define MAX_ROW		25
#define MAX_COL		80

class cConsole:public iConsole {
public:
	cConsole() {
		m_p_input = 0;
		m_p_output = 0;
		m_p_rb = 0;
		DRIVER_MODE = DMODE_CHAR;
		DRIVER_TYPE = DTYPE_DEV;
		REGISTER_OBJECT(RY_CLONEABLE, RY_NO_OBJECT_ID);
	}
	
	~cConsole();

	// iBase
	DECLARE_BASE(cConsole);
	DECLARE_INIT();
	DECLARE_DESTROY();
	
	// iDriverbase
	DECLARE_DRIVER_INIT();
	DECLARE_DRIVER_UNINIT();
	
	// iSerialDevice
	void init(iDriverBase *p_inpit, // requires an initialized object of iKeyboard
		  iDriverBase *p_output // requires an initialized object of iTextDisplay
		 );
		
	void hexdump(_u8 *p_buffer, _u32 sz);
	
	iDriverBase *get_input(void) {
		return m_p_input;
	}
	
	iDriverBase *get_output(void) {
		return m_p_output;
	}
	
	void driver_ioctl(_u32 cmd, _driver_io_t *p_dio);
	
protected:
	iDriverBase 	*m_p_input;
	iDriverBase 	*m_p_output;
	iRepositoryBase	*m_p_rb;

	_u16		m_err;

	_u32 tty_read(_u8 *p_buffer, _u32 sz_buffer);
	_u32 tty_write(_u8 *p_data, _u32 sz);
	_u32 tty_write(_str_t text);
	_u32 tty_vfwrite(_str_t format, va_list args);
	
};
