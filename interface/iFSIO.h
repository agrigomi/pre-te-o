#ifndef __I_FSIO_H__
#define __I_FSIO_H__

#ifdef __cplusplus
#include "iBase.h"
#endif

typedef struct {
	_u16	sector_size; // device sector in bytes
	_u8	unit_size; // number of filesystem sectors per unit
	_u32	nunits;  // total number of filesystem units
	_str_t	fs_name; // filesystem name
	_u16	fs_version; // filesystem version
}_fs_info_t;

// sector I/O (return number of affected sectors)
typedef _u32 _fs_read_sector_t(void *p_io_cxt, _u32 sector, _u32 count, void *buffer);
typedef _u32 _fs_write_sector_t(void *p_io_cxt, _u32 sector, _u32 count, void *buffer);
// memory allocation
typedef void *_fs_mem_alloc_t(void *p_io_cxt, _u32 size);
typedef void _fs_mem_free_t(void *p_io_cxt, void *ptr, _u32 size);
// synchronization
typedef _u64 _h_lock_;
typedef _h_lock_ _fs_lock_t(void *p_io_cxt, _h_lock_ hlock);
typedef void _fs_unlock_t(void *p_io_cxt, _h_lock_ hlock);
typedef _u32 _fs_timestamp_t(void *p_io_cxt);
typedef void _fs_get_info_t(void *p_io_cxt, _fs_info_t *p_fsi);

typedef struct {
	// filesystem implementation specific
	void	*p_io_context;

	// If true, buffer management do not write immediately,
	//  then needed additional call of method 'flush' from timer or thread periodically
	// If false, buffer management call 'flush' method immediately after 'write'
#ifdef __cplusplus	
	bool	write_back;
#else
	_u8	write_back;
#endif	

	// I/O callbacks
	_fs_read_sector_t 	*p_read;
	_fs_write_sector_t 	*p_write;
	_fs_mem_alloc_t		*p_alloc;
	_fs_mem_free_t		*p_free;
	_fs_lock_t		*p_lock;
	_fs_unlock_t		*p_unlock;
	_fs_timestamp_t		*p_timestamp;
	_fs_get_info_t		*p_info;

	// pointer to buffer map. Initially should be zero
	void	*p_buffer_map;
	// number of items in buffer map. Initially should be zero
	_u32	bmap_sz; 
}_fs_context_t;

#define I_FS_BUFFER	"iFSBuffer"

#define INVALID_BUFFER_HANDLE	0xffffffff
#define INVALID_UNIT_NUMBER	0xffffffff

typedef _u32 	_h_buffer_;
#ifdef __cplusplus
class iFSBuffer:public iBase {
public:
	DECLARE_INTERFACE(I_FS_BUFFER, iFSBuffer, iBase);

	virtual _h_buffer_ alloc(_fs_context_t *p_cxt, _u32 unit_number, _h_lock_ hlock=0)=0;
	virtual void *ptr(_fs_context_t *p_cxt, _h_buffer_ hb)=0;
	virtual _u32 unit(_fs_context_t *p_cxt, _h_buffer_ hb)=0;
	virtual void free(_fs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock=0)=0;
	virtual void write(_fs_context_t *p_cxt, _h_buffer_ hb, _h_lock_ hlock=0)=0;
	virtual void flush(_fs_context_t *p_cxt, _h_buffer_ hb=INVALID_BUFFER_HANDLE, _h_lock_ hlock=0)=0;
	virtual void cleanup(_fs_context_t *p_cxt, _h_lock_ hlock=0)=0;
};
#endif
#endif
