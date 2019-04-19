#ifndef __I_BLOCK_DEVICE__
#define __I_BLOCK_DEVICE__

#include "iDriver.h"
#include "iMemory.h"
#include "iSyncObject.h"

#define I_BDCTRL_HOST		"iBDCtrlHost"
#define I_VOLUME		"iVolume" // volume driver
#define I_CACHE			"iCache"

#define I_IDE_CONTROLLER	"iIDEController"
#define I_IDE_CHANNEL		"iIDEChannel"
#define I_IDE_DEVICE		"iIDEDevice"

#define I_AHCI_CONTROLLER	"iAHCIController"
#define I_AHCI_DEVICE		"iAHCIDevice"

#define I_FD_CONTROLLER		"iFDController"
#define I_FD_DEVICE		"iFDDevice"

#define I_RAM_DEVICE		"iRamDevice"

class iBDCtrlHost:public iDriverBase { // host driver of block device controllers
public:
	DECLARE_INTERFACE(I_BDCTRL_HOST, iBDCtrlHost, iDriverBase);
};

#define MAX_VOLUME_IDENT	16

#define VF_READONLY	(1<<0)
#define VF_NOCACHE	(1<<1)
#define VF_HOSTCTRL	(1<<2)

typedef struct {
	_u8	_vol_flags_;
	_u8 	_vol_cidx_;	// controller index
	_u8	_vol_didx_;	// drive index
	_u8	_vol_pidx_;	// partition index (0 for whole device)
	_u8 	_vol_ident_[MAX_VOLUME_IDENT]; // volume serial number
	void	*_vol_pvd_;	// volume implementation specific
}_volume_t;

class iVolume: public iDriverBase, public _volume_t {
public:
	DECLARE_INTERFACE(I_VOLUME, iVolume, iDriverBase);
	virtual _str_t name(void)=0;
	virtual _str_t serial(void)=0;
};

// cache I/O commands
#define CIOCTL_READ		1
#define CIOCTL_WRITE		2
#define CIOCTL_FLUSH		3

typedef	void* HCACHE;

typedef struct {
	_u64	bstart;		// start block
	_u16	boffset;	// offset in first block
	_u32	bcount;		// block count
	_u32	bsize;		// transfer size in bytes
	_u8	*buffer;	// io buffer (can be NULL for cache buffer request)
	void	*p_udata;	// cache user data
	HCACHE	handle;		// cache handle
	void	*plock;		// lock info
	HMUTEX	hlock;		// mutex handle for 'plock'
}_cache_io_t;

typedef _u32 _cb_cache_io_t(_u32 cmd, _cache_io_t *p_cio);

typedef struct {
	_u16	block_size;	// physical block size
	_u16	chunk_size;	// number of physical blocks in one chunk
	_u16	num_chunks;	// number of chunks
	_cb_cache_io_t *p_ccb;	// cache callback
	void	*p_udata;	// user specific data
}__attribute__((packed)) _cache_info_t;

class iCache:public iBase {
public:
	DECLARE_INTERFACE(I_CACHE, iCache, iBase);
	virtual HCACHE alloc(_cache_info_t *p_ci)=0;
	virtual void free(HCACHE handle)=0;
	virtual _u32 cache_ioctl(_u32 cmd, _cache_io_t *cio)=0;
};
///////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
	_u16	_ide_base_primary_;	// primary base port
	_u16	_ide_ctrl_primary_;	// primary ctrl port
	_u16	_ide_base_secondary_;	// secondary base port
	_u16	_ide_ctrl_secondary_;	// secondary ctrl port
	_u16	_ide_bus_master_;	// bus master IDE
	_u8	_ide_prog_if_;		// proramming interface byte
	_u8	_ide_irq_;		// IRQ marker (PCI interrupt line)
	_u32	_ide_pci_index_;	// PCI device index (0xffffffff for invalid index)
}_ide_ctrl_t;

class iIDEController:public iDriverBase, public _ide_ctrl_t {
public:
	DECLARE_INTERFACE(I_IDE_CONTROLLER, iIDEController, iDriverBase);
	virtual _u8 read(_u8 channel, _u8 reg)=0;
	virtual void write(_u8 channel, _u8 reg, _u8 data)=0;
	virtual _u32 read(_u8 channel, _u8 *p_buffer, _u32 sz)=0;
	virtual _u32 write(_u8 channel, _u8 *p_buffer, _u32 sz)=0;
	virtual bool polling(_u8 channel)=0;
	virtual void wait(_u8 channel)=0;
	virtual void irq_on(_u8 channel)=0;
	virtual void irq_off(_u8 channel)=0;
	virtual void wait_irq(_u8 channel, _u32 timeout=EVENT_TIMEOUT_INFINITE)=0;
};

