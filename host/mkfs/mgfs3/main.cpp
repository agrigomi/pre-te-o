#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "time.h"
#include "repository.h"
#include "str.h"
#include <pthread.h>
#include "mkfs.h"
#include "buffer.h"
#include "iMemory.h"

#define MKMGFS_VERSION	"3.0"

IMPLEMENT_SO_STORAGE(MAX_STATIC_OBJECTS);


static _u8 *_g_dev_ = 0;
static _u64 _g_dev_size_=0;
static _u64 last_map_size = 0;
static void *_g_p_heap_=0;
static _mgfs_context_t _g_mgfs_context_;
static iMutex *_g_fs_mutex_=0;

#define SZ_SECTOR	512
#define HEAP_SIZE	((1024 *1024) * 4)

bool enable_interrupts(bool irqs) {
	return !irqs;
}


_mgfs_context_t *get_context(void) {
	return &_g_mgfs_context_;
}

_fs_context_t *get_fs_context(void) {
	return &(_g_mgfs_context_.fs_context);
}

_u8 *get_device(void) {
	return _g_dev_;
}

_u64 get_device_size(void) {
	return _g_dev_size_;
}

_u8 *map_file(_str_t name,_u64 *sz) {
	_u8 *r = NULL;
	int fd = open(name,O_RDWR);
	unsigned long len;
	
	if(fd != -1) {
		len = (_u64)lseek(fd,0,SEEK_END);
		lseek(fd,0,SEEK_SET);
		r = (_u8 *)mmap(NULL,len,PROT_WRITE,MAP_SHARED,fd,0);
		*sz = last_map_size = len;
	}
	
	return r;
}

_u32 sector_read(void *p_io_cxt, _u32 sector, _u32 count, void *buffer) {
	_u32 r = 0;

	if(_g_dev_) {
		_u64 offset = sector * SZ_SECTOR;
		_u32 len = count * SZ_SECTOR;

		mem_cpy((_u8 *)buffer, _g_dev_ + offset, len);
		r = count;
	}

	return r;
}

_u32 sector_write(void *p_io_cxt, _u32 sector, _u32 count, void *buffer) {
	_u32 r = 0;

	if(_g_dev_) {
		_u64 offset = sector * SZ_SECTOR;
		_u32 len = count * SZ_SECTOR;

		mem_cpy(_g_dev_ + offset, (_u8 *)buffer, len);
		r = count;
	}

	return r;
}

void *mem_alloc(void *p_io_cxt, _u32 size) {
	void *r = 0;

	iHeap *p_heap = (iHeap *)__g_p_repository__->get_object_by_interface(I_HEAP, RY_STANDALONE);
	if(p_heap) {
		r = p_heap->alloc(size);
		__g_p_repository__->release_object(p_heap);
	}

	return r;
}

void mem_free(void *p_io_cxt, void *ptr, _u32 size) {
	iHeap *p_heap = (iHeap *)__g_p_repository__->get_object_by_interface(I_HEAP, RY_STANDALONE);
	if(p_heap) {
		p_heap->free(ptr, size);
		__g_p_repository__->release_object(p_heap);
	}
}

HMUTEX fs_lock(void *p_io_cxt, HMUTEX hlock) {
	HMUTEX r = 0;

	if(!_g_fs_mutex_)
		_g_fs_mutex_ = (iMutex *)__g_p_repository__->get_object_by_interface(I_MUTEX, RY_CLONE);

	if(_g_fs_mutex_) 
		r = _g_fs_mutex_->lock(hlock);

	return r;
}

void fs_unlock(void *p_io_cxt, HMUTEX hlock) {
	if(!_g_fs_mutex_)
		_g_fs_mutex_ = (iMutex *)__g_p_repository__->get_object_by_interface(I_MUTEX, RY_CLONE);

	if(_g_fs_mutex_) 
		_g_fs_mutex_->unlock(hlock);
}

void fs_info(void *p_io_context, _fs_info_t *p_fsi) {
	p_fsi->sector_size = _g_mgfs_context_.fs.sz_sector;
	p_fsi->unit_size = _g_mgfs_context_.fs.sz_unit;
	p_fsi->nunits = _g_mgfs_context_.fs.sz_fs;
	p_fsi->fs_name = (_str_t)"mgfs3";
	p_fsi->fs_version = _g_mgfs_context_.fs.version;
}

