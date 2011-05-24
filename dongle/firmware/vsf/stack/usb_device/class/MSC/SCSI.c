#include "app_cfg.h"

#include "SCSI.h"

static enum SCSI_errcode_t SCSI_errcode = SCSI_ERRCODE_OK;

RESULT SCSI_handler_INQUIRY(struct SCSI_LUN_info_t *info, uint8_t CB[16], 
		struct vsf_buffer_t *buffer, uint32_t *page_size, uint32_t *page_num)
{
	uint8_t *pbuffer = buffer->buffer;
	
	if (CB[1] & 1)
	{
		// When the EVPD bit is set to one, 
		// the PAGE CODE field specifies which page of 
		// vital product data information the device server shall return
		if (0 == CB[2])
		{
			// 0x00: Supported VPD Pages
			memset(pbuffer, 0, 5);
			buffer->size = 5;
			*page_size = 5;
			*page_num = 1;
		}
	}
	else
	{
		if (CB[2] != 0)
		{
			// If the PAGE CODE field is not set to zero 
			// when the EVPD bit is set to zero, 
			// the command shall be terminated with CHECK CONDITION status, 
			// with the sense key set to ILLEGAL REQUEST, 
			// and the additional sense code set to INVALID FIELD IN CDB.
			info->status.sense_key = SCSI_SENSEKEY_ILLEGAL_REQUEST;
			info->status.asc = SCSI_ASC_INVALID_FIELED_IN_COMMAND;
			SCSI_errcode = SCSI_ERRCODE_INVALID_PARAM;
			return ERROR_FAIL;
		}
		// If the EVPD bit is set to zero, 
		// the device server shall return the standard INQUIRY data.
		memset(pbuffer, 0, 36);
		if (info->param.removable)
		{
			pbuffer[1] = 0x80;
		}
		pbuffer[4] = 32;
		pbuffer += 8;
		memcpy(pbuffer, info->param.vendor, sizeof(info->param.vendor));
		pbuffer += sizeof(info->param.vendor);
		memcpy(pbuffer, info->param.product, sizeof(info->param.product));
		pbuffer += sizeof(info->param.product);
		memcpy(pbuffer, info->param.revision, sizeof(info->param.revision));
		buffer->size = 36;
		*page_size = 36;
		*page_num = 1;
	}
	
	return ERROR_OK;
}

RESULT SCSI_handler_READ_FORMAT_CAPACITIES(struct SCSI_LUN_info_t *info, 
		uint8_t CB[16], struct vsf_buffer_t *buffer, uint32_t *page_size, 
		uint32_t *page_num)
{
	info->status.sense_key = SCSI_SENSEKEY_NOT_READY;
	info->status.asc = SCSI_ASC_MEDIUM_NOT_PRESENT;
	SCSI_errcode = SCSI_ERRCODE_FAIL;
	return ERROR_FAIL;
}

RESULT SCSI_handler_READ_CAPACITY10(struct SCSI_LUN_info_t *info, 
		uint8_t CB[16], struct vsf_buffer_t *buffer, uint32_t *page_size, 
		uint32_t *page_num)
{
	info->status.sense_key = SCSI_SENSEKEY_NOT_READY;
	info->status.asc = SCSI_ASC_MEDIUM_NOT_PRESENT;
	SCSI_errcode = SCSI_ERRCODE_FAIL;
	return ERROR_FAIL;
}

static struct SCSI_handler_t SCSI_handlers[] = 
{
	{
		SCSI_CMD_INQUIRY,
		SCSI_handler_INQUIRY,
		NULL
	},
	{
		SCSI_CMD_READ_FORMAT_CAPACITIES,
		SCSI_handler_READ_FORMAT_CAPACITIES,
		NULL
	},
	{
		SCSI_CMD_READ_CAPACITY10,
		SCSI_handler_READ_CAPACITY10,
		NULL
	},
	SCSI_HANDLER_NULL
};

static struct SCSI_handler_t* SCSI_get_handler(struct SCSI_handler_t *handlers, 
												uint8_t operation_code)
{
	while (handlers->handler != NULL)
	{
		if (handlers->operation_code == operation_code)
		{
			return handlers;
		}
		handlers++;
	}
	return NULL;
}

RESULT SCSI_Handle(struct SCSI_handler_t *handlers, 
		struct SCSI_LUN_info_t *info, uint8_t CB[16], 
		struct vsf_buffer_t *buffer, uint32_t *page_size, uint32_t *page_num)
{
	if (NULL == handlers)
	{
		handlers = SCSI_handlers;
	}
	handlers = SCSI_get_handler(handlers, CB[0]);
	if (NULL == handlers)
	{
		SCSI_errcode = SCSI_ERRCODE_INVALID_COMMAND;
		return ERROR_FAIL;
	}
	
	SCSI_errcode = SCSI_ERRCODE_OK;
	return handlers->handler(info, CB, buffer, page_size, page_num);
}

RESULT SCSI_IO(struct SCSI_handler_t *handlers, 
		struct SCSI_LUN_info_t *info, uint8_t CB[16], 
		struct vsf_buffer_t *buffer, uint32_t cur_page)
{
	if (NULL == handlers)
	{
		handlers = SCSI_handlers;
	}
	handlers = SCSI_get_handler(handlers, CB[0]);
	if ((NULL == handlers) || (NULL == handlers->io))
	{
		SCSI_errcode = SCSI_ERRCODE_INVALID_COMMAND;
		return ERROR_FAIL;
	}
	
	SCSI_errcode = SCSI_ERRCODE_OK;
	return handlers->io(info, CB, buffer, cur_page);
}

enum SCSI_errcode_t SCSI_GetErrorCode(void)
{
	return SCSI_errcode;
}
