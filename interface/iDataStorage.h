#ifndef __I_DATA_STORAGE__
#define __I_DATA_STORAGE__

#include "iMemory.h"
#include "iSyncObject.h"

#define I_DATA_STORAGE	"iDataStorage"
#define I_VECTOR	"iVector"
#define I_QUEUE		"iQueue"
#define I_BTREE		"iBTree"
#define I_ENV		"iEnv"

class iDataStorage:public iBase {
public:
	DECLARE_INTERFACE(I_DATA_STORAGE, iDataStorage, iBase);
	
	// init the data storage
  	virtual void init(_u32 data_sz, _u8 ncol=1, iHeap *p_heap=0) = 0;
	
	// return pointer to data at 'index'
	virtual void *get(_u32 index, HMUTEX hlock=0) = 0;
	
	// add item at the end of storage and return pointer to user data
	virtual void *add(void *p_data, HMUTEX hlock=0) = 0;
	
	// insert item in place passed by 'index' parameter and return pointer user data
	virtual void *ins(_u32 index, void *p_data, HMUTEX hlock=0) = 0;
	
	// delete item
	virtual void del(_u32 index, HMUTEX hlock=0) = 0;
	// delete current item
	virtual void del(HMUTEX hlock=0)=0;
	
	// delete all items
	virtual void clr(HMUTEX hlock=0) = 0;

	// return number of items
	virtual _u32 cnt(HMUTEX hlock=0) = 0;

	// select column
	virtual void col(_u8 ccol, HMUTEX hlock=0)=0;
	
	// select record and make it current (if it owns by atorage)
	virtual bool sel(void *p_data, HMUTEX hlock=0)=0;

	// move record between columns
	virtual bool mov(void *p_data, _u8 col, HMUTEX hlock=0)=0;

	virtual void *next(HMUTEX hlock=0)=0;
	virtual void *current(HMUTEX hlock=0)=0;
	virtual void *first(HMUTEX hlock=0)=0;
	virtual void *last(HMUTEX hlock=0)=0;
	virtual void *prev(HMUTEX hlock=0)=0;

	virtual HMUTEX lock(HMUTEX hlock=0)=0;
	virtual void unlock(HMUTEX hlock)=0;
	virtual iMutex *get_mutex(void)=0;
};


class iVector:public iDataStorage {
public:
	DECLARE_INTERFACE(I_VECTOR, iVector, iDataStorage);
};

class iQueue:public iDataStorage {
public:
	DECLARE_INTERFACE(I_QUEUE, iQueue, iDataStorage);
	virtual void roll(HMUTEX hlock=0)=0;
};

typedef _u64	_bt_key_t;

class iBTree:public iBase {
public:	
	DECLARE_INTERFACE(I_BTREE, iBTree, iBase);
	virtual void init(iHeap *p_heap=0)=0;
	virtual _bt_key_t add(void *data, _u32 size, HMUTEX hlock=0)=0;
	virtual _bt_key_t key(void *data, _u32 size)=0;
	virtual void *get(_bt_key_t key, _u32 *dsz=0,HMUTEX hlock=0)=0;
	virtual void del(_bt_key_t key, HMUTEX hlock=0)=0;
	virtual HMUTEX lock(HMUTEX hlock=0)=0;
	virtual void unlock(HMUTEX hlock)=0;
	virtual iMutex *get_mutex(void)=0;
};

class iEnv:public iBase {
public:
	DECLARE_INTERFACE(I_ENV, iEnv, iBase);
	virtual bool get(_str_t var, _str_t *val, _u32 *sz_val)=0;
	virtual bool get(_u32 idx, _str_t *val, _u32 *sz_val)=0;
	virtual bool set(_str_t var, _str_t val, _u32 sz_val)=0;
	virtual void clr(void)=0;
};

#endif
