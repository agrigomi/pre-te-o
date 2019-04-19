#include "interface.h"

#define _OBJECT_NAME_		"cMyObjectName"
#define _OBJECT_CLASS_		cMyObjectClass
#define _OBJECT_INTERFACE_	iMyInterface
#define _OBJECT_TYPE_		RY_CLONEABLE // or RY_ONCE
#define _OBJECT_ID_		RY_NO_OBJECT_ID // or ID
#define _OBJECT_VERSION_	0x0001

class _OBJECT_CLASS_:public _OBJECT_INTERFACE_ {
private:
	// TODO private members
protected:
	// TODO protected members	
public:
	_OBJECT_CLASS_() {
		// TODO initialization of class members
		REGISTER_OBJECT(_OBJECT_TYPE_, _OBJECT_ID_);
	}
	~_OBJECT_CLASS_() {
	}

	// declare base members
	DECLARE_BASE(_OBJECT_CLASS_);
	
	bool object_init(iRepositoryBase *p_rb) {
		REPOSITORY = p_rb;
		// TODO object initialization here and return true if success
		return true;
	}
	void object_destroy(void) {
		// TODO object destroy code here
	}

	// TODO public methods ...
};

IMPLEMENT_BASE(_OBJECT_CLASS_, _OBJECT_NAME_, _OBJECT_VERSION_);
