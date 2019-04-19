#include "ext.h"
#include "iSystemLog.h"
#include "repository.h"
#include "iCPUHost.h"
#include "iScheduler.h"
#include "iSyncObject.h"
#include "iDisplay.h"
#include "iSerialDevice.h"

_u8 ialive =3;
_s8 alive[]="-\\|/";
_s8 space[]="\t\t\t\t\t\t\t\t\t\t";
iConsole *p_tty=0;
iMutex *p_mutex=0;

iCPUHost *_g_p_cpu_host = 0;
iEvent *_g_event_=0;
_s32 _OPTIMIZE_SIZE_ test_thread(void *p_data) {
	if(p_tty) {
		_u32 x = 200;
		_u8 phase = 0;
		_u32 sleep2 = 1000 / ((_u32)(_u64)p_data + 1);
		_u32 sleep4 = sleep2  / ((_u32)(_u64)p_data + 1);
		_u32 sleep = sleep4;
		
		iCPU *p_cpu = get_cpu();
		if(p_cpu) {
			_u8 c = p_cpu->cpu_id();
			_u8 d = (_u8)(_u64)p_data;

			_s8 tabs[80]="";
			mem_set((_u8 *)tabs, '\t', sizeof(tabs)-1);
			//iTextDisplay *p_disp = (iTextDisplay *)p_tty->get_output();

			while(1) {
				
				// MUTEX SYNC implementarion //
				//bool irqs = enable_interrupts(false);
				//HMUTEX hsl = p_mutex->lock();
				HMUTEX hsl = acquire_mutex(p_mutex, 0, 200);
				if(hsl) {
					tabs[c*10+d] = alive[phase];	
					//p_disp->color(c+d+1+phase);
					p_tty->fwrite("%s\r", tabs);
					p_mutex->unlock(hsl);
				}
				//enable_interrupts(irqs);
				
				/*
				// EVENT SYNC implementation 
				//bool irqs = enable_interrupts(false);
				//32 evt = wait_event(_g_event_); 
				_u32 evt = _g_event_->wait();
				if(evt == 1) {
					tabs[c*10+d] = alive[phase];
					//p_disp->color(c+d+1);
					p_tty->fwrite("%s\r", tabs);
					_g_event_->set(1);
				}
				//enable_interrupts(irqs);
				*/
				phase++;
				if(phase > ialive)
					phase = 0;
				
				if(!(x--)) {
					if(sleep == sleep4) {
						sleep = sleep2;
						x = 10;
					} else {
						sleep = sleep4;
						x = 100;
					}
				}
				
				
				//switch_context();
				sleep_systhread(sleep);
				//sleep_systhread(1);
			}
		}
	}

	return 0;
}

iSystemLog *_g_syslog_;

void thread_test(void) {
	iSystemLog *syslog = (iSystemLog *)__g_p_repository__->get_object_by_interface(I_SYSTEM_LOG, RY_STANDALONE);
	_g_syslog_ = syslog;
	_g_event_ = (iEvent *)__g_p_repository__->get_object_by_interface(I_EVENT, RY_CLONE);
	
	_g_syslog_->write("---- SMP multithread test ----\r\n");
	
	p_mutex = (iMutex *)__g_p_repository__->get_object_by_interface(I_MUTEX, RY_CLONE);
	iDriverRoot *p_dr = (iDriverRoot *)__g_p_repository__->get_object_by_interface(I_DRIVER_ROOT, RY_STANDALONE);
	if(p_dr) {
		_driver_request_t dr;

		mem_set((_u8 *)&dr, 0, sizeof(dr));
		dr.flags = DRF_IDENT;
		dr.DRIVER_IDENT = "tty0";
		p_tty = (iConsole *)p_dr->driver_request(&dr);
	}

	_g_event_->init();

	// get the instance of current (boot) CPU 
	//iCPU *p_cpu = get_cpu();
	// dump VMM
	//p_cpu->get_vmm()->dump(0, 0xffffffffffffffff);
	HMUTEX hl = acquire_mutex(p_mutex, 0, 200);

	_u16 c = cpu_count();
	bool irqs = enable_interrupts(false);
	HMUTEX hsl = syslog->lock();
	syslog->fwrite(hsl, "CPU count = %d\r\n", c);
	_u16 i=0, n = 0;
	for(i = 0; i < c  ;i++) {
		iCPU *p_cpu = get_cpu_by_index(i);
		if(p_cpu) {
			syslog->fwrite(hsl, " CPU(%d)   ", p_cpu->cpu_id());
			create_systhread(i, test_thread, 0, (void *)0);
			create_systhread(i, test_thread, 0, (void *)1);
			create_systhread(i, test_thread, 0, (void *)2);
			create_systhread(i, test_thread, 0, (void *)3);
			create_systhread(i, test_thread, 0, (void *)4);
			create_systhread(i, test_thread, 0, (void *)5);
			create_systhread(i, test_thread, 0, (void *)6);
			create_systhread(i, test_thread, 0, (void *)7);
			n++;
			if(n >= 8)
				break;
		}
	}

	syslog->write("\r\n",hsl);
	syslog->unlock(hsl);
	enable_interrupts(irqs);
	_g_event_->set(1);
	p_mutex->unlock(hl);
	__g_p_repository__->release_object(syslog);
}

