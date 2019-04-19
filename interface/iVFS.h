#ifndef __I_VFS_H__
#define __I_VFS_H__

#include "iBlockDevice.h"

#define I_FS_HANDLE	"iFSHandle"
#define I_FS_HOST	"iFSHost"
#define I_FS		"iFS"
#define I_FS_CONTEXT	"iFSContext"
#define I_FS_ROOT	"iFSRoot"

// permissions
#define FSP_ALL_EXEC		(0x01 << 0)
#define FSP_ALL_WRITE		(0x01 << 1)
#define FSP_ALL_READ		(0x01 << 2)
#define FSP_GROUP_EXEC		(0x01 << 3)
#define FSP_GROUP_WRITE		(0x01 << 4)
#define FSP_GROUP_READ		(0x01 << 5)
#define FSP_OWNER_EXEC		(0x01 << 6)
#define FSP_OWNER_WRITE		(0x01 << 7)
#define FSP_OWNER_READ		(0x01 << 8)
#define FSP_DIR			(0x01 << 9)
#define FSP_HIDDEN		(0x01 << 10)
#define FSP_SLINK		(0x01 << 11)
#define FSP_BLOCK_DEVICE	(0x01 << 14) // 0 - serial device
#define FSP_NETWORK_DEVICE	(0x01 << 15)

// ioctl commands
#define DIOCTL_FOPEN	101
#define DIOCTL_FCLOSE	102
#define DIOCTL_FCREATE	103
#define DIOCTL_FDELETE	104
#define DIOCTL_FLINK	105
#define DIOCTL_FSLINK	106
#define DIOCTL_FMOVE	107
#define DIOCTL_FPMASK	108	// get permissions mask
#define DIOCTL_FSIZE	109
#define DIOCTL_FINFO	110

#define FS_MAX_HANDLE_DATA	64
#define FS_MAX_PATH		256

#define FS_RESULT_OK			0
#define FS_RESULT_ERR			FS_RESULT_OK - 1
#define FS_RESULT_UNKNOWN		FS_RESULT_OK - 2
#define FS_RESULT_INVALID_PATH		FS_RESULT_OK - 3
#define FS_RESULT_ITRC_ERR		FS_RESULT_OK - 4
#define FS_RESULT_INVALID_HANDLE	FS_RESULT_OK - 5

typedef _d_res_t _fs_res_t;

// handle type
#define HTYPE_DEVICE	1
#define HTYPE_VOLUME	2
#define HTYPE_FSCONTEXT	3
#define HTYPE_FILE	4

#define FSHANDLE	HANDLE

// handle state
#define HSTATE_OPEN		(1<<0)
#define HSTATE_IO_PENDING	(1<<1)
#define HSTATE_CLOSE_PENDING	(1<<2)

class iFSHandle: public iBase {
public:
	DECLARE_INTERFACE(I_FS_HANDLE, iFSHandle, iBase);
	virtual FSHANDLE create(iDriverBase *p_drv, _u32 user, _u8 type, _u8 *data=0)=0;
	virtual bool update_user(FSHANDLE h, _u32 user)=0;
	virtual bool update_state(FSHANDLE h, _u8 state)=0;
	virtual bool update_data(FSHANDLE h, _u8 *data)=0;
	virtual bool compare(FSHANDLE h1, FSHANDLE h2)=0;
	virtual void remove(FSHANDLE h)=0;
	virtual _u8  *data(FSHANDLE h)=0;
	virtual _u8  state(FSHANDLE h)=0;
	virtual _u32 user(FSHANDLE h)=0;
	virtual _u8  type(FSHANDLE h)=0;
	virtual iDriverBase *driver(FSHANDLE h)=0;
};

typedef struct {
	_str_t	fname;	// file name
	_str_t	lname;	// link name (full path)
	_u64	fsize;	// file size in bytes
	_u32	ctime;	// creation time
	_u32	mtime;	// last modification time
	_u32	usrid;	// user ID
	_u32	pmask;	// permissions mask
}_fs_file_info_t;

typedef struct {
	_u16	pbsize;	// physical block size
	_u16	lbsize;	// logical block size
	_u64	tpbnum; // total storage size in physical blocks
	_u16	nitems; // number of storage handles
	_str_t	serial; // serial number
	_str_t	fsname; // filesystem name
}_fs_context_info_t;

typedef void _fs_list_t(_fs_file_info_t *p_fi, void *p_udata);

//////////////////////// FILESYSTEM IMPLEMENTATION SPECIFIC //////////////////////////////////

class iFSContext: public iDriverBase {
public:
	DECLARE_INTERFACE(I_FS_CONTEXT, iFSContext, iBase);
	virtual bool format_storage(_u64 sz_dev, _u8 usize=1, 
			_u8 flags=0, _str_t serial=0, _u8 *vbr=0, _u8 *fsbr=0, _u32 sz_fsbr=0)=0;
	// add storage, and return internal handle to this storage
	virtual void add_storage(FSHANDLE fsh)=0; 
	virtual bool add_storage(iVolume *p_vol)=0;

