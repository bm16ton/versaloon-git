#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "compiler.h"
#include "app_cfg.h"

#include "interfaces.h"
#include "dal/mal/mal.h"
#include "tool/fakefat32/fakefat32.h"

#include "stack/usb_device/vsf_usbd.h"
#include "stack/usb_device/class/MSC/vsfusbd_MSC_BOT.h"

static const char readme_str[] = 
"\
================================================================================\r\n\
VSF MSCBoot BootLoaddr 0.1beta          COPYRIGHT by SimonQian                  \r\n\
www.versaloon.com                                                               \r\n\
================================================================================\r\n\
\r\n\
Simply update firmware by copying new flash.bin to the root directory.\r\n\
LOST.DIR directory is used to avoid Android systems creating directory,\r\n\
which is not supported by MSCBoot.\r\n\
\r\n\
By default, the bootloader will take 32KB from the start address of flash.\r\n\
So, your application should start after the bootloader, and flash.bin should \r\n\
not exceed flash space of the target chip.\r\n\
\r\n\
Note: For VersaloonMini shipped with STM32F103CBT6, this bootloader is \r\n\
recommended. You can find the bootloader binary under:\r\n\
\trelease/firmware/MiniRelease1/NFW\r\n\
\r\n\
Report to author of Versaloon if any problem when using this bootloader.\r\n\
";

// FAKEFAT32
static vsf_err_t WriteFlash(struct fakefat32_file_t*file, uint32_t addr,
									uint8_t *buff, uint32_t page_size);
static vsf_err_t WriteFlash_isready(struct fakefat32_file_t*file, uint32_t addr,
									uint8_t *buff, uint32_t page_size);
static vsf_err_t ReadFlash(struct fakefat32_file_t*file, uint32_t addr,
									uint8_t *buff, uint32_t page_size);
static vsf_err_t ReadFlash_isready(struct fakefat32_file_t*file, uint32_t addr,
									uint8_t *buff, uint32_t page_size);
static vsf_err_t ReadInfo(struct fakefat32_file_t*file, uint32_t addr,
									uint8_t *buff, uint32_t page_size);
static struct fakefat32_file_t lost_dir[] =
{
	{
		".", NULL,
		FAKEFAT32_FILEATTR_DIRECTORY,
	},
	{
		"..", NULL,
		FAKEFAT32_FILEATTR_DIRECTORY,
	},
	{
		NULL,
	}
};
static struct fakefat32_file_t root_dir[] =
{
	{
		"MSCBoot", NULL,
		FKAEFAT32_FILEATTR_VOLUMEID,
	},
	{
		"LOST", "DIR",
		FAKEFAT32_FILEATTR_DIRECTORY,
		0,
		{fakefat32_dir_read, NULL, fakefat32_dir_write, NULL},
		lost_dir
	},
	{
		"flash", "bin",
		FAKEFAT32_FILEATTR_ARCHIVE,
		512,
		{ReadFlash, ReadFlash_isready, WriteFlash, WriteFlash_isready},
	},
	{
		"readme", "txt",
		FAKEFAT32_FILEATTR_ARCHIVE | FAKEFAT32_FILEATTR_READONLY,
		sizeof(readme_str) - 1,
		{ReadInfo, NULL, NULL, NULL},
	},
	{
		NULL,
	}
};
static struct fakefat32_param_t fakefat32_param =
{
	512,			// uint16_t sector_size;
	0x00760000,		// uint32_t sector_number;
	8,				// uint8_t sectors_per_cluster;
	
	0x0CA93E47,		// uint32_t volume_id;
	0x12345678,		// uint32_t disk_id;
	{				// struct fakefat32_file_t root;
		{
			"ROOT", NULL,
			0,
			0,
			{fakefat32_dir_read, NULL, fakefat32_dir_write, NULL},
			root_dir
		}
	}
};
static struct mal_info_t fakefat32_mal_info = 
{
	{0, 0}, NULL, 0, 0, 0, &fakefat32_drv
};
static struct dal_info_t fakefat32_dal_info = 
{
	NULL,
	&fakefat32_param,
	NULL,
	&fakefat32_mal_info,
};

static vsf_err_t ReadInfo(struct fakefat32_file_t*file, uint32_t addr,
									uint8_t *buff, uint32_t page_size)
{
	uint32_t remain_size = sizeof(readme_str) - 1 - addr;
	
	memcpy(buff, &readme_str[addr], min(remain_size, page_size));
	return VSFERR_NONE;
}

