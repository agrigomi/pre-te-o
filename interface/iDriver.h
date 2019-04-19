#ifndef __I_DRIVER_H__
#define __I_DRIVER_H__

#include "iBase.h"
#include "iSyncObject.h"
#include "iScheduler.h"
#include "str.h"

#define I_DRIVER_BASE	"iDriverBase"
#define I_DRIVER_ROOT	"iDriverRoot"

// common driver type
#define DTYPE_VBUS	0	// virtual  bus
#define DTYPE_BUS	1	// system bus
#define DTYPE_CTRL	2	// controller
#define DTYPE_DEV	3	// device
#define DTYPE_VOL	4	// volume
#define DTYPE_FS	5	// file system

// driver I/O mode
#define DMODE_CHAR	1
#define DMODE_BLOCK	2
#define DMODE_LAN	3

// device/BUS driver class 
#define DCLASS_CDROM	1 // CD/DVD drive
#define DCLASS_HDD	2 // hard disk drive
#define DCLASS_DISPLAY	3 // video adapter
#define DCLASS_FDC	4 // floppy controller
#define DCLASS_FDD	5 // floppy disk drive
#define DCLASS_HDC	6 // hard disk controller
#define DCLASS_HID	7 // USB device (human interface)
#define DCLASS_1394	8 // IEEE 1394 host controller
#define DCLASS_IMG	9 // Cameras and scanners
#define DCLASS_KBD	10 // keyboard
#define DCLASS_MODEM	11 // modems
#define DCLASS_MOUSE	12 // pointing devices
#define DCLASS_MEDIA	13 // Audio and video devices
#define DCLASS_NET	14 // network adapter
#define DCLASS_PORT	15 // serial and paralel port
#define DCLASS_SCSI	16 // SCSI and RAID controllers
#define DCLASS_SYS	17 // System buses, bridges, etc (BUS or VBUS)
#define DCLASS_USB	18 // USB host controllers and hubs
#define DCLASS_RDD	19 // RAM disk drive
#define DCLASS_UMS	20 // USB mass storage
#define DCLASS_VFS	21 // virtual file system
#define DCLASS_FSC	22 // file system context

class iDriverBase;

// limits
#define INVALID_DEV_INDEX	0xff
#define MAX_DRIVER_IDENT	16
#define MAX_BUS_TYPE		6
#define MAX_CTRL_TYPE		6
#define MAX_VOLUME_NAME		6

//status
#define DSTATE_INITIALIZED	(1<<0)
#define DSTATE_ATTACHED		(1<<1)
#define DSTATE_SUSPEND		(1<<2)
#define DSTATE_PENDING		(1<<3)

struct __attribute__((packed)) _driver_t {
	_u8		_d_type_ :4;	// driver type
	_u8		_d_mode_ :4;	// driver I/O mode
	_u8		_d_class_;	// driver class
	_u16		_d_state_;	// driver status
	_s8		_d_ident_[MAX_DRIVER_IDENT];
	_u8		_reserved_;
	iDriverBase 	*_d_host_;
	_u16		_d_index_;	// driver number
	iMutex		*_d_mutex_;
	iDriverBase 	**_d_node_;
	
	union {
		struct { // BUS
			_u8	_bus_id_;
			_u8	_bus_type_[MAX_BUS_TYPE];
		};
		struct { // controller
			_u8 	_ctrl_index_;	// controller index
			_u8	_ctrl_type_[MAX_CTRL_TYPE];
		};
		struct { // block device info
			_u8	_block_dev_;	// block device number
			_u16	_block_size_;	// block size in bytes
			_u16	_max_op_sectors_;	// maximum number of sectors per I/O operation
			_u32	_block_dev_size_;	// number of device blocks
		};
		struct { // character device info
			_u8	_char_dev_;	// char device number
			_u8	_char_irq_;
			_u16	_char_flags_;
		};
		struct { // volume info
			_u16	_vol_block_size_;
			_u32	_vol_offset_;	// first volume block (dep. by device block size)
			_u32	_vol_size_;	// size of volume in device blocks
		};
		struct { // fs info
			_u16	_fs_version_;	// filesystem version
			_u16	_fs_unit_size_;	// sizeof filesystem block in bytes
			_u32	_fs_size_;	// total amount of filesystem blocks
		};
		struct { // lan info
			_u8	_lan_dev_;	// lan device number
			_u8	_lan_irq_;
			_u8	_lan_mac_[6];	// MAC address
		};
	};

	_driver_t() {
		_d_type_ = _d_mode_ = _d_class_ = 0;
		_reserved_ = 0;
		_d_host_ = 0;
		_d_mutex_ = 0;
		_d_state_ = 0;
		_d_index_ = INVALID_DEV_INDEX;
		for(_u32 i = 0; i < MAX_DRIVER_IDENT; i++)
			_d_ident_[i] = 0;
		_d_node_ = 0;
	}
};

