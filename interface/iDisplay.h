#ifndef __I_DISPLAY_DRIVER__
#define __I_DISPLAY_DRIVER__

#define I_TEXT_DISPLAY	"iTextDisplay"

#include "iDriver.h"

class iTextDisplay:public iDriverBase {
public:
	DECLARE_INTERFACE(I_TEXT_DISPLAY, iTextDisplay, iDriverBase);
	virtual void init(_u16 max_row, _u16 max_col)=0;
	virtual void color(_u8 color)=0;
	virtual void position(_u8 row, _u8 col)=0;
	virtual void mode(_u8 mode)=0;
	virtual void scroll(void)=0;
	virtual void clear(void)=0;
};

#endif
