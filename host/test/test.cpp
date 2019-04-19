#include "iDataStorage.h"
#include "iMemory.h"
#include "repository.h"
#include "startup_context.h"
#include "karg_parser.h"
#include "str.h"
#include "iScheduler.h"
#include "iCPUHost.h"
#include "iBlockDevice.h"
#include "iDisplay.h"
#include "iSerialDevice.h"
#include "iSystemLog.h"
#include "iExecutable.h"
#include "iVFS.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define DEFAULT_PAGE_SIZE	4096
#define DEFAULT_LOG_SIZE	2

static void *_g_p_heap_=0;

iBlender *get_blender(void) {
	return 0;
}
iCPU *get_cpu(void) {
	return 0;
}

HMUTEX acquire_mutex(iMutex *p_mutex, HMUTEX hlock, _u32 timeout) {
	return p_mutex->lock(hlock);
}

bool enable_interrupts(bool i) {
	return !i;
}

void sleep_systhread(_u32 ms) {
	usleep(ms*100);
}

_u32 create_systhread(_thread_t *p_thread, _u32 stack_sz, void *p_udata, bool suspend, _u32 resolution) {
	_s32 r = 0;
	//???
	return r;
}

IMPLEMENT_SO_STORAGE(MAX_STATIC_OBJECTS);

_startup_context_t	_g_startup_context_;
_startup_context_t	*__g_p_startup_context__ = &_g_startup_context_;


#define _HEAP_SIZE (4 * (1024*1024))

void init_heap(void) {
	iHeap *p_heap = (iHeap *)__g_p_repository__->get_object_by_interface(I_HEAP, RY_STANDALONE);
	if(p_heap) {
		/*
		_g_p_heap_ = malloc(_HEAP_SIZE);
		if(_g_p_heap_) {
			p_heap->init(_g_p_heap_, _HEAP_SIZE);
		}
		*/
		p_heap->init(_HEAP_SIZE);
		__g_p_repository__->release_object(p_heap);
	}
}

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

void test_bitmap_pma(void) {
	iPMA *p_pma = (iPMA *)__g_p_repository__->get_object_by_interface(I_PMA, RY_STANDALONE);
	if(p_pma) {
		/*
		void *ram1 = malloc(8*(1024));
		void *ram2 = malloc(9*(1024*1024));
		void *ram3 = malloc(17*(1024*1024));

		_ram_part_t ram[]= {
			{(_u64)ram1,	8*(1024)},
			{(_u64)ram2,	9*(1024*1024)},
			{(_u64)ram3,  	17*(1024*1024)}
		};

		p_pma->init(ram, 3, 4096);
		*/

		void *p = p_pma->alloc(4);
		void *p1 = p_pma->alloc(16);
		p_pma->free(p1,8);
		_u32 _3m_pg = (2*(1024*1024)) / p_pma->get_page_size();
		void *p2 = p_pma->alloc(_3m_pg+4);
		void *p3 = p_pma->alloc(3);

		_u64 page_array[8192];
		_u32 npages = (1024*1024)*5 / p_pma->get_page_size();
		p_pma->alloc(npages, page_array);
	
		p_pma->free(2, &page_array[2]);
		p_pma->free(4, &page_array[510]);
		p_pma->free(2, &page_array[541]);

		_u64 pga[8];
		p_pma->alloc(6, pga);
	
		/*
		free(ram3);
		free(ram2);
		free(ram1);
		*/
	}
}

void test_heap(void) {
	iHeap *p_heap = (iHeap *)__g_p_repository__->get_object_by_interface(I_HEAP, RY_STANDALONE);
	void *ptr[300];
	
	for(int i = 0; i < 100; i++) {
		ptr[i] = p_heap->alloc(4097);
	}
	

	p_heap->free(ptr[98], 4097);
	p_heap->free(ptr[3], 4097);
		
	for(int i = 0; i < 300; i++) {
		ptr[i] = p_heap->alloc(130);
	}
	p_heap->free(ptr[3],130);

}

_cstr_t e1="1111111111";
_cstr_t e2="2222222222";
_cstr_t e3="3333333333";
_cstr_t e4="4444444444";