// ioctl commands
#define DIOCTL_SUSPEND	10
#define DIOCTL_RESUME	11
#define DIOCTL_SREAD	12	// serial read
#define DIOCTL_BREAD	13	// block read
#define DIOCTL_FREAD	14	// file read
//#define DIOCTL_VFREAD	15	// formatted read
#define DIOCTL_SWRITE	16	// serial write
#define DIOCTL_BWRITE	17	// block write
#define DIOCTL_FWRITE	18	// file write
#define DIOCTL_VFWRITE	19	// formatted write
#define DIOCTL_FSEEK	20
#define DIOCTL_EJECT	21

// ioctl errors
#define DIOCTL_ERRCMD	-1	// unsupported command

typedef void*	 _handle_t;
typedef _u32 	 _d_res_t;
#define HANDLE	 _handle_t

typedef struct {
	union {
		struct {
			_u8	*buffer;// client buffer [IN]
			_u32	size;	// buffer size   [IN]
		};
		struct {
			_cstr_t	fmt;	// formatted output [IN]
			va_list *args;	// argument list [IN]
		};
		struct {
			_str_t		name;	// file name [IN]
			_str_t		_name;	// destination file name [IN]
			_u32		user_id;	// [IN]
			_u32		pmask;	// permissions mask [IN]
			HANDLE		_handle;	// destination handle [IN/OUT]
		};
	};
	union {
		_u64	lba;		// block device LBA [IN]
		_u64	offset;		// volume | file | controller offset [IN]
	};
	union {
		_u32	result;		// [OUT]
		_u32	count; 		// byte/block count [IN/OUT]
		_u64	result64;	// [OUT]
	};
	HANDLE		handle; 	// file handle [IN]
	_d_res_t	error;		// error code [OUT]
	void		*p_data; 	// implementation specific [IN]
}_driver_io_t;

class iDriverBase:public iBase, public _driver_t {
public:
	DECLARE_INTERFACE(I_DRIVER_BASE, iDriverBase, iBase);

	virtual bool driver_init(iDriverBase *p_host_drv=0)=0; // set host
	virtual void driver_uninit(void)=0;
	// I/O
	virtual void driver_ioctl(_u32 _UNUSED_ cmd, _driver_io_t _UNUSED_ *p_dio) {}
	

	// ATTENTION: Do not overwrite the functions below !!!
	iMutex *driver_mutex(void) {
		return _d_mutex_;
	}
	iDriverBase *driver_host(void) { // get host
		return _d_host_;
	}
	HMUTEX lock(HMUTEX hlock=0) {
		HMUTEX r = 0;
		if(_d_mutex_)
			r = acquire_mutex(_d_mutex_, hlock);
		return r;
	}
	void unlock(HMUTEX hlock) {
		if(_d_mutex_)
			_d_mutex_->unlock(hlock);
	}
	void suspend(void) {
		driver_ioctl(DIOCTL_SUSPEND, 0);
	}
	void resume(void) {
		driver_ioctl(DIOCTL_RESUME, 0);
	}
	// serial I/O
	_d_res_t read(_u8 *p_buffer, _u32 sz, _u32 *nb, HANDLE handle=0, void *p_udata=0) {
		_driver_io_t dio;
		dio.buffer = p_buffer;
		dio.size = dio.count = sz;
		dio.handle = handle;
		dio.p_data = p_udata;
		driver_ioctl(DIOCTL_SREAD, &dio);
		*nb = dio.result;
		return dio.error;
	}
	_d_res_t write(_u8 *p_data, _u32 sz=0, _u32 *nb=0, HANDLE handle=0, void *p_udata=0) {
		_driver_io_t dio;
		dio.buffer = p_data;
		dio.size = dio.count = sz;
		dio.handle = handle;
		dio.p_data = p_udata;
		driver_ioctl(DIOCTL_SWRITE, &dio);
		if(nb)
			*nb = dio.result;
		return dio.error;
	}
	_u32 fwrite(_cstr_t format,...) {
		va_list args;
		_driver_io_t dio;
		va_start(args, format);
		dio.fmt = format;
		dio.args = &args;
		driver_ioctl(DIOCTL_VFWRITE, &dio);
		va_end(args);
		return dio.result;
	}
	// block I/O (return number of bytes)
	_d_res_t read(_u64 block, // first block number
			  _u32 count, // number of blocks to read
			  _u8  *p_buffer, _u32 *nb, void *p_udata=0) {
		_driver_io_t dio;
		dio.lba = block;
		dio.count = dio.size = count;
		dio.buffer = p_buffer;
		dio.p_data = p_udata;
		driver_ioctl(DIOCTL_BREAD, &dio);
		*nb = dio.result;
		return dio.error;
	}
	_d_res_t write(_u64 block, // first block number
			  _u32 count, // number of blocks to write
			  _u8  *p_data, _u32 *nb, void *p_udata=0) {
		_driver_io_t dio;
		dio.lba = block;
		dio.count = dio.size = count;
		dio.buffer = p_data;
		dio.p_data = p_udata;
		driver_ioctl(DIOCTL_BWRITE, &dio);
		*nb = dio.result;
		return dio.error;
	}
	// volume & filesystem I/O
	_d_res_t read(_u64 offset, _u8 *p_buffer, _u32 sz, _u32 *nb, HANDLE handle=0, void *p_udata=0) {
		_driver_io_t dio;
		dio.offset = offset;
		dio.buffer = p_buffer;
		dio.size = dio.count = sz;
		dio.handle = handle;
		dio.p_data = p_udata;
		driver_ioctl(DIOCTL_FREAD, &dio);
		*nb = dio.result;
		return dio.error;
	}
	_d_res_t write(_u64 offset, _u8 *p_data, _u32 sz, _u32 *nb, HANDLE handle=0, void *p_udata=0) {
		_driver_io_t dio;
		dio.offset = offset;
		dio.buffer = p_data;
		dio.size = dio.count = sz;
		dio.handle = handle;
		dio.p_data = p_udata;
		driver_ioctl(DIOCTL_FWRITE, &dio);
		*nb = dio.result;
		return dio.error;
	}
	_d_res_t seek(_u64 offset, HANDLE handle=0, void *p_udata=0) {
		_driver_io_t dio;
		dio.offset = offset;
		dio.handle = handle;
		dio.p_data = p_udata;
		driver_ioctl(DIOCTL_FSEEK, &dio);
		return dio.error;
	}
	_d_res_t eject(void) {
		_driver_io_t dio;
		driver_ioctl(DIOCTL_EJECT, &dio);
		return dio.error;
	}
};

