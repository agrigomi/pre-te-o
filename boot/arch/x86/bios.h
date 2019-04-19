#ifndef __BIOS_H__
#define __BIOS_H__

//bios
#define BIOS_FN_READ	0x02
#define BIOS_FN_WRITE	0x03

typedef struct { // BIOS device parameters
	_u16	size;	/* The caller shall set this value to the maximum Result Buffer length, in bytes. If the length of this
				buffer is less than 30 bytes, this function shall not return the pointer to Drive Parameter Table (DPT)
				extension. If the buffer length is 30 or greater on entry, it shall be set to 30 on exit. If the buffer length
				is between 26 and 29, it shall be set to 26 on exit. If the buffer length is less than 26 on entry an error
				shall be returned.*/
	_u16 	flags;	/*Information Flags. A value of one in a bit indicates that the feature shall be available. A value of zero
				in a bit indicates the feature shall be not available and shall operate in a manner consistent with the
				conventional INT 13h interface.
				Bit 	Description
				0 	DMA boundary errors are handled transparently
				1 	The geometry returned in bytes 4-15 shall be valid
				2 	Media shall be removable. Bits 4-6 are not valid if this bit is cleared to zero
				3 	Device supports write verify
				4 	Device has media change notification
				5 	Media shall be lockable
				6 	Device geometry shall be set to maximum and no media shall be present when this bit is set to one
				7 	BIOS calls INT13h FN 50h to access the device
				8-15 	Reserved */
	_u32	cylinders; /* Number of default cylinders. The content of this field shall be one greater than the maximum
				cylinder number. INT 13h FN 08h shall be used to find the logical number of cylinders.*/
	_u32	heads;	/* Number of default heads. The content of this field shall be one greater than the maximum head
				number. INT 13h FN 08h shall be used to find the logical number of heads.*/
	_u32	spt;	/* Number of default sectors per track. The content of this field shall be the same as the maximum
				sector number because sector addresses are 1 based. INT 13h FN 08h shall be used to find the
				logical number of sectors per track.*/
	_u64	sectors; /* Number of sectors. This shall be one greater than the maximum sector number. If this field is greater
				than 15,482,880 then word 2, bit 1 shall be cleared to zero.*/
	_u16	sz_sector; /* Number of bytes in a sector.*/
	_u32	dpte;	/* Pointer to the Device Parameter Table Extension (DPTE). This field follows the seg:offset address
				format. The DPTE shall only be present if INT 13h, FN 41h, CX register bit 2 is set to one. This field
				points to a temporary buffer that the BIOS may invalidate on subsequent INT 13h calls. If the length
				of this result buffer is less than 30, the DPTE shall not be present. This field is only used for INT 13h
				based systems configured with ATA or ATAPI devices.*/
	_u16	key_dev_patch; /*0BEDDh - Key, indicates presence of Device Path Information */
	_u8	sz_dev_patch; /* Length of Device Path Information including the key. The content of this byte shall be 2Ch */
	_u8	reserved1;
	_u8	reserved2;
	_u16	reserved3;
	_s8	host_bus_type[4]; /* Host bus type, 4 bytes. ASCII data shall be left justified and padded with the value 20h
					PCI		PCI Local Bus 			50h 43h 49h 20h
					ISA		Conventional 16 bit fixed bus 	49h 53h 41h 20h
					PCIX		PCI-X Bus			50h 43h 49h 58h
					IBND		Infiniband			49h 42h 4Eh 44h
					XPRS		PCI Express			58h 50h 52h 53h
					HTPT		HyperTransport			48h 54h 50h 54h */
	_s8	interface_type[8]; /* Interface type, 8 bytes. ASCII data shall be left justified and padded with the value 20h
					ATA 	ATA/ATAPI compliant device using ATA commands 		41h 54h 41h 20h 20h 20h 20h 20h
					ATAPI	ATA/ATAPI compliant device using ATAPI commands 	41h 54h 41h 50h 49h 20h 20h 20h
					SCSI	SCSI compliant device 					53h 43h 53h 49h 20h 20h 20h 20h
					USB	USB Mass Storage compliant device			55h 53h 42h 20h 20h 20h 20h 20h
					1394	1394 Mass Storage device				31h 33h 39h 34h 20h 20h 20h 20h
					FIBRE	Fibre Channel						46h 49h 42h 52h 45h 20h 20h 20h
					I2O	Intelligent Input/Output				49h 32h 4Fh 20h 20h 20h 20h 20h
					RAID	Redundant Array of Inexpensive Disks (RAID) member	52h 41h 49h 44h 20h 20h 20h 20h
					SATA	Serial ATA						53h 41h 54h 41h 20h 20h 20h 20h
					SAS	Serial Attached SCSI					53h 41h 53h 20h 20h 20h 20h 20h */
	_u64	interface_patch; /* Interface Path, 8 bytes. */
	_u64	device_patch[2]; /* Device path 16 bytes */
	_u8	reserved4;
	_u8	check_sum;	/* Checksum for Device Path Information includes the 0BEDDh signature. The content of this field shall
					be the two’s complement of the unsigned sum of offset 30 through 72. The unsigned sum of offset
					30 through 73 shall be 0.*/
}_bios_device_parameters_t;

#ifdef __cplusplus
extern "C" {
#endif	
void bios_print_char(_char_t c);
void bios_print_byte(_u8 c);
void bios_print_word(_u16 w);
void bios_print_dword(_u32 dw);
void bios_print_qword(_u32 qw[2]);
void bios_print_hex(_u8 *ptr,_u16 sz);
void bios_print_text(_str_t p, _u16 sz);
_u16 bios_wait_key(void);
_u16 bios_get_key(void);
_u8 bios_rw_sector_conv(_u8 mode, _u32 lba, _u8 dev, _u8 *_ptr);
_u8 bios_rw_sector_ex(_u8 mode, _lba_t *p_lba, _u8 dev, _u8 count, _u8 *_ptr);
_u8 bios_rw_sector(_u8 mode, _lba_t *p_lba, _u8 dev, _u8 count, _u8 *_ptr);
void bios_display_text(_str_t _ptr, _u16 sz, _u8 row, _u8 col, _u8 color);
void bios_wait(_u32 micro_s);
void bios_set_cursor_pos(_u8 row, _u8 col);
void bios_hide_cursor(void);
void bios_clear_screen(void);
void bios_enable_a20(void);
_u16 bios_get_memory_map_e820(_u8 *buffer,_u16 sz);
_u8 bios_get_device_parameters(_u8 dev, _bios_device_parameters_t *dp);
_u32 bios_get_rtc(void);
void bios_set_rtc(_u32 tm);
#ifdef __cplusplus
};
#endif // __cplusplus


#endif
