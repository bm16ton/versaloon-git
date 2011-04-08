/***************************************************************************
 *   Copyright (C) 2009 - 2010 by Simon Qian <SimonQian@SimonQian.com>     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "../versaloon_include.h"
#include "../versaloon.h"
#include "../versaloon_internal.h"
#include "usbtoxxx.h"
#include "usbtoxxx_internal.h"

RESULT usbtoswim_init(uint8_t interface_index)
{
	return usbtoxxx_init_command(USB_TO_SWIM, interface_index);
}

RESULT usbtoswim_fini(uint8_t interface_index)
{
	return usbtoxxx_fini_command(USB_TO_SWIM, interface_index);
}

RESULT usbtoswim_config(uint8_t interface_index, uint8_t mHz, uint8_t cnt0, 
						uint8_t cnt1)
{
	uint8_t buff[3];
	
#if PARAM_CHECK
	if (interface_index > 7)
	{
		LOG_BUG(ERRMSG_INVALID_INTERFACE_NUM, interface_index);
		return ERROR_FAIL;
	}
#endif
	
	buff[0] = mHz;
	buff[1] = cnt0;
	buff[2] = cnt1;
	
	return usbtoxxx_conf_command(USB_TO_SWIM, interface_index, buff, 3);
}

RESULT usbtoswim_srst(uint8_t interface_index)
{
#if PARAM_CHECK
	if (interface_index > 7)
	{
		LOG_BUG(ERRMSG_INVALID_INTERFACE_NUM, interface_index);
		return ERROR_FAIL;
	}
#endif
	
	return usbtoxxx_reset_command(USB_TO_SWIM, interface_index, NULL, 0);
}

RESULT usbtoswim_wotf(uint8_t interface_index, uint8_t *data, uint16_t bytelen, 
						uint32_t addr)
{
#if PARAM_CHECK
	if (interface_index > 7)
	{
		LOG_BUG(ERRMSG_INVALID_INTERFACE_NUM, interface_index);
		return ERROR_FAIL;
	}
#endif
	
	SET_LE_U16(&versaloon_cmd_buf[0], bytelen);
	SET_LE_U32(&versaloon_cmd_buf[2], addr);
	memcpy(&versaloon_cmd_buf[6], data, bytelen);
	
	return usbtoxxx_out_command(USB_TO_SWIM, interface_index, 
									versaloon_cmd_buf, bytelen + 6, 0);
}

RESULT usbtoswim_rotf(uint8_t interface_index, uint8_t *data, uint16_t bytelen, 
						uint32_t addr)
{
#if PARAM_CHECK
	if (interface_index > 7)
	{
		LOG_BUG(ERRMSG_INVALID_INTERFACE_NUM, interface_index);
		return ERROR_FAIL;
	}
#endif
	
	SET_LE_U16(&versaloon_cmd_buf[0], bytelen);
	SET_LE_U32(&versaloon_cmd_buf[2], addr);
	memset(&versaloon_cmd_buf[6], 0, bytelen);
	
	return usbtoxxx_in_command(USB_TO_SWIM, interface_index, 
				versaloon_cmd_buf, bytelen + 6, bytelen, data, 0, bytelen, 0);
}

RESULT usbtoswim_sync(uint8_t interface_index, uint8_t mHz)
{
	uint8_t buff[1];
	
#if PARAM_CHECK
	if (interface_index > 7)
	{
		LOG_BUG(ERRMSG_INVALID_INTERFACE_NUM, interface_index);
		return ERROR_FAIL;
	}
#endif
	
	buff[0] = mHz;
	
	return usbtoxxx_sync_command(USB_TO_SWIM, interface_index, buff, 1, 
									0, NULL);
}

RESULT usbtoswim_enable(uint8_t interface_index)
{
#if PARAM_CHECK
	if (interface_index > 7)
	{
		LOG_BUG(ERRMSG_INVALID_INTERFACE_NUM, interface_index);
		return ERROR_FAIL;
	}
#endif
	return usbtoxxx_enable_command(USB_TO_SWIM, interface_index, NULL, 0);
}