enum flash_io_fsm_t
{
	FLASH_IO_IDLE,
	FLASH_IO_ERASE,
	FLASH_IO_WRITE,
} flash_io_fsm = FLASH_IO_IDLE;
static vsf_err_t WriteFlash(struct fakefat32_file_t* file, uint32_t addr,
									uint8_t *buff, uint32_t page_size)
{
	uint32_t pagesize, pagenum;
	
	interfaces->flash.unlock(0);
	interfaces->flash.getcapacity(0, &pagesize, &pagenum);
	if (addr > (pagenum - 1) * pagesize)
	{
		// location not valid, ignore
		return VSFERR_NONE;
	}
	
	if (!(addr % pagesize))
	{
		interfaces->flash.erasepage(0, addr + APP_CFG_BOOTSIZE);
		flash_io_fsm = FLASH_IO_ERASE;
	}
	else
	{
		interfaces->flash.write(0, addr + APP_CFG_BOOTSIZE, buff, page_size);
		flash_io_fsm = FLASH_IO_WRITE;
	}
	return VSFERR_NONE;
}

static vsf_err_t WriteFlash_isready(struct fakefat32_file_t* file, uint32_t addr,
									uint8_t *buff, uint32_t page_size)
{
	vsf_err_t ret;
	
	switch (flash_io_fsm)
	{
	case FLASH_IO_ERASE:
		ret = interfaces->flash.erasepage_isready(0, addr + APP_CFG_BOOTSIZE);
		if (ret)
		{
			return ret;
		}
		else
		{
			interfaces->flash.write(0, addr + APP_CFG_BOOTSIZE, buff, page_size);
			flash_io_fsm = FLASH_IO_WRITE;
		}
	case FLASH_IO_WRITE:
		ret = interfaces->flash.write_isready(0, addr + APP_CFG_BOOTSIZE, buff, page_size);
		if (ret)
		{
			return ret;
		}
		else
		{
			interfaces->flash.lock(0);
			flash_io_fsm = FLASH_IO_IDLE;
		}
	default:
		return VSFERR_NONE;
	}
}

static vsf_err_t ReadFlash(struct fakefat32_file_t* file, uint32_t addr,
									uint8_t *buff, uint32_t page_size)
{
#if APP_CFG_MSC_WRITEONLY
	memset(buff, 0xFF, page_size);
	return VSFERR_NONE;
#else
	uint32_t pagesize, pagenum;
	
	interfaces->flash.getcapacity(0, &pagesize, &pagenum);
	if (addr > (pagenum - 1) * pagesize)
	{
		// location not valid, ignore
		return VSFERR_NONE;
	}
	return interfaces->flash.read(0, addr + APP_CFG_BOOTSIZE, buff, page_size);
#endif
}

static vsf_err_t ReadFlash_isready(struct fakefat32_file_t* file, uint32_t addr,
									uint8_t *buff, uint32_t page_size)
{
#if APP_CFG_MSC_WRITEONLY
	return VSFERR_NONE;
#else
	uint32_t pagesize, pagenum;
	
	interfaces->flash.getcapacity(0, &pagesize, &pagenum);
	if (addr > (pagenum - 1) * pagesize)
	{
		// location not valid, ignore
		return VSFERR_NONE;
	}
	return interfaces->flash.read_isready(0, addr + APP_CFG_BOOTSIZE, buff, page_size);
#endif
}

// USB
static const uint8_t MSCBOT_DeviceDescriptor[] =
{
	0x12,	// bLength
	USB_DESC_TYPE_DEVICE,	 // bDescriptorType
	0x00,
	0x02,	// bcdUSB = 2.00
	0, 0, 0,
	0x40,	// bMaxPacketSize0
	0x83,
	0x04,	// idVendor = 0x0483
	0x20,
	0x57,	// idProduct = 0x5720
	0x00,
	0x01,	// bcdDevice = 1.00
	1,		// Index of string descriptor describing manufacturer
	2,		// Index of string descriptor describing product
	3,		// Index of string descriptor describing the device's serial number
	0x01	// bNumConfigurations
};