	virtual bool remove_storage(FSHANDLE fsh)=0; // remove storage
	virtual bool remove_storage(iVolume *p_vol)=0; // remove storage

	virtual bool verify_storage(FSHANDLE fsh)=0; // return true if this context own this handle as storage
	virtual bool verify_storage(iVolume *p_vol)=0; // return true if this context own this volume as storage

	virtual void info(_fs_context_info_t *p_cxti)=0;
	virtual FSHANDLE root(void)=0; // get handle to ROOT directory
	virtual void list(FSHANDLE hdir, _fs_list_t *p_cb_list, void *p_udata=0)=0;



	//// driver_ioctl wrappers ////
	// ATTENTION: Do not overwrite !!!
	_fs_res_t fopen(_str_t name, // [in] name only (without path)
			FSHANDLE hcontainer, // [in]
			FSHANDLE *hfile, // [out]
			void *p_udata=0 // [in]
		  ) {
		_driver_io_t dio;
		dio.name = name;
		dio.handle = hcontainer;
		dio.p_data = p_udata;
		driver_ioctl(DIOCTL_FOPEN, &dio);
		*hfile = dio._handle;
		return dio.error;
	}
	_fs_res_t fread(_u64 offset, _u8 *p_buffer, _u32 sz, _u32 *nb, FSHANDLE handle=0, void *p_udata=0) {
		return read(offset, p_buffer, sz, nb, handle, p_udata);
	}
	_fs_res_t fread(_u8 *p_buffer, _u32 sz, _u32 *nb, FSHANDLE handle=0, void *p_udata=0) {
		return read(p_buffer, sz, nb, handle, p_udata);
	}
	_fs_res_t fwrite(_u64 offset, _u8 *p_data, _u32 sz, _u32 *nb, FSHANDLE handle=0, void *p_udata=0) {
		return write(offset, p_data, sz, nb, handle, p_udata);
	}
	_fs_res_t fwrite(_u8 *p_buffer, _u32 sz, _u32 *nb, FSHANDLE handle=0, void *p_udata=0) {
		return write(p_buffer, sz, nb, handle, p_udata);
	}
	_fs_res_t fseek(_u64 offset, FSHANDLE handle=0, void *p_udata=0) {
		return seek(offset, handle, p_udata);
	}
	_fs_res_t fclose(FSHANDLE handle, void *p_udata=0) {
		_driver_io_t dio;
		dio.handle = handle;
		dio.p_data = p_udata;
		driver_ioctl(DIOCTL_FCLOSE, &dio);
		return dio.error;
	}
	_fs_res_t fcreate(_u32 user_id, _str_t name, // [in]
			_u32 pmask, // [in]
			FSHANDLE hcontainer, // [in]
			FSHANDLE *hfile, // [out]
			void *p_udata=0 // [in]
	  	    ) {
		_driver_io_t dio;
		dio.name = name;
		dio.pmask = pmask;
		dio.handle = hcontainer;
		dio.user_id = user_id;
		dio.p_data = p_udata;
		driver_ioctl(DIOCTL_FCREATE, &dio);
		*hfile = dio._handle;
		return dio.error;
	}
	_fs_res_t fdelete(_str_t name, FSHANDLE hcontainer, void *p_udata=0) {
		_driver_io_t dio;
		dio.name = name;
		dio.handle = hcontainer;
		dio.p_data = p_udata;
		driver_ioctl(DIOCTL_FDELETE, &dio);
		return dio.error;
	}
	_fs_res_t flink(_str_t name, // link file name
			FSHANDLE hcontainer, // directory handle for link place
			_str_t _name, // destination file name
			FSHANDLE _hcontainer, // directory handle for destination file
			void *p_udata=0
		  ) {
		_driver_io_t dio;
		dio.name = name;
		dio.handle = hcontainer;
		dio._name = _name;
		dio._handle = _hcontainer;
		dio.p_data = p_udata;
		driver_ioctl(DIOCTL_FLINK, &dio);
		return dio.error;
	}
	_fs_res_t fslink(_str_t name, // link file name
			FSHANDLE hcontainer, // directory handle for link place
			_str_t _name, // path to destination file
			void *p_udata=0
		   ) {
		_driver_io_t dio;
		dio.name = name;
		dio.handle = hcontainer;
		dio._name = _name;
		dio._handle = 0;
		dio.p_data = p_udata;
		driver_ioctl(DIOCTL_FSLINK, &dio);
		return dio.error;
	}
	_fs_res_t fmove(_str_t name, // source file name
			FSHANDLE hcontainer, // directory handle for source file
			_str_t _name, // destination file nmae
			FSHANDLE _hcontainer, // directory handle for destination file
			void *p_udata=0
		  ) {
		_driver_io_t dio;
		dio.name = name;
		dio.handle = hcontainer;
		dio._name = _name;
		dio._handle = _hcontainer;
		dio.p_data = p_udata;
		driver_ioctl(DIOCTL_FMOVE, &dio);
		return dio.error;
	}
	_fs_res_t fpmask(FSHANDLE handle, _u32 *pmask, void *p_udata=0) {
		_driver_io_t dio;
		dio.handle = handle;
		dio.p_data = p_udata;
		driver_ioctl(DIOCTL_FPMASK, &dio);
		*pmask = dio.result;
		return dio.error;
	}
	_fs_res_t fsize(FSHANDLE handle, _u64 *nb, void *p_udata=0) {
		_driver_io_t dio;
		dio.handle = handle;
		dio.p_data = p_udata;
		driver_ioctl(DIOCTL_FSIZE, &dio);
		*nb = dio.result64;
		return dio.error;
	}
	_fs_res_t finfo(FSHANDLE handle, _fs_file_info_t *p_fi) {
		_driver_io_t dio;
		dio.handle = handle;
		dio.p_data = p_fi;
		driver_ioctl(DIOCTL_FINFO, &dio);
		return dio.error;
	}
};

