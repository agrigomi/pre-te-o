#include "repository.h"
#include "iSystemBus.h"
#include "iSystemLog.h"
#include "ctrldef.h"
#include "iBlockDevice.h"
#include "iSerialDevice.h"
#include "addr.h"
#include "io_port.h"
#include "iCMOS.h"

typedef struct {
	_u32	ctrl_id;
	_cstr_t name;
}_controller_name_t;

typedef struct {
	_u32	ctrl_id;
	_u16	vendor_id;
	_u16	device_id;
}_vendor_device_t;

//////////// controller description //////////////////
static _vendor_device_t _g_vd_[] = {
	// controller,	vendor,		device
	// TODO
	{0,		0,		0}
};
/////////////////////////////////////////////////////
_u32 get_ctrl_id(_pci_hdr_t *p_pci_hdr) {
	//detect controller
	_u32 vd_idx = 0;

	// search controller by vendor/device ID
	while(_g_vd_[vd_idx].ctrl_id) {
		if(p_pci_hdr->vendor_id == _g_vd_[vd_idx].vendor_id &&
				p_pci_hdr->device_id == _g_vd_[vd_idx].device_id)
			break;

		vd_idx++;
	}
	
	return  _g_vd_[vd_idx].ctrl_id;
}

_u32 get_ctrl_idx(_u32 ctrl_id, _controller_name_t _ctrl_[]) {
	_u32 r = 0;

	// search by controller ID
	while(_ctrl_[r].ctrl_id) {
		if(_ctrl_[r].ctrl_id == ctrl_id)
			break;
		
		r++;
	}

	return r;
}

static _controller_name_t _g_ide_ctrl_[] = {
	// controllelr,		name
	// TODO
	{0,			IDE_CTRL_NAME_GENERIC}
};

#define LOG(_fmt_, ...) \
	if(syslog) \
		syslog->fwrite(_fmt_, __VA_ARGS__) 

bool setup_pide_ctrl(iDriverRoot *p_dr, _u8 index, _u32 pci_index, _pci_hdr_t *p_pci_hdr) {
	bool r = false;
	iBDCtrlHost *p_ctrl_host = (iBDCtrlHost *)__g_p_repository__->get_object_by_interface(I_BDCTRL_HOST, RY_STANDALONE);
		
	_u32 ctrl_id = get_ctrl_id(p_pci_hdr);
	_u32 ctrl_idx = get_ctrl_idx(ctrl_id, _g_ide_ctrl_);
	
	// register IDE controller
	iIDEController *p_drv = (iIDEController *)__g_p_repository__->get_object_by_name(_g_ide_ctrl_[ctrl_idx].name, RY_CLONE);
	if(p_drv && p_ctrl_host) {
		p_drv->_ide_base_primary_   = (p_pci_hdr->bar0 & 0xFFFFFFFC) + IDE_PRIMARY_BASE * (!p_pci_hdr->bar0); // Base
		p_drv->_ide_ctrl_primary_   = (p_pci_hdr->bar1 & 0xFFFFFFFC) + IDE_PRIMARY_CTRL * (!p_pci_hdr->bar1); // Ctrl
		p_drv->_ide_base_secondary_ = (p_pci_hdr->bar2 & 0xFFFFFFFC) + IDE_SECONDARY_BASE * (!p_pci_hdr->bar2); // Base
		p_drv->_ide_ctrl_secondary_ = (p_pci_hdr->bar3 & 0xFFFFFFFC) + IDE_SECONDARY_CTRL * (!p_pci_hdr->bar3); // Ctrl 
		p_drv->_ide_bus_master_     = (p_pci_hdr->bar4 & 0xFFFFFFFC);
		p_drv->_ide_irq_            = p_pci_hdr->interrupt_line;
		p_drv->_ide_prog_if_	    = p_pci_hdr->prog_if;
		p_drv->_ctrl_index_	    = index;
		p_drv->_ide_pci_index_	    = pci_index;

		p_dr->register_driver(p_drv, p_ctrl_host);
		r = true;
	}

	return r;
}

static _controller_name_t _g_ahci_ctrl_[] = {
	// controllelr,		name
	// TODO
	{0,			AHCI_CTRL_NAME_GENERIC}
};

