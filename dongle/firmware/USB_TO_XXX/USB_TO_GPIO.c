/**************************************************************************
 *  Copyright (C) 2008 - 2010 by Simon Qian                               *
 *  SimonQian@SimonQian.com                                               *
 *                                                                        *
 *  Project:    Versaloon                                                 *
 *  File:       USB_TO_GPIO.c                                             *
 *  Author:     SimonQian                                                 *
 *  Versaion:   See changelog                                             *
 *  Purpose:    implementation file for USB_TO_GPIO                       *
 *  License:    See license                                               *
 *------------------------------------------------------------------------*
 *  Change Log:                                                           *
 *      YYYY-MM-DD:     What(by Who)                                      *
 *      2008-11-07:     created(by SimonQian)                             *
 **************************************************************************/

#include "app_cfg.h"
#if USB_TO_GPIO_EN

#include "USB_TO_XXX.h"
#include "interfaces.h"

void USB_TO_GPIO_ProcessCmd(uint8* dat, uint16 len)
{
	uint16 index, device_idx, length;
	uint8 command;

	uint16 port_data, mask_data, io_data;

	index = 0;
	while(index < len)
	{
		command = dat[index] & USB_TO_XXX_CMDMASK;
		device_idx = dat[index] & USB_TO_XXX_IDXMASK;
		length = GET_LE_U16(&dat[index + 1]);
		index += 3;

		switch(command)
		{
		case USB_TO_XXX_INIT:
			if (ERROR_OK == interfaces->gpio.init(device_idx))
			{
				buffer_reply[rep_len++] = USB_TO_XXX_OK;
			}
			else
			{
				buffer_reply[rep_len++] = USB_TO_XXX_FAILED;
			}

			break;
		case USB_TO_XXX_CONFIG:
			mask_data = GET_LE_U16(&dat[index + 0]);
			io_data   = GET_LE_U16(&dat[index + 2]);
			port_data = GET_LE_U16(&dat[index + 4]);
			io_data  &= mask_data;

			if (ERROR_OK == 
					interfaces->gpio.config(device_idx, mask_data, io_data, port_data))
			{
				buffer_reply[rep_len++] = USB_TO_XXX_OK;
			}
			else
			{
				buffer_reply[rep_len++] = USB_TO_XXX_FAILED;
			}

			break;
		case USB_TO_XXX_FINI:
			if (ERROR_OK == interfaces->gpio.fini(device_idx))
			{
				buffer_reply[rep_len++] = USB_TO_XXX_OK;
			}
			else
			{
				buffer_reply[rep_len++] = USB_TO_XXX_FAILED;
			}

			break;
		case USB_TO_XXX_IN:
			mask_data = GET_LE_U16(&dat[index]);

			if (ERROR_OK == 
					interfaces->gpio.in(device_idx, mask_data, &port_data))
			{
				buffer_reply[rep_len] = USB_TO_XXX_OK;
				SET_LE_U16(&buffer_reply[rep_len + 1], port_data);
			}
			else
			{
				buffer_reply[rep_len] = USB_TO_XXX_FAILED;
			}
			rep_len += 3;

			break;
		case USB_TO_XXX_OUT:
			mask_data = GET_LE_U16(&dat[index + 0]);
			port_data = GET_LE_U16(&dat[index + 2]);

			if (ERROR_OK == 
					interfaces->gpio.out(device_idx, mask_data, port_data))
			{
				buffer_reply[rep_len++] = USB_TO_XXX_OK;
			}
			else
			{
				buffer_reply[rep_len++] = USB_TO_XXX_FAILED;
			}

			break;
		default:
			buffer_reply[rep_len++] = USB_TO_XXX_CMD_NOT_SUPPORT;

			break;
		}
		index += length;
	}
}

#endif
