#include "iDMA.h"
#include "iMemory.h"
#include "iSystemLog.h"
#include "io_port.h"
#include "addr.h"
#include "iScheduler.h"

#define DMA_LA_BASE		0x1000
#define DMA_LA_BLOCKS		54
#define DMA_LA_BLOCK_SIZE	512
#define DMA_MAX_CHANNELS 	8
#define DMA_HLIMIT		k_to_p(16*(1024*1024))

class cDMA:public iDMA {
public:
	cDMA() {
		DRIVER_TYPE=DTYPE_DEV;
		DRIVER_CLASS=DCLASS_SYS;
		DRIVER_IDENT[0]='D';DRIVER_IDENT[1]='M';DRIVER_IDENT[2]='A';DRIVER_IDENT[3]=0;
		m_labmp = 0;
		m_p_syslog = 0;
		m_p_heap = 0;
		REGISTER_OBJECT(RY_STANDALONE, RY_NO_OBJECT_ID);
	}

	//virtual ~cDMA() {}

	DECLARE_BASE(cDMA);
	DECLARE_INIT();
	DECLARE_DRIVER_INIT();

	HDMA open(_u8 channel, _u32 bcount, _u8 *addr=0, _u8 irq=0xff);
	void start(HDMA hc, _u8 mode, _u32 bcount=0, _u8 *addr=0);
	_u8 *buffer(HDMA hc);
	void close(HDMA hc);
	void driver_uninit(void) {}

protected:
	iSystemLog	*m_p_syslog;
	iHeap		*m_p_heap;
	_u64		m_labmp;

	_u8 *alloc_labuffer(_u32  bcount, HMUTEX hm=0);
	void release_labuffer(_u8 *p_buffer, _u32 bcount, HMUTEX hm=0);
};

IMPLEMENT_BASE(cDMA, "cDMA", 0x0001);
IMPLEMENT_INIT(cDMA, repo) {
	REPOSITORY = repo;
	m_p_syslog = (iSystemLog *)OBJECT_BY_INTERFACE(I_SYSTEM_LOG, RY_STANDALONE);
	m_p_heap = (iHeap *)OBJECT_BY_INTERFACE(I_HEAP, RY_STANDALONE);
	INIT_MUTEX();
	REGISTER_DRIVER();
	return true;
}
IMPLEMENT_DRIVER_INIT(cDMA, host) {
	bool r = false;

	if(!(DRIVER_HOST = host)) {
		if((DRIVER_HOST = GET_DRIVER_ROOT()))
			r = true;
	}

	return r;
}

typedef struct {
	_u8	channel;
	_u8	addr_port;
	_u8	count_port;
	_u8	addrex_port;
	_u32	bcount;
	_u8	*addr;
	_u8	irq;
	bool	open;
}_dma_channel_t;

static _dma_channel_t _g_dc_[]={
	{0,DMA8_CH0_ADDR,  DMA8_CH0_COUNT,  DMA_CH0_PAGE, 0, 0, 0xff, true},
	{1,DMA8_CH1_ADDR,  DMA8_CH1_COUNT,  DMA_CH1_PAGE, 0, 0, 0xff, false},
	{2,DMA8_CH2_ADDR,  DMA8_CH2_COUNT,  DMA_CH2_PAGE, 0, 0, 0xff, false},
	{3,DMA8_CH3_ADDR,  DMA8_CH3_COUNT,  DMA_CH3_PAGE, 0, 0, 0xff, false},
	{4,DMA16_CH4_ADDR, DMA16_CH4_COUNT, DMA_CH4_PAGE, 0, 0, 0xff, true},
	{5,DMA16_CH5_ADDR, DMA16_CH5_COUNT, DMA_CH5_PAGE, 0, 0, 0xff, false},
	{6,DMA16_CH6_ADDR, DMA16_CH6_COUNT, DMA_CH6_PAGE, 0, 0, 0xff, false},
	{7,DMA16_CH7_ADDR, DMA16_CH7_COUNT, DMA_CH7_PAGE, 0, 0, 0xff, false}
};

_u8 *cDMA::alloc_labuffer(_u32  bcount, HMUTEX hm) {
	_u8 *r = 0;
	_u32 nblocks = bcount / DMA_LA_BLOCK_SIZE;
	nblocks += (bcount % DMA_LA_BLOCK_SIZE) ? 1 : 0;
	_u64 mask = 0x8000000000000000;
	_u64 slice = 0;
	_u8 fbit = 0xff;
	_u8 count = 0;

	HMUTEX hlock = acquire_mutex(DRIVER_MUTEX, hm);

	for(_u8 i = 0; i < DMA_LA_BLOCKS && count < nblocks; i++) {
		if(!(m_labmp & mask)) {
			if(fbit == 0xff)
				fbit = i;
			count++;
			slice |= mask;
		} else {
			fbit = 0xff;
			count = 0;
			slice = 0;
		}

		mask >>= 1;
	}

	if(fbit != 0xff && count == nblocks) {
		r = (_u8 *)(_u64)(DMA_LA_BASE + (fbit*DMA_LA_BLOCK_SIZE));
		m_labmp |= slice;
	}

	unlock(hlock);

	return r;
}