#include "iMemory.h"
#include "addr.h"

void mem_dump(void) {
	_ram_info_t pma_info;

	iSystemLog *syslog = (iSystemLog *)__g_p_repository__->get_object_by_interface(I_SYSTEM_LOG, RY_STANDALONE);
	iPMA *p_pma = (iPMA *)__g_p_repository__->get_object_by_interface(I_PMA, RY_STANDALONE);
	if(p_pma) {
		p_pma->info(&pma_info);
		syslog->write("PMA info :\r\n");
		syslog->fwrite("all pages: %u\r\n", pma_info.rp);
		syslog->fwrite("used     : %u\r\n", pma_info.pu);
		syslog->fwrite("free     : %u\r\n", pma_info.rp - pma_info.pu - pma_info.sp);
		syslog->fwrite("system   : %u\r\n", pma_info.sp);
		//p_pma->dump();

		/*void *p = p_pma->alloc(1, RT_SYSTEM, p_to_k(0xffffffff));
		syslog->fwrite("4G pma  limit: 0x%h\r\n", (_ulong)p);
		mem_set((_u8 *)p, 0, 4096);*/
	}
	iHeap *p_heap = (iHeap *)__g_p_repository__->get_object_by_interface(I_HEAP, RY_STANDALONE);
	if(p_heap) {
		_heap_info_t heap_info;
		p_heap->info(&heap_info);
		syslog->write("heap info:\r\n");
		syslog->fwrite("base     : 0x%h\r\n", heap_info.base);
		syslog->fwrite("total KB : %u\r\n", heap_info.size / 1024);
		//syslog->fwrite("free  KB : %u.%u\r\n", heap_info.free / 1024,heap_info.free % 1024) ;
		//syslog->fwrite("unused KB: %u.%u\r\n", heap_info.unused / 1024, heap_info.unused % 1024);
		syslog->fwrite("objects  : %u\r\n", heap_info.objects);

		/*void *p = p_heap->alloc(4096, p_to_k(0xffffffff));
		syslog->fwrite("4G heap limit: 0x%h\r\n", (_ulong)p);
		mem_set((_u8 *)p, 0, 4096);*/
	}
}

#include "iRTC.h"

#include "../../core/mem/slab.h"
#define MAX_TABS	4
#define PAGE_SIZE	4096
#define SLAB_PATTERN	(_u64)0xcacacacacacacaca
#define SLAB_PATTERN2	(_u64)0xdadadadadadadada

