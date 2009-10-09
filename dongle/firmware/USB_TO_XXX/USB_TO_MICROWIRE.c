/**************************************************************************
 *  Copyright (C) 2008 by Simon Qian                                      *
 *  SimonQian@SimonQian.com                                               *
 *                                                                        *
 *  Project:    Versaloon                                                 *
 *  File:       USB_TO_MICROWIRE.c                                        *
 *  Author:     SimonQian                                                 *
 *  Versaion:   See changelog                                             *
 *  Purpose:    implementation file for USB_TO_MICROWIRE                  *
 *  License:    See license                                               *
 *------------------------------------------------------------------------*
 *  Change Log:                                                           *
 *      YYYY-MM-DD:     What(by Who)                                      *
 *      2008-11-07:     created(by SimonQian)                             *
 **************************************************************************/

#include "app_cfg.h"
#if USB_TO_MICROWIRE_EN

#include "USB_TO_XXX.h"

void USB_TO_MICROWIRE_ProcessCmd(uint8* dat, uint16 len)
{
	uint16 index, device_num, length;
	uint8 command;

	index = 0;
	while(index < len)
	{
		command = dat[index] & USB_TO_XXX_CMDMASK;
		device_num = dat[index] & USB_TO_XXX_IDXMASK;
		if(device_num >= USB_TO_MICROWIRE_NUM)
		{
			buffer_out[rep_len++] = USB_TO_XXX_INVALID_INDEX;
			return;
		}
		length = dat[index + 1] + (dat[index + 2] << 8);
		index += 3;

		switch(command)
		{
		case USB_TO_XXX_INIT:
			buffer_out[rep_len++] = USB_TO_XXX_OK;
			buffer_out[rep_len++] = USB_TO_MICROWIRE_NUM;

			break;
		case USB_TO_XXX_CONFIG:
			buffer_out[rep_len++] = USB_TO_XXX_OK;
			// no need to configure in this implementation for they are already configured

			break;
		case USB_TO_XXX_FINI:
			buffer_out[rep_len++] = USB_TO_XXX_OK;

			break;
		case USB_TO_XXX_IN:
			buffer_out[rep_len++] = USB_TO_XXX_OK;

			break;
		default:
			buffer_out[rep_len++] = USB_TO_XXX_CMD_NOT_SUPPORT;

			break;
		}
		index += length;
	}
}

#endif