typedef struct {
	_u8  		_ch_index_;	// ATA_PRIMARY or ATA_SECONDARY
	_u16 		_ch_reg_base_;	// I/O Base
	_u16 		_ch_reg_ctrl_;	// Controll base
	_u16 		_ch_reg_bmide_;	// Bus Master IDE
	_u8  		_ch_int_;	// enable/disable Interrupt
	_u8		_ch_ata_index_;	// selected device (ATA_MASTER or ATA_SLAVE)
	iIDEController	*_ch_ctrl_;	// IDE controller
}_ide_channel_t;

class iIDEChannel:public iBase, public _ide_channel_t {
public:
	DECLARE_INTERFACE(I_IDE_CHANNEL, iIDEChannel, iBase);
	virtual bool init(void)=0;
	virtual HMUTEX lock(HMUTEX hlock=0);
	virtual void unlock(HMUTEX hlock);
	
	virtual _u32 read_sector(_u8 ata_idx, _u64 lba, _u8 *p_buffer, _u16 count, HMUTEX hlock=0)=0;
	virtual _u32 write_sector(_u8 ata_idx, _u64 lba, _u8 *p_data, _u16 count, HMUTEX hlock=0)=0;
	virtual void eject(_u8 ata_idx)=0;
};

typedef struct {
	_u8		_ata_index_; 		// device index (ATA_MASTER or ATA_SLAVE)
	_u8 		_ata_type_;  		// device type (IDE_ATA or IDE_ATAPI)
	_u16 		_ata_signature_;
	_u16 		_ata_features_;
	_u32 		_ata_command_sets_;
	_u32 		_ata_size_; 		// size in sectors
	_u8  		_ata_model_[42];
	iIDEChannel	*_ata_channel_; 	// IDE channel
}_ide_device_t;

class iIDEDevice:public iDriverBase, public _ide_device_t {
public:
	DECLARE_INTERFACE(I_IDE_DEVICE, iIDEDevice, iDriverBase);
};

///////////////////////////////////////////////////////////////////////////////////////////////////

typedef  struct {
	_vaddr_t	_ahci_abar_;		// controller memory map
	_u8		_ahci_prog_if_;		// proramming interface byte
	_u8		_ahci_irq_;		// IRQ marker (PCI interrupt line)
	_u32		_ahci_pci_index_;	// PCI device index (0xffffffff for invalid index)
}_ahci_ctrl_t;

class iAHCIController:public iDriverBase, public _ahci_ctrl_t {
public:	
	DECLARE_INTERFACE(I_AHCI_CONTROLLER, iAHCIController, iDriverBase);
	virtual void wait_irq(_u32 timeout=EVENT_TIMEOUT_INFINITE)=0;
	virtual _u32 read(_u8 reg)=0; // read HBA register
	virtual void write(_u8 reg, _u32 value)=0; // write HBA register
};

typedef struct {
	_u8		_sata_index_;		// sata index per controller (0..31)
	_vaddr_t	_sata_port_;		// port memory map
	_u8		_sata_max_cmd_count_;	// max. number of commands
	_u32 		_sata_command_sets_;
	_u8  		_sata_model_[42];
}_ahci_device_t;

class iAHCIDevice:public iDriverBase, public _ahci_device_t {
public:
	DECLARE_INTERFACE(I_AHCI_DEVICE, iAHCIDevice, iDriverBase);
};

/////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
	_u32	_fdc_base_;
	_u8	_fdc_detect_; // high 4 bits for FDA & low 4 bits for FDB
}_fd_ctrl_t;

class iFDController:public iDriverBase, public _fd_ctrl_t {
public:
	DECLARE_INTERFACE(I_FD_CONTROLLER, iFDController, iDriverBase);
	virtual _u8 read_reg(_u32 reg)=0;
	virtual void write_reg(_u32 reg, _u8 value)=0;
	virtual bool wait_irq(_u32 timeout=EVENT_TIMEOUT_INFINITE)=0;
	virtual void enable_irq(bool eirq=true)=0;
	virtual void select(_u8 dev_idx)=0;
};

typedef struct {
	_u8		_fdd_index_;
	_u8		_fdd_data_rate_;
	_u8		_fdd_tracks_; 	// cylindres
	_u8		_fdd_heads_;
	_u8		_fdd_spt_;	// sectors per track
	_u16		_fdd_sectors_;	// total sectors per disk
	_u8		_fdd_gap_;
	_cstr_t		_fdd_ident_;	// ident text
}_fd_device_t;

class iFDDevice:public iDriverBase, public _fd_device_t {
public:
	DECLARE_INTERFACE(I_FD_DEVICE, iFDDevice, iDriverBase);
};


typedef struct {
	_u8	_ramd_idx_;	// ram disk index
	_ulong	_ramd_size_;	// disk size in bytes
}_ram_device_t;

class iRamDevice:public iDriverBase, public _ram_device_t {
public:
	DECLARE_INTERFACE(I_RAM_DEVICE, iRamDevice, iDriverBase);
};

#endif

