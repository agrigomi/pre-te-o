#ifndef __DEBUG_H__
#define __DEBUG_H__

#ifdef _DEBUG_ 
	#include "iDisplay.h" 
	#include "repository.h"
	static iTextDisplay *_g_p_disp_=0; 
	#define INIT_DEBUG() \
		if(!_g_p_disp_) \
			_g_p_disp_ = (iTextDisplay *)__g_p_repository__->get_object_by_interface(I_TEXT_DISPLAY, RY_STANDALONE);	

	#define DBG(_fmt_, ...)  {\
		INIT_DEBUG() \
		if(_g_p_disp_) \
			_g_p_disp_->fwrite(_fmt_, __VA_ARGS__); \
	}
#else
	#define INIT_DEBUG()	;
	#define DBG(_fmt_, ...)	;
#endif //_DEBUG_
#endif

