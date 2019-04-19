#ifndef __I_RTC__
#define __I_RTC__

#include "iDriver.h"

#define I_RTC	"iRTC"

typedef void _rtc_callback_t(void *p_udata);

class iRTC:public iDriverBase {
public:
	DECLARE_INTERFACE(I_RTC, iRTC, iDriverBase);
	
	virtual _u32 timestamp(void)=0;
	virtual _u8 seconds(void)=0;
	virtual _u8 minutes(void)=0;
	virtual _u8 hours(void)=0;
	virtual _u8 day_of_week(void)=0;
	virtual _u8 day_of_month(void)=0;
	virtual _u8 month(void)=0;
	virtual _u8 year(void)=0;
	virtual void wait(void)=0;
	virtual bool enable_irq(bool enable=true)=0;
	virtual _u32 count_irq(void)=0;
	virtual bool set_callback(_rtc_callback_t *p_callback, void *p_udata)=0;
	virtual void del_callback(_rtc_callback_t *p_callback)=0;
};

#endif
