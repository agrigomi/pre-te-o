#ifndef __IDMA_H__
#define __IDMA_H__

#include "iDriver.h"

#define I_DMA	"iDMA"

typedef void*	HDMA;

//mode
#define	DMA_MODE_DEV_TO_MEM	(1<<2)
#define DMA_MODE_MEM_TO_DEV	(1<<3)
#define DMA_MODE_AUTO_RESET	(1<<4)
#define DMA_MODE_DOWN		(1<<5)
#define DMA_MODE_SINGLE		(1<<6)
#define DMA_MODE_BLOCK		(1<<7)

//errors
#define DMA_ERROR_CHLOCK	-1	// the channel is already oppen
#define DMA_ERROR_MEMORY	-2	// memory muffer cannot be allocated
#define DMA_ERROR_CHANNEL	-3	// invalid channel number

#define INVALID_DMA_HANDLE	0

class iDMA: public iDriverBase {
public:
	DECLARE_INTERFACE(I_DMA, iDMA, iDriverBase);

	virtual HDMA open(_u8 channel, _u32 bcount, _u8 *addr=0, _u8 irq=0xff)=0;
	virtual void start(HDMA hc, _u8 mode, _u32 bcount=0, _u8 *addr=0)=0;
	virtual _u8 *buffer(HDMA hc)=0;
	virtual void close(HDMA hc)=0;
};

#endif