bool setup_ahci_ctrl(iDriverRoot *p_dr, _u8 index, _u32 pci_index, _pci_hdr_t *p_pci_hdr) {
	bool r = false;

	iBDCtrlHost *p_ctrl_host = (iBDCtrlHost *)__g_p_repository__->get_object_by_interface(I_BDCTRL_HOST, RY_STANDALONE);

	_u32 ctrl_id = get_ctrl_id(p_pci_hdr);
	_u32 ctrl_idx = get_ctrl_idx(ctrl_id, _g_ahci_ctrl_);
	
	// register AHCI controller
	iAHCIController *p_drv = (iAHCIController *)__g_p_repository__->get_object_by_name(_g_ahci_ctrl_[ctrl_idx].name, RY_CLONE);
	if(p_drv && p_ctrl_host) {
		p_drv->_ahci_abar_	= p_to_k((_vaddr_t)p_pci_hdr->bar5);
		p_drv->_ahci_prog_if_	= p_pci_hdr->prog_if;
		p_drv->_ahci_irq_	= p_pci_hdr->interrupt_line;
		p_drv->_ahci_pci_index_	= pci_index;
		p_drv->_ctrl_index_	= index;

		p_dr->register_driver(p_drv, p_ctrl_host);
		r = true;
	}

	return r;
}

static _controller_name_t _g_usb_ctrl_[] = {
	// controllelr,		name
	// TODO
	{0,			USB_CTRL_NAME_GENERIC}
};

bool setup_usb_ctrl(iDriverRoot *p_dr, _u8 index, _u32 pci_index, _pci_hdr_t *p_pci_hdr) {
	bool r = false;

	iSDHost *p_ctrl_host = (iSDHost *)__g_p_repository__->get_object_by_interface(I_SD_HOST, RY_STANDALONE);

	_u32 ctrl_id = get_ctrl_id(p_pci_hdr);
	_u32 ctrl_idx = get_ctrl_idx(ctrl_id, _g_usb_ctrl_);
	// register USB controller
	iUSBController *p_drv = (iUSBController *)__g_p_repository__->get_object_by_name(_g_usb_ctrl_[ctrl_idx].name,RY_CLONE);
	if(p_drv && p_ctrl_host) {
		p_drv->_ctrl_index_ 	= index;
		p_drv->_usb_prog_if_ 	= p_pci_hdr->prog_if;
		p_drv->_usb_bus_index_	= pci_index;
		p_drv->_cr_ex_ 		= 0;
		p_drv->_cr_base_ 	= 0;
		p_drv->_usb_irq_	= p_pci_hdr->interrupt_line;

		switch(p_pci_hdr->prog_if) {
			case USB_PI_UHCI:
				p_drv->_cr_base_ = p_to_k((_vaddr_t)p_pci_hdr->bar4);
				break;
			case USB_PI_OHCI:
				p_drv->_cr_base_ = p_to_k((_vaddr_t)p_pci_hdr->bar0);
				break;
			case USB_PI_EHCI:
				p_drv->_cr_base_ = p_to_k((_vaddr_t)p_pci_hdr->bar0);
				break;
			case USB_PI_EXHCI:
				p_drv->_cr_base_ = p_to_k((_vaddr_t)p_pci_hdr->bar0);
				p_drv->_cr_ex_   = p_to_k((_vaddr_t)p_pci_hdr->bar1);
				break;
		}
		p_dr->register_driver(p_drv, p_ctrl_host);
		r = true;
	}
	return r;
}

