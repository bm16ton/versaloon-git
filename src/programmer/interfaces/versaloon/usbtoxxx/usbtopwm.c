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

vsf_err_t usbtopwm_init(uint8_t interface_index)
{
	return usbtoxxx_init_command(USB_TO_PWM, interface_index);
}

vsf_err_t usbtopwm_fini(uint8_t interface_index)
{
	return usbtoxxx_fini_command(USB_TO_PWM, interface_index);
}

vsf_err_t usbtopwm_config(uint8_t interface_index, uint16_t kHz, uint8_t mode)
{
	uint8_t buff[3];
	
#if PARAM_CHECK
	if (interface_index > 7)
	{
		LOG_BUG(ERRMSG_INVALID_INTERFACE_NUM, interface_index);
		return VSFERR_FAIL;
	}
#endif
	
	SET_LE_U16(&buff[0], kHz);
	buff[2] = mode;
	
	return usbtoxxx_conf_command(USB_TO_PWM, interface_index, buff, 3);
}

vsf_err_t usbtopwm_out(uint8_t interface_index, uint16_t count, uint16_t *rate)
{
	uint16_t i;
	
#if PARAM_CHECK
	if (interface_index > 7)
	{
		LOG_BUG(ERRMSG_INVALID_INTERFACE_NUM, interface_index);
		return VSFERR_FAIL;
	}
	if (NULL == rate)
	{
		return VSFERR_FAIL;
	}
#endif
	
	SET_LE_U16(&versaloon_cmd_buf[0], count);
	for (i = 0; i < count; i++)
	{
		SET_LE_U16(&versaloon_cmd_buf[2 + i * 2], rate[i]);
	}
	
	return usbtoxxx_out_command(USB_TO_PWM, interface_index, versaloon_cmd_buf,
								2 + 2 * count, 0);
}

vsf_err_t usbtopwm_in(uint8_t interface_index, uint16_t count, uint16_t *rate)
{
#if PARAM_CHECK
	if (interface_index > 7)
	{
		LOG_BUG(ERRMSG_INVALID_INTERFACE_NUM, interface_index);
		return VSFERR_FAIL;
	}
	if (NULL == rate)
	{
		return VSFERR_FAIL;
	}
#endif
	
	SET_LE_U16(&versaloon_cmd_buf[0], count);
	memset(&versaloon_cmd_buf[2], 0, 4 * count);
	
	return usbtoxxx_in_command(USB_TO_PWM, interface_index, versaloon_cmd_buf,
					2 + 4 * count, 4 * count, (uint8_t *)rate, 0, 4 * count, 0);
}