_u32 timestamp(void *p_io_cxt) {
	return time(NULL);
}

pthread_t t;
pthread_attr_t attr;

static volatile bool _cp_running = false;
static volatile bool _cp_stop = true;

void *cache_process(void *p_data) {
	_cp_running = true;
	_cp_stop = false;
	
	while(_cp_running) {
		usleep(100000);
		mgfs_buffer_flush(&_g_mgfs_context_, INVALID_BUFFER_HANDLE);
	}
	
	mgfs_buffer_flush(&_g_mgfs_context_, INVALID_BUFFER_HANDLE);
	_cp_stop = true;

	return 0;
}

void create_context(void) {
	memset(&_g_mgfs_context_, 0, sizeof(_mgfs_context_t));

	_g_mgfs_context_.fs_context.p_io_context = 0;
	_g_mgfs_context_.fs_context.p_read = sector_read;
	_g_mgfs_context_.fs_context.p_write = sector_write;
	_g_mgfs_context_.fs_context.p_alloc = mem_alloc;
	_g_mgfs_context_.fs_context.p_free = mem_free;
	_g_mgfs_context_.fs_context.p_lock = fs_lock;
	_g_mgfs_context_.fs_context.p_unlock = fs_unlock;
	_g_mgfs_context_.fs_context.p_timestamp = timestamp;
	_g_mgfs_context_.fs_context.p_info = fs_info;
	_g_mgfs_context_.fs_context.bmap_sz = 0;
	_g_mgfs_context_.fs_context.p_buffer_map = 0;
	
	mgfs_get_info(&_g_mgfs_context_);

	_g_mgfs_context_.flags = MGFS_DELAY_WRITE;
	_g_mgfs_context_.last_error = 0;
}

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

bool open_device(void) {
	bool r = false;
	_str_t dev_name;
	_u32 sz_name=0;

	if(clargs_option("-d", &dev_name, &sz_name)) {
		if(access(dev_name, R_OK|W_OK) == 0) {
			// open existing device
			_g_dev_ = map_file(dev_name, &_g_dev_size_);
			if(_g_dev_) {
				// check for requested device size
				_str_t str_dev_sz;
				_u32 dev_sz = 0;
				if(clargs_option("-s", &str_dev_sz, &dev_sz)) {
					if((_u64)atoll(str_dev_sz) != (_g_dev_size_ / (1024*1024))) {
						TRACE("%s\n","ERROR: device size does not matched with requested size");
					} else
						r = true;
				} else
					r = true;
			}
		} else {
			if(access(dev_name, F_OK) == 0) {
				TRACE("%s\n", "ERROR: access denied");
			} else {
				// create new device
				_str_t str_dev_sz;
				_u32 dev_sz = 0;
				if(clargs_option("-s", &str_dev_sz, &dev_sz)) {
					iHeap *p_heap = (iHeap *)__g_p_repository__->get_object_by_interface(
										I_HEAP, RY_STANDALONE);
					if(p_heap) {
						_u8 *p_mb = (_u8 *)p_heap->alloc(1024*1024);
						if(p_mb) {
							TRACE("Create device '%s'\n",dev_name);
							mem_set(p_mb, 0, 1024*1024);
							_u32 nmb = atoi(str_dev_sz);
							_u32 i = 0;
							FILE *f = fopen(dev_name,"w+");
							if(f) {
								for(i = 0; i < nmb; i++) {
									fwrite(p_mb,1024*1024,1,f);
									TRACE("%d MB \r", i);
									usleep(1000);
								}

								TRACE("%d MB done.\n", i);
								fclose(f);
							}

							p_heap->free(p_mb, 1024*1024);
						}

						__g_p_repository__->release_object(p_heap);
					}
				}

				if((_g_dev_ = map_file(dev_name, &_g_dev_size_)))
					r = true;
			}
		}
	}

	return r;
}