void test_vector(void) {
	iVector *v = (iVector *)__g_p_repository__->get_object_by_interface(I_VECTOR, RY_CLONE);
	iQueue *q = (iQueue *)__g_p_repository__->get_object_by_interface(I_QUEUE, RY_CLONE);
	if(v) {
		v->init(10, 2);
		v->add((void *)e1);
		v->add((void *)e2);
		v->add((void *)e3);
		v->add((void *)e4);
		
		_str_t e = (_str_t)v->current();
		e = (_str_t)v->first();
		v->mov(e, 1);
		e = (_str_t)v->first();
		v->mov(e, 1);
		e = (_str_t)v->first();
		if(e) {
			do  {
				printf("%s\n", e);
			} while(e = (_str_t)v->next());
		}
		v->col(1);
		e = (_str_t)v->first();
		if(e) {
			do  {
				printf("%s\n", e);
			} while(e = (_str_t)v->next());
		}
		
		v->get(0);
		v->del();

		e = (_str_t)v->first();
		if(e) {
			do  {
				printf("%s\n", e);
			} while(e = (_str_t)v->next());
		}
		
		e = (_str_t)v->first();
		v->col(0);
		v->mov(e, 0);
		e = (_str_t)v->first();
		if(e) {
			do  {
				printf("%s\n", e);
			} while(e = (_str_t)v->next());
		}
		v->col(1);
		e = (_str_t)v->first();
		if(e) {
			do  {
				printf("%s\n", e);
			} while(e = (_str_t)v->next());
		}
	}
	
	if(q) {
		q->init(10, 2);
		q->add((void *)e1);
		q->add((void *)e2);
		q->add((void *)e3);

		_str_t e = 0;
		q->roll();
		e = (_str_t)q->first();
		if(e) {
			do  {
				printf("%s\n", e);
			} while(e = (_str_t)q->next());
		}
	}
}

