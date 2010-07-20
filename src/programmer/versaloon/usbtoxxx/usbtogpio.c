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

#include <stdio.h>
#include <string.h>

#include "../versaloon_include.h"
#include "../versaloon.h"
#include "../versaloon_internal.h"
#include "usbtoxxx.h"
#include "usbtoxxx_internal.h"

uint8_t usbtogpio_num_of_interface;

RESULT usbtogpio_init(void)
{
	return usbtoxxx_init_command(USB_TO_GPIO, &usbtogpio_num_of_interface);
}

RESULT usbtogpio_fini(void)
{
	return usbtoxxx_fini_command(USB_TO_GPIO);
}

RESULT usbtogpio_config(uint8_t interface_index, uint16_t mask, 
						uint16_t dir_mask, uint16_t input_pull_mask)
{
	uint8_t conf[6];
	
#if PARAM_CHECK
	if (interface_index > 7)
	{
		LOG_BUG(ERRMSG_INVALID_INTERFACE_NUM, interface_index);
		return ERROR_FAIL;
	}
#endif
	
	dir_mask &= mask;
	conf[0] = (mask >> 0) & 0xFF;
	conf[1] = (mask >> 8) & 0xFF;
	conf[2] = (dir_mask >> 0) & 0xFF;
	conf[3] = (dir_mask >> 8) & 0xFF;
	conf[4] = (input_pull_mask >> 0) & 0xFF;
	conf[5] = (input_pull_mask >> 8) & 0xFF;
	
	return usbtoxxx_conf_command(USB_TO_GPIO, interface_index, conf, 6);
}

RESULT usbtogpio_in(uint8_t interface_index, uint16_t mask, uint16_t *value)
{
	uint8_t buf[2];
	
#if PARAM_CHECK
	if (interface_index > 7)
	{
		LOG_BUG(ERRMSG_INVALID_INTERFACE_NUM, interface_index);
		return ERROR_FAIL;
	}
#endif
	
	buf[0] = (mask >> 0) & 0xFF;
	buf[1] = (mask >> 8) & 0xFF;
	
	return usbtoxxx_in_command(USB_TO_GPIO, interface_index, buf, 2, 2, 
							   (uint8_t*)value, 0, 2, 0);
}

RESULT usbtogpio_out(uint8_t interface_index, uint16_t mask, uint16_t value)
{
	uint8_t buf[4];
	
#if PARAM_CHECK
	if (interface_index > 7)
	{
		LOG_BUG(ERRMSG_INVALID_INTERFACE_NUM, interface_index);
		return ERROR_FAIL;
	}
#endif
	
	buf[0] = (mask >> 0) & 0xFF;
	buf[1] = (mask >> 8) & 0xFF;
	buf[2] = (value >> 0) & 0xFF;
	buf[3] = (value >> 8) & 0xFF;
	
	return usbtoxxx_out_command(USB_TO_GPIO, interface_index, buf, 4, 0);
}

