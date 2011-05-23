#ifndef __VSFUSBD_MSCBOT_H_INCLUDED__
#define __VSFUSBD_MSCBOT_H_INCLUDED__

#include "tool/list/list.h"
#include "SCSI.h"

#define USBMSC_CBW_SIGNATURE			0x43425355
#define USBMSC_CSW_SIGNATURE			0x53425355

#define USBMSC_CBWFLAGS_DIR_OUT			0x00
#define USBMSC_CBWFLAGS_DIR_IN			0x80

struct USBMSC_CBW_t
{
	uint32_t dCBWSignature;
	uint32_t dCBWTag;
	uint32_t dCBWDataTransferLength;
	uint8_t bmCBWFlags;
	uint8_t bCBWLUN;
	uint8_t bCBWCBLength;
	uint8_t CBWCB[16];
};

#define USBMSC_CSW_OK					0x00
#define USBMSC_CSW_FAILED				0x01
#define USBMSC_CSW_PHASE_ERROR			0x02

struct USBMSC_CSW_t
{
	uint32_t dCSWSignature;
	uint32_t dCSWTag;
	uint32_t dCSWDataResidue;
	uint8_t dCSWStatus;
};

enum usb_MSCBOT_req_t
{
	USB_MSCBOTREQ_GET_MAX_LUN	= 0xFE,
	USB_MSCBOTREQ_RESET			= 0xFF,
};

extern const struct vsfusbd_class_protocol_t vsfusbd_MSCBOT_class;

enum vsfusbd_MSCBOT_status_t
{
	VSFUSBD_MSCBOT_STATUS_IDLE,
	VSFUSBD_MSCBOT_STATUS_OUT,
	VSFUSBD_MSCBOT_STATUS_IN,
	VSFUSBD_MSCBOT_STATUS_CSW,
	VSFUSBD_MSCBOT_STATUS_ERROR,
};

struct vsfusbd_MSCBOT_param_t
{
	uint8_t iface;
	uint8_t ep_out;
	uint8_t ep_in;
	
	uint8_t max_lun;
	struct SCSI_LUN_info_t *lun_info;
	struct SCSI_handler_t *user_handlers;
	uint8_t *mal_indexes;
	
	// tick-tock operation
	// buffer size should be the largest one of all LUNs
	struct vsf_buffer_t page_buffer[2];
	
	// no need to initialize below by user
	bool tick_tock;
	struct USBMSC_CBW_t CBW;
	struct USBMSC_CSW_t CSW;
	enum vsfusbd_MSCBOT_status_t status;
	
	struct sllist list;
};
RESULT vsfusbd_MSCBOT_set_param(struct vsfusbd_MSCBOT_param_t *param);

#endif	// __VSFUSBD_MSC_H_INCLUDED__

