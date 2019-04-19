#include "repository.h"
#include "startup_context.h"
#include "iScheduler.h"
#include "iMemory.h"
#include "iDataStorage.h"
#include "iDisplay.h"
#include "iSerialDevice.h"
#include "iSystemLog.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define DEFAULT_PAGE_SIZE	4096
#define DEFAULT_LOG_SIZE	2

_startup_context_t	_g_startup_context_;
_startup_context_t	*__g_p_startup_context__ = &_g_startup_context_;

HMUTEX acquire_mutex(iMutex *p_mutex, HMUTEX hlock, _u32 timeout) {
	return p_mutex->lock(hlock);
}

IMPLEMENT_SO_STORAGE(MAX_STATIC_OBJECTS);

void init_pma(void) {
	iPMA *p_pma = (iPMA *)__g_p_repository__->get_object_by_interface(I_PMA, RY_STANDALONE);
	if(p_pma) {
		void *ram1 = malloc(30*(1024*1024));
		void *ram2 = malloc(80*(1024*1024));
		void *ram3 = malloc(160 *(1024*1024));

		_ram_part_t ram[]= {
			{(_u64)ram1,	30*(1024*1024)},
			{(_u64)ram2,	80*(1024*1024)},
			{(_u64)ram3,  	160*(1024*1024)}
		};

		p_pma->init(ram, 3, 4096);
	}
}
void init_heap(void) {
	iHeap *p_heap = (iHeap *)__g_p_repository__->get_object_by_interface(I_HEAP, RY_STANDALONE);
	if(p_heap) {
		p_heap->init(0);
		__g_p_repository__->release_object(p_heap);
	}
}
void init_syslog(void) {
	iTextDisplay *p_out = (iTextDisplay *)__g_p_repository__->get_object_by_interface(I_TEXT_DISPLAY, 
										RY_STANDALONE);
	if(p_out) {
		p_out->init(24,80);
		p_out->position(24,0);
		p_out->color(0x07);
		__g_p_repository__->release_object(p_out);
	}

	_u32 page_size = DEFAULT_PAGE_SIZE;
	iPMA *p_pma = (iPMA *)__g_p_repository__->get_object_by_interface(I_PMA, RY_STANDALONE);
	if(p_pma) {
		_ram_info_t ri;
		p_pma->info(&ri);
		page_size = ri.ps;
		__g_p_repository__->release_object(p_pma);
	}
	iSystemLog *syslog = (iSystemLog *)__g_p_repository__->get_object_by_interface(I_SYSTEM_LOG, 
										RY_STANDALONE);
	if(syslog) {
		_u32 ls = 4;
		_u32 log_pages = ls/page_size;
		log_pages += (ls % page_size)?1:0;
		syslog->init((log_pages)?log_pages:DEFAULT_LOG_SIZE);
		__g_p_repository__->release_object(syslog);
	}
}
void init_console(void) {
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
int main(void) {
	_ulong so_idx = ADD_SO_STORAGE();
	init_pma();
	init_heap();
	init_syslog();
	INIT_SO_STORAGE(so_idx);
	init_console();

	//???
}

