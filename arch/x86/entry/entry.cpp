#include "mgtype.h"
#include "startup_context.h"
#include "repository.h"
#include "iMemory.h"
#include "karg_parser.h"
#include "str.h"
#include "iDisplay.h"
#include "iSerialDevice.h"
#include "iBlockDevice.h"
#include "iSystemLog.h"
#include "iCPUHost.h"
#include "iRTC.h"
#include "iVFS.h"
#include "addr.h"

#define DEFAULT_HEAP_SIZE	(1024 * 1024) * 8	// 8M
#define DEFAULT_LOG_SIZE	2
#define DEFAULT_PAGE_SIZE	4096
// DMA memory object can manage up to 1M
#define DEFAULT_DMA_SIZE	(65536 * 4) // 256 K


extern _startup_context_t *__g_p_startup_context__;

extern void setup_pci_controllers(void);
extern void setup_fd_controller(void);

_s8 _g_kargs_[128]="";

bool setup_system_heap(void) {
	bool r = false;
	_u32 hs = int_parameter((_str_t)__g_p_startup_context__->p_kargs, HEAP_SIZE);
	iHeap *p_heap = (iHeap *)__g_p_repository__->get_object_by_interface(I_HEAP, RY_STANDALONE);
	if(p_heap)
		r = p_heap->init(hs);

	return r;
}

void setup_system_log(void) {
	// initialize display driver
	iTextDisplay *p_out = (iTextDisplay *)__g_p_repository__->get_object_by_interface(I_TEXT_DISPLAY, 
											RY_STANDALONE);
	if(p_out) {
		p_out->init(24,80);
		p_out->position(24,0);
		p_out->color(0x07);
	}

	_u32 page_size = DEFAULT_PAGE_SIZE;
	iPMA *p_pma = (iPMA *)__g_p_repository__->get_object_by_interface(I_PMA, RY_STANDALONE);
	if(p_pma) {
		_ram_info_t ri;
		p_pma->info(&ri);
		page_size = ri.ps;
	}
	iSystemLog *syslog = (iSystemLog *)__g_p_repository__->get_object_by_interface(I_SYSTEM_LOG, 
											RY_STANDALONE);
	if(syslog) {
		_u32 ls = int_parameter((_str_t)__g_p_startup_context__->p_kargs, LOG_SIZE);
		_u32 log_pages = ls/page_size;
		log_pages += (ls % page_size)?1:0;
		syslog->init((log_pages)?log_pages:DEFAULT_LOG_SIZE);
	}
}

void setup_console(void) {
	// create console instance
	iDriverRoot *p_dr = (iDriverRoot *)__g_p_repository__->get_object_by_interface(I_DRIVER_ROOT, 
											RY_STANDALONE);
	if(p_dr) {
		_driver_request_t dr;

		iConsole *p_tty = (iConsole *)__g_p_repository__->get_object_by_interface(I_CONSOLE, RY_CLONE);
		dr.flags = DRF_INTERFACE;
		dr.DRIVER_INTERFACE = I_TEXT_DISPLAY;
		iTextDisplay *p_out = (iTextDisplay *)p_dr->driver_request(&dr);
		dr.DRIVER_INTERFACE = I_KEYBOARD;
		iKeyboard *p_in = (iKeyboard *)p_dr->driver_request(&dr);
		p_tty->init(p_in, p_out);
	}
}

void setup_ram_disk(void) {
	_u32 rds = int_parameter((_str_t)__g_p_startup_context__->p_kargs, RAMD_SIZE);
	if(rds) {
		iDriverRoot *p_droot = (iDriverRoot *)__g_p_repository__->get_object_by_interface(I_DRIVER_ROOT, 
												RY_STANDALONE);
		if(p_droot) {
			iBDCtrlHost *p_bdch = (iBDCtrlHost *)__g_p_repository__->get_object_by_interface(
											I_BDCTRL_HOST,
											RY_STANDALONE);
			if(p_bdch) {
				iRamDevice *p_rd = (iRamDevice *)__g_p_repository__->get_object_by_interface(
												I_RAM_DEVICE, 
												RY_CLONE);
				if(p_rd) {
					p_rd->_ramd_idx_ = 0;
					p_rd->_ramd_size_ = rds;
					p_droot->register_driver(p_rd, p_bdch);
				}
			}
		}
	}
}