void slab_print(_slab_t *p_slab) {
	char tabs[MAX_TABS+1]="";
	iSystemLog *syslog = (iSystemLog *)__g_p_repository__->get_object_by_interface(I_SYSTEM_LOG, RY_STANDALONE);

	for(_u8 i = 0; i < MAX_TABS-p_slab->level; i++)
		tabs[i] = '\t';

	syslog->fwrite("%sosz  =%u\r\n", tabs, p_slab->osz);
	syslog->fwrite("%slevel=%d\r\n", tabs, p_slab->level);
	syslog->fwrite("%scount=%u\r\n", tabs, p_slab->count);

	for(_u8 i = 0; i < SLAB_NODES; i++) {
		if(p_slab->ptr[i]) {
			syslog->fwrite("%s   ptr[%d]=0x%h\r\n", tabs, i, p_slab->ptr[i]);
			if(p_slab->level) {
				_u32 count = PAGE_SIZE / sizeof(_slab_t);
				_slab_t *ps = (_slab_t *)p_slab->ptr[i];
				for(_u32 j = 0; j < count; j++) {
					if(ps->osz)
						slab_print(ps);
					ps++;
				}
			} else {
				_u32 npages = 1;
				if(p_slab->osz > PAGE_SIZE)
					npages = p_slab->osz / PAGE_SIZE;
				_u32 mem_sz = npages * PAGE_SIZE;
				_u32 nobjs = mem_sz / p_slab->osz;
				_u8 *ptr = (_u8 *)p_slab->ptr[i];
				syslog->fwrite("%s\t",tabs);
				for(_u32 j = 0; j < nobjs; j++) {
					_u64 *p64 = (_u64 *)(ptr + (j * p_slab->osz));
					switch(*p64) {
						case SLAB_PATTERN:
							syslog->write("-");
							break;
						case SLAB_PATTERN2:
							syslog->write(".");
							break;
						default:
							syslog->write("+");
							break;
					}
				}
				syslog->fwrite("\r\n");
			}
		} else
			break;
	}
}

void dump(_slab_t *p_slab) {
	iSystemLog *syslog = (iSystemLog *)__g_p_repository__->get_object_by_interface(I_SYSTEM_LOG, RY_STANDALONE);
	_u32 count = PAGE_SIZE / sizeof(_slab_t);
	_u64 e =  SLAB_MIN_ALLOC;
	for(_u32 i = 0; i < count; i++) {
		_slab_t *ps = p_slab + i;
		if(ps->osz) {
			syslog->fwrite("%u\r\n", e);
			slab_print(ps);
			sleep_systhread(1000);
		}
		e <<= 1;
	}
}
#include "iDriver.h"
void driver_test(void) {
	iSystemLog *syslog = (iSystemLog *)__g_p_repository__->get_object_by_interface(I_SYSTEM_LOG, RY_STANDALONE);
	iDriverRoot *p_dr = (iDriverRoot *)__g_p_repository__->get_object_by_interface(I_DRIVER_ROOT, RY_STANDALONE);
	if(p_dr) {
		_driver_request_t dr;
		dr.flags = DRF_IDENT;
		dr.DRIVER_IDENT = "TDriver";
		iDriverBase *p_drv = p_dr->driver_request(&dr);
		if(p_drv) {
			_u8 buf[64];
			_u32 nb=0;
			p_drv->read(buf, 64, &nb);
		} else {
			syslog->write("No driver 'TDriver'\r\n");
			//iDriverBase *p_drv - (iDriverBase *)__g_p_repository__
		}
	} else
		syslog->write("No Driver Root\r\n");
}

void _main(_str_t _UNUSED_ args) {
	iTextDisplay *p_disp = (iTextDisplay *)__g_p_repository__->get_object_by_interface(I_TEXT_DISPLAY, RY_STANDALONE);
	if(p_disp)
		p_disp->color(0x0f);
	
	//iSystemLog *syslog = (iSystemLog *)__g_p_repository__->get_object_by_interface(I_SYSTEM_LOG, RY_STANDALONE);

	//sleep_systhread(100);
	
	/*iRTC *p_rtc = (iRTC *)__g_p_repository__->get_object_by_interface(I_RTC, RY_STANDALONE);
	if(p_rtc) {
		_u64 *prtc = (_u64 *)p_rtc;
		syslog->fwrite("prtc: 0x%h\r\n", *prtc);
		syslog->fwrite("20%x-%x-%x %x:%x:%x\r\n", p_rtc->year(), 
				p_rtc->month(), p_rtc->day_of_month(), p_rtc->hours(), p_rtc->minutes(), p_rtc->seconds());
	}*/
	
	// dump SLAB
	/*
	_heap_info_t hi;
	iHeap *p_heap = (iHeap *)__g_p_repository__->get_object_by_interface(I_HEAP, RY_STANDALONE);
	if(p_heap) {
		p_heap->info(&hi);
		_slab_t *ps = (_slab_t *)hi.base;
		dump(ps);
	}
	*/
	///////////
	
	

	//driver_test();
	mem_dump();
	thread_test();
}