_cl_arg_t _g_options_[]={
	{ARG_TFLAG,	"--verbose",	"-V",	"be verbose"},
	{ARG_TFLAG,	"--version",	"-v",	"display version"},	
	{ARG_TFLAG,	"--help",	"-h",	"show this message and exit"},
	{ARG_TINPUT,	"--device",	"-d",	"target device '--device=<file name>'"},
	{ARG_TINPUT,	"--dsize",	"-s",	"device size in Megabytes '--dsize=<value>'"},
	{ARG_TINPUT,	"--usize",	"-u",	"unit size in sectors '--usize=<value>'"},
	{ARG_TINPUT,	"--fmap",	"-f",	"file map '--fmap=<file name>'"},
	{ARG_TINPUT,	"--bootS1",	"-a",	"stage 1 boot loader (volume boot record)"},
	{ARG_TINPUT,	"--bootS2",	"-b",	"stage 2 filesystem boot loader"},
	{ARG_TINPUT,	"--src",	"-S",	"source file"},
	{ARG_TINPUT,	"--dst",	"-D",	"destination file"},
	{ARG_TINPUT,	"--serial",	"-n",	"volume serial number"},
	{0,0,0,0}
};


_action_t _g_action_[]={
	{"create",	_create_fs_,	"create MGFS3 device (--device; --bootS1; --bootS2; --dsize; --usize; --fmap)"},
	{"read",	_read_file_,	"read file from MGFS3 \
(--device; --src; --dst; --bootS1; --bootS2)"},
	{"write",	_write_file_,	"write file to MGFS3 \
(--device; --src; --dst; --bootS1; --bootS2; --fmap)"},
	{"delete",	_delete_file_,	"delete file in MGFS3 (--device; --dst)"},
	{"move",	_move_file_,	"move file in MGFS3 (--device; --src; --dst)"},
	{"mkdir",	_mkdir_,	"create directory in MGFS3 (--device; --dst)"},
	{"rmdir",	_rmdir_,	"delete directory in MGFS3 (--device; --dst)"},
	{"slink",	_soft_link_,	"create soft link in MGFS3 (--device; --src; --dst)"},
	{"hlink",	_hard_link_,	"create hard link in MGFS3 (--device; --src; --dst)"},
	{"list",	_list_,		"list directory in MGFS3 (--device; --dst)"},
	{"status",	_status_,	"print status of MGFS3 device (--device)"},
	{0,		0}
};

void print_usage(void) {
	TRACE("mkmgfs3 version: %s\n", MKMGFS_VERSION);
	TRACE("%s\n", "\toptions:");
	clargs_print(_g_options_);
	TRACE("%s\n", "\tcommands:");
	_u32 n = 0;
	while(_g_action_[n].action) {
		TRACE("%s\t\t\t\t: %s\n", _g_action_[n].action, _g_action_[n].description);
		n++;
	}
}

int main(int argc, char* argv[]) {
	int r = 0;

	if(init_repository()) {
		// parse command line 
		if(clargs_parse(argc, argv, _g_options_)) {
			if(clargs_option("-h") || argc < 2) 
				print_usage();
			else {
				if(clargs_option("-v")) { 
					TRACE("mkmgfs3 version: %s\n", MKMGFS_VERSION);
				}

				if(open_device()) {
					// create fs context (necessarily having an open device)
					create_context();

					pthread_create(&t, &attr, cache_process, NULL);

					_str_t str_action = clargs_parameter(1);
					if(str_action) {
						_u32 n = 0;
						while(_g_action_[n].action) {
							if(str_cmp((_str_t)_g_action_[n].action, 
									str_action) == 0) {
								_g_action_[n].p_action();
								break;
							}

							n++;
						}

						if(!_g_action_[n].action) {
							TRACE("ERROR: Unknown command '%s'\n", str_action);
						}
					}

					if(_cp_running) {
						_cp_running = false;
						while(_cp_stop == false)
							usleep(1000);
					}

					munmap(_g_dev_, _g_dev_size_);
				} else {
					TRACE("%s\n", "ERROR: can't open device !");
				}
			}
		} else {
			clargs_print(_g_options_);
			TRACE("ERROR: %s", clargs_err_text());
		}

		free(_g_p_heap_);
	}

	return r;
}