void test_kargs(void) {
	_str_t args=(_str_t)"rds=\"10m\" hs=\"8m\" ext=\"alabala.xml\" ls=\"4\"";


	_u32 rds = int_parameter(args, (_str_t)"rds");
	_u32 hs = int_parameter(args, (_str_t)"hs");
	_u32 ls = int_parameter(args, (_str_t)"ls");
	_s8 ext[128]="";
	str_parameter(args, (_str_t)"ext", ext, sizeof(ext));
	asm("nop");
	return;
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

void setup_ram_disk(_u8 mb) {
	_u32 rds = mb * (1024 *1024);
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

void setup_file_dev(void) {
	_driver_request_t dr;
	dr.flags = DRF_INTERFACE;
	dr.DRIVER_INTERFACE = I_BDCTRL_HOST;

	iDriverRoot *p_droot = (iDriverRoot *)__g_p_repository__->get_object_by_interface(I_DRIVER_ROOT, 
												RY_STANDALONE);
	if(p_droot) {
		iBDCtrlHost *p_bdch = (iBDCtrlHost *)__g_p_repository__->get_object_by_interface(I_BDCTRL_HOST, 
												RY_STANDALONE);
		if(p_bdch) {
			iDriverBase *p_fd = (iDriverBase *)__g_p_repository__->get_object_by_name("cFDevice", 
												RY_CLONE);
			if(p_fd) {
				p_droot->register_driver(p_fd, p_bdch);
			}
		}
	}
}

void setup_mgfs3_root(void) {
	/*
	iDriverRoot *p_dr = (iDriverRoot *)__g_p_repository__->get_object_by_interface(I_DRIVER_ROOT, 
											RY_STANDALONE);
	if(p_dr) {
		_driver_request_t dr;
		dr.flags = DRF_IDENT;
		dr.DRIVER_IDENT = "vrdd0";
		iVolume *p_vol = (iVolume *)p_dr->driver_request(&dr);
		if(p_vol) {
			dr.DRIVER_IDENT = "MGFS3-VFS";
			iFS *p_fs = (iFS *)p_dr->driver_request(&dr);
			if(p_fs) {
				iFSContext *p_cxt = p_fs->create_context();
				if(p_cxt) {
					if(p_cxt->add_storage(p_vol)) {
						_fs_context_info_t cxti;
						p_cxt->info(&cxti);
						p_cxt->format_storage(cxti.pbsize * cxti.tpbnum, 1, 0, (_str_t)"ROOT");
						//p_fs->destroy_context(p_cxt);
					}
				}
			}
		}
	}
	asm("nop");
	*/
	FSHANDLE hdev = 0;

	iDriverRoot *p_dr = (iDriverRoot *)__g_p_repository__->get_object_by_interface(I_DRIVER_ROOT, 
											RY_STANDALONE);
	iFSHandle *p_fsh = (iFSHandle *)__g_p_repository__->get_object_by_interface(I_FS_HANDLE, RY_STANDALONE);
	
	if(p_dr && p_fsh) {
		_driver_request_t dr;
		dr.flags = DRF_IDENT;
		dr.DRIVER_IDENT = "vrdd0";
		iVolume *p_vol = (iVolume *)p_dr->driver_request(&dr);
		if(p_vol) {
			dr.DRIVER_IDENT = "MGFS3-VFS";
			iFS *p_fs = (iFS *)p_dr->driver_request(&dr);
			if(p_fs) {
				if((hdev = p_fsh->create(p_vol, 0, HTYPE_VOLUME))) {
					p_fsh->update_state(hdev, HSTATE_OPEN);
					_u64 dev_size = p_vol->_vol_block_size_ * p_vol->_vol_size_;
					if(p_fs->format(hdev, dev_size, 1, 0, (_str_t)"ROOT")) {
						// mount root
						iFSRoot *p_fs_root = 
							(iFSRoot *)__g_p_repository__->get_object_by_interface(
									I_FS_ROOT, RY_STANDALONE);
						if(p_fs_root) {
							p_fs_root->mount(p_vol, (_str_t)"/");
							__g_p_repository__->release_object(p_fs_root);
						}
					}
				}
			}
		}

		__g_p_repository__->release_object(p_dr);
	}
}

void fs_list(_fs_file_info_t *p_fi, void *p_udata) {
	iSystemLog *syslog = (iSystemLog *)__g_p_repository__->get_object_by_interface(I_SYSTEM_LOG, 
											RY_STANDALONE);
	syslog->fwrite("%u\t\t\t%s\r\n", p_fi->fsize, p_fi->fname);
}
void enum_volume(void) {
	iFSHost *p_fs_host = (iFSHost *)__g_p_repository__->get_object_by_interface(I_FS_HOST, RY_STANDALONE);
	if(p_fs_host) {
		iSystemLog *syslog = (iSystemLog *)__g_p_repository__->get_object_by_interface(I_SYSTEM_LOG, 
											RY_STANDALONE);
		iVolume *p_vol = 0;
		iFS *p_fs=0;
		_u32 idx=0;
		while((p_vol = p_fs_host->volume(idx))) {
			p_fs = p_fs_host->ident(p_vol);
			syslog->fwrite("volume: '%s' filesystem: '%s' serial: '%s'\r\n", p_vol->DRIVER_IDENT, 
						(p_fs)?p_fs->DRIVER_IDENT:"unknown", p_vol->_vol_ident_);
			if(p_fs) {
				iFSContext *p_cxt = p_fs->get_context(p_vol);
				if(!p_cxt) {
					p_cxt = p_fs->create_context();
					p_cxt->add_storage(p_vol);
				}
				FSHANDLE hroot = p_cxt->root();
				if(hroot) {
					syslog->fwrite("'%s' root is open\r\n", p_vol->DRIVER_IDENT);
					FSHANDLE hboot;
					if(p_cxt->fopen((_str_t)"boot", hroot, &hboot) == 0) {
						p_cxt->list(hboot, fs_list);
						p_cxt->fclose(hboot);
					}
					p_cxt->list(hroot, fs_list);
					//iDriverRoot *p_dr = (iDriverRoot *)__g_p_repository__->get_object_by_interface(
					//				I_DRIVER_ROOT, RY_STANDALONE);
					//p_dr->remove_driver(p_vol);
					//continue;
				} 
			}
			idx++;
		}
	}
}

_str_t ext_env="dummy extension environment";

void test_ext(void) {
	iExtLoader *p_eld = (iExtLoader *)__g_p_repository__->get_object_by_interface(I_EXT_LOADER, RY_STANDALONE);
	iFSRoot *p_fs_root = (iFSRoot *)__g_p_repository__->get_object_by_interface(I_FS_ROOT, RY_STANDALONE);

	if(p_eld && p_fs_root) {
		FSHANDLE hfinit;
		_fs_res_t r = p_fs_root->fopen(0, (_str_t)"/init/", 0, 0, &hfinit);
		//p_fs_root->fclose(&hfinit);

		FSHANDLE hf;
		r = p_fs_root->fopen(0, (_str_t)"boot/", (_str_t)"boot.config", 0, &hf, hfinit);
		p_fs_root->fclose(&hf);
		r = p_fs_root->fopen(0, (_str_t)"boot/boot.config", 0, 0, &hf, hfinit);
		p_fs_root->fclose(&hf);
		//if(p_fs_root->fopen((_str_t)"init/boot/test.elf", 0, 0, &htest) == FS_RESULT_OK) {
		HEXTENSION htest = p_eld->load((_str_t)"/init/boot/test.elf", (_str_t)"test", 4);
			//p_fs_root->fclose(&htest);
		//}
		__g_p_repository__->release_object(p_fs_root);
		__g_p_repository__->release_object(p_eld);
	}
}

void test_mem_cpy(void) {
	_u8 buffer[]="01234567890123456789";

	mem_cpy(buffer, buffer+1, 5);
	mem_cpy(buffer+1, buffer, 10);
	asm("nop");
}

void mount_volume(_str_t vname, _str_t mount_point) {
	iFSRoot *p_fs_root = (iFSRoot *)__g_p_repository__->get_object_by_interface(I_FS_ROOT, RY_STANDALONE);
	if(p_fs_root) {
		p_fs_root->mount(vname, mount_point);
	}
}

void test_mount(void) {
	mount_volume("vrdd0", "/");
	iFSHandle *p_fsh = (iFSHandle *)__g_p_repository__->get_object_by_interface(I_FS_HANDLE, RY_STANDALONE);
	iFSRoot *p_fs_root = (iFSRoot *)__g_p_repository__->get_object_by_interface(I_FS_ROOT, RY_STANDALONE);
	if(p_fs_root) {
		FSHANDLE hfile;
		
		if(p_fs_root->fopen(9, "/", 0, 0, &hfile) == FS_RESULT_OK) {
			iFSContext *p_cxt = (iFSContext *)p_fsh->driver(hfile);
			if(p_cxt)
				p_cxt->list(hfile, fs_list);
		}
		p_fs_root->fclose(hfile);
		
		p_fs_root->fopen(0, "/", "init", FSP_DIR|FSP_CREATE_NEW, &hfile);
		p_fs_root->fclose(hfile);
		/*
		p_fs_root->fopen("/", "dir2", FSP_DIR|FSP_CREATE_NEW, &hfile,0);
		p_fs_root->fclose(&hfile);
		
		if(p_fs_root->fopen("/", 0, 0, &hfile,0) == FS_RESULT_OK)
			hfile.p_fscxt->list(&hfile, fs_list);
		p_fs_root->fclose(&hfile);
		*/
		mount_volume("vfdd0b", "/init");
		/*
		if(p_fs_root->fopen("/init", 0, 0, &hfile,0) == FS_RESULT_OK)
			hfile.p_fscxt->list(&hfile, fs_list);
		p_fs_root->fclose(&hfile);
		
		if(p_fs_root->fopen("/init/boot", 0, 0, &hfile,0) == FS_RESULT_OK)
			hfile.p_fscxt->list(&hfile, fs_list);
		p_fs_root->fclose(&hfile);

		if(p_fs_root->fopen("/init/boot/core2/", 0, 0, &hfile,0) == FS_RESULT_OK)
			hfile.p_fscxt->list(&hfile, fs_list);
		p_fs_root->fclose(&hfile);
		
		p_fs_root->mount(&hfile,"/init/boot/core2", "/dir2");
		if(p_fs_root->fopen("/dir2", 0, 0, &hfile,0) == FS_RESULT_OK)
			hfile.p_fscxt->list(&hfile, fs_list);
		p_fs_root->fclose(&hfile);
		
		p_fs_root->unmount("/dir2");
		if(p_fs_root->fopen("/dir2", 0, 0, &hfile,0) == FS_RESULT_OK)
			hfile.p_fscxt->list(&hfile, fs_list);
		p_fs_root->fclose(&hfile);
		asm("nop");
		*/
	}
}

#include "sha1.h"

void test_sha1(void) {
	char data[]="1234567890";
	char data2[]="1234567891";
	SHA1Context cxt;
	uint8_t result[8]="";

	SHA1Reset(&cxt);
	SHA1Input(&cxt, (const uint8_t *)data, str_len(data));
	SHA1Result(&cxt, result, 8);
	
	SHA1Reset(&cxt);
	SHA1Input(&cxt, (const uint8_t *)data2, str_len(data2));
	SHA1Result(&cxt, result, 8);
	asm("nop");
}

void test_btree(void) {
	iBTree *p_bt = (iBTree *)__g_p_repository__->get_object_by_interface(I_BTREE, RY_CLONE);
	if(p_bt) {
		_u8 d1[]="1111111111111111111111";
		_u8 da[]="0111111111111111111111";
		_u8 d2[]="22222222222222222222222";
		_u8 d3[]="33333333333333333333333333";
		_u8 d4[]="4444444444444444444444";
		_u8 d5[]="555555555555555555555555555555";
		_u8 d6[]="66666666666666666666666";
		_u8 d7[]="777777777777777777777777777777";


		_bt_key_t k1 = p_bt->add(d1, str_len((_str_t)d1));
		_bt_key_t ka = p_bt->add(da, str_len((_str_t)da));
		_bt_key_t k2 = p_bt->add(d2, str_len((_str_t)d2));
		_bt_key_t k3 = p_bt->add(d3, str_len((_str_t)d3));
		_bt_key_t k4 = p_bt->add(d4, str_len((_str_t)d4));
		_bt_key_t k5 = p_bt->add(d5, str_len((_str_t)d5));
		_bt_key_t k6 = p_bt->add(d6, str_len((_str_t)d6));
		_bt_key_t k7 = p_bt->add(d7, str_len((_str_t)d7));

		p_bt->del(k1);
		p_bt->del(k4);
		_str_t _d1 = (_str_t)p_bt->get(k4);
		_str_t _d7 = (_str_t)p_bt->get(k7);
		_str_t _d5 = (_str_t)p_bt->get(k5);
		asm("nop");
	}
}	

#include "slab.h"
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
			syslog->fwrite("%s   ptr[%d]=0x%h\r\n", tabs, i, p_slab->ptr[i]);
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

void test_event(void) {
	iEvent *p_evt = (iEvent *)__g_p_repository__->get_object_by_interface(I_EVENT, RY_CLONE);
	_u32 x = 0;
	if(p_evt) {
		p_evt->init();
		x = p_evt->check(0x01);
		p_evt->set(0x02);
		x = p_evt->check();
		x = p_evt->check();
		x = p_evt->check(0x01);
		p_evt->set(0x01);
		x = p_evt->check(0x02);
		x = p_evt->check(0x01);
		x = p_evt->check(0x04);
		p_evt->set(0x03);
		x = p_evt->check(1);
		x = p_evt->check(2);
		x = p_evt->check(3);
		p_evt->set(0x03);
		x = p_evt->check(3);
		x = p_evt->check(1);
		x = p_evt->check(2);
		p_evt->set(0x03);
		x = p_evt->check(2);
		x = p_evt->check(3);
		x = p_evt->check(1);
		x = p_evt->check(0x08);
	}
}

void test_env(void) {
	_str_t v = 0;
	_u32 sz = 0;

	iEnv *p_env = (iEnv *)__g_p_repository__->get_object_by_interface(I_ENV, RY_ONCE);
	if(p_env) {
		p_env->set((_str_t)"PATH", (_str_t)"/init/boot;", 11);
		if(p_env->get((_str_t)"PATH", &v, &sz));
			asm("nop");
		p_env->set((_str_t)"PATH", (_str_t)"/init/boot;/init/boot/core2;",28);
		if(p_env->get((_str_t)"PATH", &v, &sz));
			asm("nop");
		p_env->set((_str_t)"PATH", (_str_t)"/init/boot;", 11);
		if(p_env->get((_str_t)"PATH", &v, &sz));
			asm("nop");
	}
}

int main(void) {
	test_mem_cpy();
	_ulong so_idx = ADD_SO_STORAGE();

	init_pma();
	
	test_bitmap_pma();
	
	init_heap();
	
	//test_sha1();
	//test_btree();
	init_syslog();
	INIT_SO_STORAGE(so_idx);
	test_env();
	test_event();
	init_console();
	
	iDriverRoot *p_dr = (iDriverRoot *)__g_p_repository__->get_object_by_interface(I_DRIVER_ROOT, RY_STANDALONE);
	if(p_dr) {
		p_dr->sys_update();
		//__g_p_repository__->release_object(p_dr);
	}
	
	setup_ram_disk(16);
	setup_file_dev();
	setup_mgfs3_root();
	test_mount();
	enum_volume();
	test_ext();

		iFSHost *p_fsh = (iFSHost *)__g_p_repository__->get_object_by_interface(I_FS_HOST, RY_STANDALONE);
		iFS *p_fs = p_fsh->filesystem(0);
		if(p_fs)
			p_dr->remove_driver(p_fs, true);
	
	//test_vector();
	//test_kargs();
	
	/*
	test_heap();
	_heap_info_t hi;
	
	iHeap *p_heap = (iHeap *)__g_p_repository__->get_object_by_interface(I_HEAP, RY_STANDALONE);
	if(p_heap) {
		p_heap->info(&hi);
		_slab_t *ps = (_slab_t *)hi.base;
		dump(ps);
	}
	*/
	
}