class iFS:public iDriverBase {
public:
	DECLARE_INTERFACE(I_FS, iFS, iDriverBase);
	virtual bool format(FSHANDLE hstorage, 
				_u64 sz_dev, 
				_u8 usize=1, 
				_u8 flags=0, 
				_str_t serial=0, 
				_u8 *vbr=0, 
				_u8 *fsbr=0, 
				_u32 sz_fsbr=0
				)=0;
	// identify filesystem
	virtual bool ident(FSHANDLE fsh)=0;
	virtual bool ident_volume(iVolume *p_vol)=0;
	virtual bool ident_context(iFSContext *p_cxt)=0;
	// create empty context
	virtual iFSContext *create_context(void)=0;
	// destroy existing context
	virtual void destroy_context(iFSContext *p_cxt)=0;
	// get filesystem context by zero based index
	virtual iFSContext *get_context(_u32 cidx)=0;
	// get filesystem context by storage
	virtual iFSContext *get_context(FSHANDLE hstorage)=0;
	virtual iFSContext *get_context(iVolume *p_vol)=0;
};





////////////////////////////// STANDALONE ///////////////////////////////////////////////////////
// ATTENTION: DO NOT REIMPLEMENT

class iFSHost:public iDriverBase {
public:
	DECLARE_INTERFACE(I_FS_HOST, iFSHost, iDriverBase);
	
	// return filesystem by zero based index
	virtual iFS *filesystem(_u32 idx)=0;
	// return volume by zero based index
	virtual iVolume *volume(_u32 idx)=0;
	// return volume by serial number
	virtual iVolume *volume_by_serial(_str_t serial)=0;
	// return volume by name (DRIVER_IDENT)
	virtual iVolume *volume_by_name(_str_t name)=0;
	// identify filesystem
	virtual iFS *ident(iVolume *p_vol)=0;
	virtual iFS *ident(FSHANDLE hstorage)=0;
};

#define FSP_CREATE_NEW		(1<<31)

class iFSRoot:public iBase {
public:
	DECLARE_INTERFACE(I_FS_ROOT, iFSRoot, iBase);

	virtual _fs_res_t mount(FSHANDLE hstorage, _str_t mp_name, _str_t mount_point)=0;
	virtual _fs_res_t mount(iVolume *p_vol, _str_t mount_point)=0;
	virtual _fs_res_t mount(_str_t volume_name, _str_t mount_point)=0;
	virtual _fs_res_t unmount(_str_t mount_point)=0;
	virtual _fs_res_t fopen(_u32 user_id, // [in] it's required, when create a new file
				_str_t path, // [in] full path if 'hcd' parameter is not specified
				_str_t fname, // [in] // required when create a new file
				_u32 flags, // [in] flags + permissions mask
				FSHANDLE *hfile, // [out] valid handle when function return FS_RESULT_OK
				FSHANDLE hcd=0 // [in] handle of current directory
			      )=0;
	virtual _fs_res_t fclose(FSHANDLE hfile)=0;
	virtual _fs_res_t fsize(FSHANDLE hfile, _u64 *nb)=0;
	virtual _fs_res_t fread(FSHANDLE hfile, // [in]
			   	void *buffer, // [in] // input buffer
			   	_u32 sz,       // [in] // bytes to read
			   	_u32 *nb // [out] number of transfered bytes
			  	)=0;
	virtual _fs_res_t fwrite(FSHANDLE hfile, // [in]
			    	void *data, // [in] output buffer
				_u32 sz,	  // bytes to write
			    	_u32 *nb // [out] number of transfered bytes
			   	)=0;
	virtual _fs_res_t fseek(FSHANDLE hfile, // [in]
				 _u64 offset // [in] offset from begin
				)=0;
	virtual _fs_res_t finfo(FSHANDLE hfile, _fs_file_info_t *p_fi)=0;
};

#define GET_FS_ROOT() (iFSRoot *)OBJECT_BY_INTERFACE(I_FS_ROOT, RY_STANDALONE)
#define GET_FS_HOST() (iFSHost *)OBJECT_BY_INTERFACE(I_FS_HOST, RY_STANDALONE)

#endif

