#include "iBase.h"

#define I_MY_INTERFACE		"iMyInterface"
#define _INTERFACE_CLASS_	iMyInterface
#define _BASE_INTERFACE_	iBase

class _INTERFACE_CLASS_: public _BASE_INTERFACE_ {
public:
	// Interface definotion macro	
	DECLARE_INTERFACE(I_MY_INTERFACE, _INTERFACE_CLASS_, _BASE_INTERFACE_);

	// TODO: public methods (virtual) ...
};