// driver request flags
#define DRF_INTERFACE		(1<<0)
#define DRF_PARENT_INTERFACE	(1<<1)
#define DRF_OBJECT_NAME		(1<<2)
#define DRF_IDENT		(1<<3)
#define DRF_TYPE		(1<<4)
#define DRF_MODE		(1<<5)
#define DRF_CLASS		(1<<6)
#define DRF_INDEX		(1<<7)

typedef struct {
	_u8		flags;
	_u8		_d_type_ :4;
	_u8		_d_mode_ :4;
	_u8		_d_class_;
	_u16		_d_index_;
	_cstr_t		_d_ident_;
	_cstr_t		_d_interface_;
	_cstr_t		_d_parent_interface_;
	_cstr_t		_d_name_;
}__attribute__((packed)) _driver_request_t;

class iDriverRoot: public iDriverBase {
public:
	DECLARE_INTERFACE(I_DRIVER_ROOT, iDriverRoot, iDriverBase);
	virtual void register_driver(iDriverBase *p_drv, iDriverBase *p_host=0)=0;
	virtual void remove_driver(iDriverBase *p_drv, bool recursive=false)=0;
	virtual void sys_update(void)=0;
	virtual bool enum_driver(_u32 idx, iDriverBase **pp_drv, iDriverBase **pp_dhost)=0;
	virtual iDriverBase *driver_request(_driver_request_t *p_dr, iDriverBase *p_host_drv=0)=0;
	virtual void suspend_node(iDriverBase *p_drv)=0;
	virtual void resume_node(iDriverBase *p_drv)=0;
	virtual _u16 get_node_limit(void)=0;
};

#define DRIVER_HOST			_d_host_
#define DRIVER_TYPE			_d_type_
#define DRIVER_MODE			_d_mode_
#define DRIVER_CLASS			_d_class_
#define DRIVER_STATUS			_d_state_
#define DRIVER_NAME			_d_name_
#define DRIVER_IDENT			_d_ident_
#define DRIVER_NODE			_d_node_
#define DRIVER_INDEX			_d_index_
#define DRIVER_INTERFACE 		_d_interface_
#define DRIVER_PARENT_INTERFACE		_d_parent_interface_
#define DRIVER_MUTEX			_d_mutex_

#define DECLARE_DRIVER_INIT() \
	bool driver_init(iDriverBase *p_host_drv=0);
#define DECLARE_DRIVER_UNINIT() \
	void driver_uninit(void)
#define IMPLEMENT_DRIVER_INIT(_class_name_, host) \
	bool _OPTIMIZE_SIZE_ _class_name_::driver_init(iDriverBase *host)
#define IMPLEMENT_DRIVER_UNINIT(_class_name_) \
	void _OPTIMIZE_SIZE_ _class_name_::driver_uninit(void)
#define INIT_MUTEX() DRIVER_MUTEX = (iMutex *)OBJECT_BY_INTERFACE(I_MUTEX, RY_CLONE)
#define GET_MUTEX() driver_mutex()
#define RELEASE_MUTEX() RELEASE_OBJECT(_d_mutex_)
#define GET_DRIVER_ROOT() (iDriverRoot *)OBJECT_BY_INTERFACE(I_DRIVER_ROOT, RY_STANDALONE)
#define REGISTER_DRIVER() \
	iDriverRoot *_p_droot_ =  GET_DRIVER_ROOT();\
	if(_p_droot_) { \
		_p_droot_->register_driver(this); \
		RELEASE_OBJECT(_p_droot_); \
	}
#endif