void cDMA::release_labuffer(_u8 *p_buffer, _u32 bcount, HMUTEX hm) {
	_u32 nblocks = bcount / DMA_LA_BLOCK_SIZE;
	nblocks += (bcount % DMA_LA_BLOCK_SIZE) ? 1 : 0;
	_u8 fblock = (_u8)(((_ulong)p_buffer - DMA_LA_BASE)/DMA_LA_BLOCK_SIZE);

	HMUTEX hlock = acquire_mutex(DRIVER_MUTEX, hm);
	_u64 mask = 0x8000000000000000>>fblock;

	for(_u8 i = 0; i < nblocks; i++) {
		m_labmp &= ~mask;
		mask >>= 1;
	}

	unlock(hlock);
}

HDMA cDMA::open(_u8 channel, _u32 bcount, _u8 *addr, _u8 irq) {
	HDMA r = INVALID_DMA_HANDLE;

	if(channel < DMA_MAX_CHANNELS) {
		if(_g_dc_[channel].open == false) {
			HMUTEX hlock = acquire_mutex(DRIVER_MUTEX);
			_g_dc_[channel].bcount = bcount-1;
			if(addr) {
				_u32 _addr = (_u32)k_to_p((_ulong)addr);
				if((_addr+ bcount) < 0x1000000) {
					_g_dc_[channel].addr = (_u8 *)(_u64)_addr;
					r = &_g_dc_[channel];
				}
			} else {
				if(channel < 4) {
					// alloc low memory buffer
					if((_g_dc_[channel].addr = alloc_labuffer(bcount, hlock)))
						r = &_g_dc_[channel];
					else
						ERROR_CODE = DMA_ERROR_MEMORY;
				} else {
					if((_g_dc_[channel].addr = (_u8 *)k_to_p((_ulong)m_p_heap->alloc(bcount, DMA_HLIMIT))))
						r = &_g_dc_[channel];
					else
						ERROR_CODE = DMA_ERROR_MEMORY;
				}
			}

			if(r != INVALID_DMA_HANDLE) {
				_g_dc_[channel].open = true;
				_g_dc_[channel].irq = irq;
				ERROR_CODE = 0;
			}
			unlock(hlock);
		} else
			ERROR_CODE = DMA_ERROR_CHLOCK;
	} else
		ERROR_CODE = DMA_ERROR_CHANNEL;

	return r;
}

void cDMA::start(HDMA hc, _u8 mode, _u32 bcount, _u8 *addr) {
	_dma_channel_t *pc = (_dma_channel_t *)hc;
	if(pc >= &_g_dc_[0] && pc <= &_g_dc_[DMA_MAX_CHANNELS-1]) {
		bool irqs = enable_interrupts(false);
		HMUTEX hlock = lock();
		_u32 _addr = 0;
		if(addr)
			_addr = (_u32)(k_to_p((_ulong)addr));
		else
			_addr = (_u32)(_ulong)pc->addr;

		_u32 _bcount = bcount;
		if(!_bcount)
			_bcount = pc->bcount;

		// channel selection
		_u8 ch_sel = (pc->channel)>>2;
		_u8 reg_mask = DMA16_SCMASK;
		_u8 reg_flip_flop = DMA16_FLIPFLOP;
		_u8 reg_mode = DMA16_MODE;

		if(pc->channel < 4) {
			// 8 bit issue
			ch_sel = pc->channel;
			reg_mask = DMA8_SCMASK;
			reg_flip_flop = DMA8_FLIPFLOP;
			reg_mode = DMA8_MODE;

			outportb(reg_mask, (1<<2)|ch_sel); // mask DMA channel
			outportb(reg_flip_flop, 0xff); // reset master flip-flop
			// set address
			outportb(pc->addr_port, (_u8)(_addr & 0xff));
			outportb(pc->addr_port, (_u8)((_addr & 0xffff)>>8));
			outportb(pc->addrex_port, (_u8)(_addr>>16));
			outportb(reg_flip_flop, 0xff); // reset master flip-flop (again !!!)
			// set count
			outportb(pc->count_port, (_u8)(_bcount & 0xff));
			outportb(pc->count_port, (_u8)((_bcount & 0xffff)>>8));
			outportb(reg_mode, mode);
			outportb(reg_mask, ch_sel); // unmask DMA channel
		} else {
			// 16 bit issue
			//???
		}

		unlock(hlock);
		enable_interrupts(irqs);
	}
}

_u8 *cDMA::buffer(HDMA hc) {
	_u8 *r = 0;

	_dma_channel_t *pc = (_dma_channel_t *)hc;
	if(pc >= &_g_dc_[0] && pc <= &_g_dc_[DMA_MAX_CHANNELS-1])
		r = (_u8 *)p_to_k((_ulong)pc->addr);

	return r;
}

void cDMA::close(HDMA hc) {
	_dma_channel_t *pc = (_dma_channel_t *)hc;
	if(pc >= &_g_dc_[0] && pc <= &_g_dc_[DMA_MAX_CHANNELS-1]) {
		if(pc->addr) {
			if(pc->channel > 4)
				m_p_heap->free((_u8 *)p_to_k((_ulong)pc->addr), pc->bcount);
			else
				release_labuffer(pc->addr, pc->bcount);

			pc->addr = 0;
		}

		pc->open = false;
	}
}

