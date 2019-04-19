#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "time.h"
#include "str.h"
#include <pthread.h>
#include "iMemory.h"
#include "xmake.h"
#include "clargs.h"

static void *_g_p_heap_=0;

IMPLEMENT_SO_STORAGE(MAX_STATIC_OBJECTS);

bool enable_interrupts(bool irqs) {
	return !irqs;
}

static _cl_arg_t _g_options_[]={
	{ARG_TFLAG,	"--verbose",	"-V",	"be verbose"},
	{ARG_TFLAG,	"--version",	"-v",	"display version"},	
	{ARG_TFLAG,	"--help",	"-h",	"show this message and exit"},
	{ARG_TINPUT,	"--target",	"-t",	"target build '--target=<name>'"},
	{ARG_TINPUT,	"--output",	"-o",	"output file '--output=<file name>'"},
	{ARG_TINPUT,	"--branch",	"-b",	"target branch '--branch=<branch name>"},
	{ARG_TINPUT,	"--project",	"-p",	"project directory '--project=<directory>'"},
	{0,0,0,0}
};

bool init_repository(void) {
	bool r = false;
	
	_ulong so_idx = ADD_SO_STORAGE();
	iHeap *p_heap = (iHeap *)__g_p_repository__->get_object_by_interface(I_HEAP, RY_STANDALONE);
	if(p_heap) {
		_g_p_heap_ = malloc(HEAP_SIZE);
		if(_g_p_heap_) {
			p_heap->init(_g_p_heap_, HEAP_SIZE);
			r = INIT_SO_STORAGE(so_idx);
		}
		
		__g_p_repository__->release_object(p_heap);
	}
	
	return r;
}

void print_usage(void) {
	TRACE("xmake version: %s\n", XMAKE_VERSION);
	TRACE("usage: xmake [options] --project=<dir> --output=<file.mk> --target=<build name> [--branch=<branch name>] file.xml\n","");
	TRACE("options:\n","");
	clargs_print(_g_options_);
}

int main(int argc, char *argv[]) {
	if(init_repository()) {
		// parse command line 
		if(clargs_parse(argc, argv, _g_options_)) {
			if(clargs_option("-h")) 
				print_usage();
			else {
				if(clargs_option("-v")) 
					TRACE("xmake version: %s\n", XMAKE_VERSION);
				
				xmake_process();
			}
			
		} else {
			clargs_print(_g_options_);
			TRACE("ERROR: %s", clargs_err_text());
		}
	} else {
		TRACE("Failed to init repository\n","");
	}
	
	if(_g_p_heap_)
		free(_g_p_heap_);

	return 0;
}

