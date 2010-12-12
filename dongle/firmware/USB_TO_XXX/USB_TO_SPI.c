/**************************************************************************
 *  Copyright (C) 2008 - 2010 by Simon Qian                               *
 *  SimonQian@SimonQian.com                                               *
 *                                                                        *
 *  Project:    Versaloon                                                 *
 *  File:       USB_TO_SPI.c                                              *
 *  Author:     SimonQian                                                 *
 *  Versaion:   See changelog                                             *
 *  Purpose:    implementation file for USB_TO_SPI                        *
 *  License:    See license                                               *
 *------------------------------------------------------------------------*
 *  Change Log:                                                           *
 *      YYYY-MM-DD:     What(by Who)                                      *
 *      2008-11-07:     created(by SimonQian)                             *
 **************************************************************************/

#include "app_cfg.h"
#if USB_TO_SPI_EN

#include "USB_TO_XXX.h"
#include "interfaces.h"

void USB_TO_SPI_ProcessCmd(uint8* dat, uint16 len)
{
	uint16 index, device_idx, length;
	uint8 command;
	
	uint8 attr;
	uint16 frequency;
	
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
			if (ERROR_OK == interfaces->spi.init(device_idx))
			{
				buffer_reply[rep_len++] = USB_TO_XXX_OK;
			}
			else
			{
				buffer_reply[rep_len++] = USB_TO_XXX_FAILED;
			}
			break;
		case USB_TO_XXX_CONFIG:
			attr = dat[index];
			frequency = GET_LE_U16(&dat[index + 1]);
			index += 3;
			
			if (ERROR_OK == interfaces->spi.config(device_idx, frequency, 
											attr & USB_TO_SPI_CPOL_MASK, 
											attr & USB_TO_SPI_CPHA_MASK, 
											attr & USB_TO_SPI_FIRSTBIT_MASK))
			{
				buffer_reply[rep_len++] = USB_TO_XXX_OK;
			}
			else
			{
				buffer_reply[rep_len++] = USB_TO_XXX_FAILED;
			}
			break;
		case USB_TO_XXX_FINI:
			if (ERROR_OK == interfaces->spi.fini(device_idx))
			{
				buffer_reply[rep_len++] = USB_TO_XXX_OK;
			}
			else
			{
				buffer_reply[rep_len++] = USB_TO_XXX_FAILED;
			}
			break;
		case USB_TO_XXX_IN_OUT:
			if (ERROR_OK == interfaces->spi.io(device_idx, &dat[index], 
								&buffer_reply[rep_len + 1], length))
			{
				buffer_reply[rep_len] = USB_TO_XXX_OK;
			}
			else
			{
				buffer_reply[rep_len] = USB_TO_XXX_FAILED;
			}
			rep_len += 1 + length;
			break;
		default:
			buffer_reply[rep_len++] = USB_TO_XXX_CMD_NOT_SUPPORT;
			break;
		}
		index += length;
	}
}

#endif