void _rtc_evt_(void *p_data) {
	iEvent *p_evt = (iEvent *)p_data;
	p_evt->set(1);
}

bool wait_rtc_event(iRTC *p_rtc, iEvent *p_evt, _u32 mask) {
	bool r = false;
	p_rtc->enable_irq(true);
	if(p_evt->wait(mask) == mask)
		r = true;
	p_rtc->enable_irq(false);
	return r;
}

#define RTC_SAMPLES	32

void setup_cpu_timing(void) {
	iRTC *p_rtc = (iRTC *)__g_p_repository__->get_object_by_interface(I_RTC, RY_STANDALONE);
	if(p_rtc) {
		bool rtc_irq = p_rtc->enable_irq(false);
		iSystemLog *syslog = (iSystemLog *)__g_p_repository__->get_object_by_interface(I_SYSTEM_LOG, 
												RY_STANDALONE);
		iEvent *p_evt = (iEvent *)__g_p_repository__->get_object_by_interface(I_EVENT, RY_CLONE);

		
		syslog->write("Adjust CPU timers ");
		p_rtc->set_callback(_rtc_evt_, p_evt);
		iCPU *p_cpu = get_cpu();
		if(p_cpu && p_evt) {
			_u32 timer = 0;
			_u32 samples[RTC_SAMPLES];
			_u32 t = 0;
			p_cpu->set_timer(0xffffffff);
			while(t < RTC_SAMPLES) {
				wait_rtc_event(p_rtc, p_evt, 1);
				p_cpu->set_timer(0xffffffff);
				wait_rtc_event(p_rtc, p_evt, 1);
				samples[t] = 0xffffffff - p_cpu->get_timer();
				samples[t] = (samples[t] * 1024) / 1000;
				syslog->write(".");
				t++;
			}

			syslog->write("\r\n");

			for(_u32 i = 0; i < RTC_SAMPLES; i++) {
				timer += samples[i];
			}

			timer = timer / RTC_SAMPLES;

			iCPUHost *p_cpu_host = (iCPUHost *)__g_p_repository__->get_object_by_interface(I_CPU_HOST,
												 RY_STANDALONE);
			if(p_cpu_host) {
				_u32 ncpu = p_cpu_host->get_cpu_count();
				for(_u32 i = 0; i < ncpu; i++) {
					p_cpu = p_cpu_host->get_cpu_by_index(i);
					if(p_cpu) {
						iBlender *p_blender = p_cpu->get_blender();
						if(p_blender)
							p_blender->set_timer(timer);
					}
				}
			}
		}
		p_rtc->del_callback(_rtc_evt_);
		__g_p_repository__->release_object(p_evt);
		p_rtc->enable_irq(rtc_irq);
	}
}

extern _s32 main(_str_t args);

_s32 _main_(void *args) {
	setup_cpu_timing();
	setup_pci_controllers();
	setup_ram_disk();
	setup_fd_controller();
	main((_str_t)args);
	return 0;
}

void _kentry(_startup_context_t _UNUSED_ *p_context) {
	if(setup_system_heap()) {
		// copy arguments
		str_cpy(_g_kargs_, (_str_t)__g_p_startup_context__->p_kargs, sizeof(_g_kargs_));

		// we needed of system log with display output
		setup_system_log();
		// initialize the object repository
		// it will initiate call 'object_init(iRepositoryBase *p_rbase)' for all STANDALONE objects
		INIT_SO_STORAGE(__g_so_storage_index__);

		// setup system console
		setup_console();

		iCPUHost *p_cpu_host = (iCPUHost *)__g_p_repository__->get_object_by_interface(I_CPU_HOST, 
												RY_STANDALONE);
		if(p_cpu_host) {
			// Last call for sleeping drivers
			iDriverRoot *p_dr = (iDriverRoot *)__g_p_repository__->get_object_by_interface(
												I_DRIVER_ROOT, 
												RY_STANDALONE);
			if(p_dr) {
				p_dr->sys_update();
				__g_p_repository__->release_object(p_dr);
			}

			/////////////////////////////////////////
			// get the instance of current (boot) CPU 
			iCPU *p_cpu = p_cpu_host->get_cpu();
			if(p_cpu) {
				iBlender *p_sch = p_cpu->get_blender();
				if(p_sch)
					p_sch->create_systhread(_main_, 8192, (void *)_g_kargs_);

				// go to idle
				p_cpu->idle();
			}
		}
	}
}