static const uint8_t MSCBOT_ConfigDescriptor[] =
{
	// Configuation Descriptor
	0x09,	// bLength: Configuation Descriptor size
	USB_DESC_TYPE_CONFIGURATION,
			// bDescriptorType: Configuration
	32,		// wTotalLength:no of returned bytes
	0x00,
	0x01,	// bNumInterfaces: 1 interface
	0x01,	// bConfigurationValue: Configuration value
	0x00,	// iConfiguration: Index of string descriptor describing the configuration
	0x80,	// bmAttributes: bus powered
	0x64,	// MaxPower 200 mA

	// Interface Descriptor
	0x09,	// bLength: Interface Descriptor size
	0x04,	// bDescriptorType:
	0x00,	// bInterfaceNumber: Number of Interface
	0x00,	// bAlternateSetting: Alternate setting
	0x02,	// bNumEndpoints
	0x08,	// bInterfaceClass: MASS STORAGE Class
	0x06,	// bInterfaceSubClass : SCSI transparent
	0x50,	// nInterfaceProtocol
	0x00,	// iInterface:

	// Endpoint 1 Descriptor
	0x07,	// Endpoint descriptor length = 7
	0x05,	// Endpoint descriptor type
	0x81,	// Endpoint address (IN, address 1)
	0x02,	// Bulk endpoint type
	0x40,	// Maximum packet size (64 bytes)
	0x00,
	0x00,	// Polling interval in milliseconds

	// Endpoint 1 Descriptor
	0x07,	// Endpoint descriptor length = 7
	0x05,	// Endpoint descriptor type
	0x01,	// Endpoint address (OUT, address 1)
	0x02,	// Bulk endpoint type
	0x40,	// Maximum packet size (64 bytes)
	0x00,
	0x00,	// Polling interval in milliseconds
};

static const uint8_t MSCBOT_StringLangID[] =
{
	4,
	USB_DESC_TYPE_STRING,
	0x09,
	0x04
};

static const uint8_t MSCBOT_StringVendor[] =
{
	38,
	USB_DESC_TYPE_STRING,
	'S', 0, 'T', 0, 'M', 0, 'i', 0, 'c', 0, 'r', 0, 'o', 0, 'e', 0,
	'l', 0, 'e', 0, 'c', 0, 't', 0, 'r', 0, 'o', 0, 'n', 0, 'i', 0,
	'c', 0, 's', 0
};

static const uint8_t MSCBOT_StringProduct[] =
{
	16,
	USB_DESC_TYPE_STRING,
	'M', 0, 'S', 0, 'C', 0, 'B', 0, 'o', 0, 'o', 0, 't', 0
};

static const uint8_t MSCBOT_StringSerial[50] =
{
	50,
	USB_DESC_TYPE_STRING,
	'0', 0, '1', 0, '2', 0, '3', 0, '4', 0, '5', 0, '6', 0, '7', 0, 
	'8', 0, '9', 0, 'A', 0, 'B', 0, 'C', 0, 'D', 0, 'E', 0, 'F', 0, 
	'0', 0, '0', 0, '0', 0, '0', 0, '0', 0, '0', 0, '0', 0, '0', 0, 
};

static const struct vsfusbd_desc_filter_t descriptors[] = 
{
	VSFUSBD_DESC_DEVICE(0, MSCBOT_DeviceDescriptor, sizeof(MSCBOT_DeviceDescriptor), NULL),
	VSFUSBD_DESC_CONFIG(0, 0, MSCBOT_ConfigDescriptor, sizeof(MSCBOT_ConfigDescriptor), NULL),
	VSFUSBD_DESC_STRING(0, 0, MSCBOT_StringLangID, sizeof(MSCBOT_StringLangID), NULL),
	VSFUSBD_DESC_STRING(0x0409, 1, MSCBOT_StringVendor, sizeof(MSCBOT_StringVendor), NULL),
	VSFUSBD_DESC_STRING(0x0409, 2, MSCBOT_StringProduct, sizeof(MSCBOT_StringProduct), NULL),
	VSFUSBD_DESC_STRING(0x0409, 3, MSCBOT_StringSerial, sizeof(MSCBOT_StringSerial), NULL),
	VSFUSBD_DESC_NULL
};

struct SCSI_LUN_info_t MSCBOT_LunInfo = 
{
	&fakefat32_dal_info, 
	{
		true,
		{'S', 'i', 'm', 'o', 'n', ' ', ' ', ' '},
		{'M', 'S', 'C', 'B', 'o', 'o', 't', ' ', 
		' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
		{'1', '.', '0', '0'},
		SCSI_PDT_DIRECT_ACCESS_BLOCK
	}
};
uint8_t MSCBOT_Buffer0[2048], MSCBOT_Buffer1[2048];

struct vsfusbd_MSCBOT_param_t MSCBOT_param = 
{
	1,							// uint8_t ep_out;
	1,							// uint8_t ep_in;
	
