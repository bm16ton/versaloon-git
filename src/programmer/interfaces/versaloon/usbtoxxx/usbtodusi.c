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

RESULT usbtodusi_init(uint8_t interface_index)
{
	return usbtoxxx_init_command(USB_TO_DUSI, interface_index);
}

RESULT usbtodusi_fini(uint8_t interface_index)
{
	return usbtoxxx_fini_command(USB_TO_DUSI, interface_index);
}

RESULT usbtodusi_config(uint8_t interface_index, uint16_t kHz, uint8_t cpol, 
					   uint8_t cpha, uint8_t firstbit)
{
	uint8_t conf[3];
	
#if PARAM_CHECK
	if (interface_index > 7)
	{
		LOG_BUG(ERRMSG_INVALID_INTERFACE_NUM, interface_index);
		return ERROR_FAIL;
	}
#endif
	
	conf[0] = cpol | cpha | firstbit;
	SET_LE_U16(&conf[1], kHz);
	
	return usbtoxxx_conf_command(USB_TO_DUSI, interface_index, conf, 3);
}

RESULT usbtodusi_io(uint8_t interface_index, uint8_t *mo, uint8_t *mi, 
					uint8_t *so, uint8_t *si, uint32_t bitlen)
{
	uint16_t bytelen = (uint16_t)((bitlen + 7) / 8);
	
#if PARAM_CHECK
	if (interface_index > 7)
	{
		LOG_BUG(ERRMSG_INVALID_INTERFACE_NUM, interface_index);
		return ERROR_FAIL;
	}
#endif
	
	SET_LE_U32(&versaloon_cmd_buf[0], bitlen);
	if (mo != NULL)
	{
		memcpy(&versaloon_cmd_buf[2], mo, bytelen);
	}
	else
	{
		memset(&versaloon_cmd_buf[2], 0xFF, bytelen);
	}
	if (so != NULL)
	{
		memcpy(&versaloon_cmd_buf[2 + bytelen], so, bytelen);
	}
	else
	{
		memset(&versaloon_cmd_buf[2 + bytelen], 0xFF, bytelen);
	}
	
	if (mi != NULL)
	{
		if (ERROR_OK != versaloon_add_want_pos(0, bytelen, mi))
		{
			return ERROR_FAIL;
		}
	}
	if (si != NULL)
	{
		if (ERROR_OK != versaloon_add_want_pos(bytelen, bytelen, si))
		{
			return ERROR_FAIL;
		}
	}
	
	return usbtoxxx_inout_command(USB_TO_DUSI, interface_index, 
		versaloon_cmd_buf, 2 + 2 * bytelen, 2 * bytelen, NULL, 0, 0, 0);
}