void setup_pci_controllers(void) {
	iSystemLog *syslog = (iSystemLog *)__g_p_repository__->get_object_by_interface(I_SYSTEM_LOG, RY_STANDALONE);
	iDriverRoot *p_dr = (iDriverRoot *)__g_p_repository__->get_object_by_interface(I_DRIVER_ROOT, RY_STANDALONE);
	if(p_dr) {
		_driver_request_t dr;
		dr.flags = DRF_INTERFACE;
		dr.DRIVER_INTERFACE = I_PCI;
		
		iPCI *p_pci = (iPCI *)p_dr->driver_request(&dr);
		if(p_pci) {
			_u32 pci_dev_cnt = p_pci->count();
			_u8 index = 0;
			
			for(_u32 i = 0; i < pci_dev_cnt; i++) {
				_pci_hdr_t pci_dev;
				if(p_pci->device(i, &pci_dev)) {
					if(pci_dev.class_code == PCI_CLASS_STORAGE) {
						switch(pci_dev.subclass) {
							case 0: LOG("SCSI bus controller (PIF:0x%x,IRQ:0x%x)\r\n",
									pci_dev.prog_if,pci_dev.interrupt_line);
								break;
							case 1: LOG("IDE controller (PIF:0x%x,IRQ:0x%x)\r\n",
									pci_dev.prog_if,pci_dev.interrupt_line);
								if(pci_dev.prog_if == 0x80 || pci_dev.prog_if == 0x8a) 
									index += (setup_pide_ctrl(p_dr, index, i, &pci_dev))?1:0;
								break;
							case 2: LOG("Floppy Disk Controller (PIF:0x%x,IRQ:0x%x)\r\n",
									pci_dev.prog_if,pci_dev.interrupt_line);
								break;
							case 3: LOG("IPI Bus Controller (PIF:0x%x,IRQ:0x%x)\r\n",
									pci_dev.prog_if,pci_dev.interrupt_line);
								break;
							case 4: LOG("RAID Controller (PIF:0x%x,IRQ:0x%x)\r\n",
									pci_dev.prog_if,pci_dev.interrupt_line);
								break;
							case 5: LOG("ATA controller (PIF:0x%x,IRQ:0x%x)\r\n",
									pci_dev.prog_if,pci_dev.interrupt_line);
								break;
							case 6: LOG("SATA (AHCI) controller (PIF:0x%x,IRQ:0x%x,ABAR:0x%h)\r\n",
									pci_dev.prog_if,pci_dev.interrupt_line, 
									p_to_k(pci_dev.bar5));
								if(pci_dev.prog_if == 1)
									index += (setup_ahci_ctrl(p_dr, index, i, &pci_dev))?1:0;
								break;
							case 7: LOG("SCSI (SAS) controller (PIF:0x%x,IRQ:0x%x)\r\n",
									pci_dev.prog_if,pci_dev.interrupt_line);
								break;
							case 8: // Other Mass Storage Controller
								break;
						}
					} else if(pci_dev.class_code == PCI_CLASS_SBUS) {
						switch(pci_dev.subclass) {
							case 3: LOG("USB host controller (PIF:0x%x,IRQ:0x%x)\r\n",
									pci_dev.prog_if, 
									pci_dev.interrupt_line); 
								switch(pci_dev.prog_if) {
									case 0x00:
									case 0x10:
									case 0x20:
									case 0x30:
										index += (setup_usb_ctrl(p_dr, 
												index, i, &pci_dev)
											)?1:0;
										break;
								}
								break;
						}
					}
				}
			}
		}
		
		__g_p_repository__->release_object(p_dr);
	}
}

void setup_fd_controller(void) {
	iBDCtrlHost *p_ctrl_host = (iBDCtrlHost *)__g_p_repository__->get_object_by_interface(I_BDCTRL_HOST, RY_STANDALONE);

	iCMOS *p_cmos = (iCMOS *)__g_p_repository__->get_object_by_interface(I_CMOS, RY_STANDALONE);
	if(p_cmos) {
		_u8 fdd_ident = p_cmos->read(FDC_CMOS_REG);
		if(fdd_ident) {
			iDriverRoot *p_dr = 
				(iDriverRoot *)__g_p_repository__->get_object_by_interface(I_DRIVER_ROOT, RY_STANDALONE);
			if(p_dr) {
				iFDController *p_fd_ctrl = 
					(iFDController *)__g_p_repository__->get_object_by_interface(I_FD_CONTROLLER, RY_CLONE);
				if(p_fd_ctrl) {
					p_fd_ctrl->_fdc_base_ = FDC_BASE;
					p_fd_ctrl->_fdc_detect_ = fdd_ident;
					p_dr->register_driver(p_fd_ctrl, p_ctrl_host);
				}
			}
		}
	}
}
