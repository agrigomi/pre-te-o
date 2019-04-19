#ifndef __I_SERIAL_DEVICE__
#define __I_SERIAL_DEVICE__

#include "iDriver.h"
#include "iMemory.h"

#define I_SD_HOST		"iSDHost"
#define I_CONSOLE		"iConsole"
#define I_KEYBOARD		"iKeyboard"
#define I_USB_CONTROLLER	"iUSBController"

class iSDHost:public iDriverBase {
public:
	DECLARE_INTERFACE(I_SD_HOST, iSDHost, iDriverBase);
};

class iConsole:public iDriverBase {
public:
	DECLARE_INTERFACE(I_CONSOLE, iConsole, iDriverBase);
	
	virtual void init(iDriverBase *p_input, // requires an initialized serial I/O object
			iDriverBase *p_output // requires an initialized serial I/O object
			)=0;
	virtual iDriverBase *get_input(void)=0; // return interface to keyboard object
	virtual iDriverBase *get_output(void)=0; // return interface to display object
};

class iKeyboard:public iDriverBase {
public:
	DECLARE_INTERFACE(I_KEYBOARD, iKeyboard, iDriverBase);
};

#define USB_PI_UHCI	0x00
#define USB_PI_OHCI	0x10
#define USB_PI_EHCI	0x20
#define USB_PI_EXHCI	0x30

/////////////////////////////////////////////////////////////////////////////////////
typedef struct {
	_u8		_usb_prog_if_;
	_u32		_usb_bus_index_;
	_u8		_usb_irq_;
	_vaddr_t	_cr_base_;
	_vaddr_t	_cr_ex_;
}_usb_ctrl_t;

class iUSBController:public iDriverBase, public _usb_ctrl_t {
public:
	DECLARE_INTERFACE(I_USB_CONTROLLER, iUSBController, iDriverBase);
};

#endif

