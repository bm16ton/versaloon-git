/**************************************************************************
 *  Copyright (C) 2008 by Simon Qian                                      *
 *  SimonQian@SimonQian.com                                               *
 *                                                                        *
 *  Project:    Versaloon                                                 *
 *  File:       USB_TO_LPCICP.c                                           *
 *  Author:     SimonQian                                                 *
 *  Versaion:   See changelog                                             *
 *  Purpose:    implementation file for USB_TO_LPCICP                     *
 *  License:    See license                                               *
 *------------------------------------------------------------------------*
 *  Change Log:                                                           *
 *      YYYY-MM-DD:     What(by Who)                                      *
 *      2008-11-07:     created(by SimonQian)                             *
 **************************************************************************/

#include "app_cfg.h"
#if USB_TO_LPCICP_EN

#include "USB_TO_XXX.h"
#include "LPC_ICP.h"

void USB_TO_LPCICP_ProcessCmd(uint8* dat, uint16 len)
{
	uint16 index, device_num, length;
	uint8 command;

	index = 0;
	while(index < len)
	{
		command = dat[index] & USB_TO_XXX_CMDMASK;
		device_num = dat[index] & USB_TO_XXX_IDXMASK;
		if(device_num >= USB_TO_LPCICP_NUM)
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
			buffer_out[rep_len++] = USB_TO_LPCICP_NUM;

			LPCICP_Init();

			break;
		case USB_TO_XXX_CONFIG:
			buffer_out[rep_len++] = USB_TO_XXX_OK;

			break;
		case USB_TO_XXX_FINI:
			LPCICP_LeavrProgMode();
			LPCICP_Fini();
			buffer_out[rep_len++] = USB_TO_XXX_OK;

			break;
		case USB_TO_LPCICP_EnterProgMode:
			if (Vtarget > TVCC_SAMPLE_MIN_POWER)
			{
				// No power should be on the target
				buffer_out[rep_len++] = USB_TO_XXX_FAILED;
			}
			else
			{
				LPCICP_EnterProgMode();
				buffer_out[rep_len++] = USB_TO_XXX_OK;
			}

			break;
		case USB_TO_LPCICP_In:
			LPCICP_In(&dat[index], length);
			buffer_out[rep_len++] = USB_TO_XXX_OK;
			memcpy(&buffer_out[rep_len], &dat[index], length);
			rep_len += length;

			break;
		case USB_TO_LPCICP_Out:
			LPCICP_Out(&dat[index], length);
			buffer_out[rep_len++] = USB_TO_XXX_OK;

			break;
		case USB_TO_LPCICP_PollRdy:
			buffer_out[rep_len++] = USB_TO_XXX_OK;
			buffer_out[rep_len++] = LPCICP_Poll(dat[index], 		// out
												dat[index + 1], 	// setbit
												dat[index + 2],		// clearbit
												dat[index + 3] + (dat[index + 4] << 8));	// pollcnt

			break;
		default:
			buffer_out[rep_len++] = USB_TO_XXX_CMD_NOT_SUPPORT;

			break;
		}
		index += length;
	}
}

#endif