	0,							// uint8_t max_lun;
	&MSCBOT_LunInfo,			// struct SCSI_LUN_info_t *lun_info;
	NULL, 						// struct SCSI_handler_t *user_handlers;
	
	{
		{MSCBOT_Buffer0, sizeof(MSCBOT_Buffer0)},
		{MSCBOT_Buffer1, sizeof(MSCBOT_Buffer1)}
	},							// struct vsf_buffer_t page_buffer[2];
};

static struct vsfusbd_iface_t ifaces[] = 
{
	{(struct vsfusbd_class_protocol_t *)&vsfusbd_MSCBOT_class, (void *)&MSCBOT_param},
	{(struct vsfusbd_class_protocol_t *)NULL, (void *)NULL}
};
static struct vsfusbd_config_t config0[] = 
{
	{
		NULL, NULL, dimof(ifaces), (struct vsfusbd_iface_t *)ifaces,
	}
};
struct vsfusbd_device_t usb_device = 
{
	1, (struct vsfusbd_config_t *)config0, 
	(struct vsfusbd_desc_filter_t *)descriptors, 0, 
	(struct interface_usbd_t *)&core_interfaces.usbd,
	
	{
		NULL,			// init
		NULL,			// fini
		NULL,			// poll
		NULL,			// on_set_interface
		
		NULL,			// on_ATTACH
		NULL,			// on_DETACH
		NULL,			// on_RESET
		NULL,			// on_ERROR
		NULL,			// on_WAKEUP
		NULL,			// on_SUSPEND
		NULL,			// on_RESUME
		NULL,			// on_SOF
		
		NULL,			// on_IN
		NULL,			// on_OUT
	},			// callback
};

static uint32_t MSP, RST_VECT;
int main(void)
{
	uint32_t pagesize, pagenum;
	
	interfaces->core.init(NULL);
	
	// read MSP and RST_VECT
	interfaces->flash.init(0);
	interfaces->flash.read(0, APP_CFG_BOOTSIZE, (uint8_t *)&MSP, sizeof(MSP));
	while (VSFERR_NONE != interfaces->flash.read_isready(0, APP_CFG_BOOTSIZE, (uint8_t *)&MSP, sizeof(MSP)));
	interfaces->flash.read(0, APP_CFG_BOOTSIZE + 4, (uint8_t *)&RST_VECT, sizeof(RST_VECT));
	while (VSFERR_NONE != interfaces->flash.read_isready(0, APP_CFG_BOOTSIZE + 4, (uint8_t *)&RST_VECT, sizeof(RST_VECT)));
	
	interfaces->gpio.init(KEY_PORT);
	interfaces->gpio.config_pin(KEY_PORT, KEY_PIN, GPIO_INPU);
	if ((interfaces->gpio.get(KEY_PORT, 1 << KEY_PIN)) &&
		((MSP & 0xFF000000) == 0x20000000) &&
		((RST_VECT & 0xFF000000) == 0x08000000))
	{
		__set_MSP(MSP);
		((void (*)(void))RST_VECT)();
		while (1);
	}
	
	interfaces->gpio.init(USB_PULL_PORT);
	// Disable USB Pull-up
	interfaces->gpio.clear(USB_PULL_PORT, 1 << USB_PULL_PIN);
	interfaces->gpio.config_pin(USB_PULL_PORT, USB_PULL_PIN, GPIO_OUTPP);
	// delay
	interfaces->delay.delayms(200);
	
	interfaces->flash.getcapacity(0, &pagesize, &pagenum);
	
	fakefat32_param.sector_size = pagesize;
	fakefat32_param.sector_number = 128 * 1024 * 1024 / pagesize;
	fakefat32_param.sectors_per_cluster = 1;
	root_dir[2].size = pagesize * pagenum - APP_CFG_BOOTSIZE;
	mal.init(&fakefat32_dal_info);
	
	// Enable USB Pull-up
	interfaces->gpio.set(USB_PULL_PORT, 1 << USB_PULL_PIN);
	
	if (!vsfusbd_device_init(&usb_device))
	{
		while (1)
		{
			if (vsfusbd_device_poll(&usb_device))
			{
				break;
			}
		}
	}
	
	return 0;
}
