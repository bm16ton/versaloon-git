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

jtag_callback_t usbtojtaghl_receive_callback = NULL;
jtag_callback_t usbtojtaghl_send_callback = NULL;
uint32_t usbtojtaghl_ir_backup = 0;

RESULT usbtojtaghl_callback(void *p, uint8_t *src, uint8_t *processed)
{
	struct versaloon_pending_t *cur_pending = (struct versaloon_pending_t *)p;
	uint16_t processed_len = 0;
	RESULT ret;
	
	if (NULL == usbtojtaghl_receive_callback)
	{
		return ERROR_OK;
	}
	
	if (cur_pending->id & 0x80000000)
	{
		// DR
		ret = usbtojtaghl_receive_callback(0, JTAG_SCANTYPE_DR, 
				cur_pending->id & 0x7FFFFFFF, cur_pending->data_buffer, 
				src, cur_pending->actual_data_size, &processed_len);
	}
	else
	{
		// IR
		ret = usbtojtaghl_receive_callback(0, JTAG_SCANTYPE_IR, 
				cur_pending->id & 0x7FFFFFFF, cur_pending->data_buffer, 
				src, cur_pending->actual_data_size, &processed_len);
	}
	if (ret != ERROR_OK)
	{
		LOG_ERROR(ERRMSG_FAILURE_OPERATION, "call callback");
		return ERRCODE_FAILURE_OPERATION;
	}
	if (processed_len)
	{
		*processed = 1;
	}
	
	return ERROR_OK;
}

RESULT usbtojtaghl_register_callback(uint8_t index, jtag_callback_t send_callback, 
									 jtag_callback_t receive_callback)
{
	REFERENCE_PARAMETER(index);
	
	usbtojtaghl_send_callback = send_callback;
	usbtojtaghl_receive_callback = receive_callback;
	return ERROR_OK;
}

RESULT usbtojtaghl_init(uint8_t interface_index)
{
	return usbtoxxx_init_command(USB_TO_JTAG_HL, interface_index);
}

RESULT usbtojtaghl_fini(uint8_t interface_index)
{
	return usbtoxxx_fini_command(USB_TO_JTAG_HL, interface_index);
}

RESULT usbtojtaghl_config(uint8_t interface_index, uint16_t kHz, uint8_t ub, 
						  uint8_t ua, uint16_t bb, uint16_t ba)
{
	uint8_t cfg_buf[8];
	
#if PARAM_CHECK
	if (interface_index > 7)
	{
		LOG_BUG(ERRMSG_INVALID_INTERFACE_NUM, interface_index);
		return ERROR_FAIL;
	}
#endif
	
	SET_LE_U16(&cfg_buf[0], kHz);
	cfg_buf[2] = ub;
	cfg_buf[3] = ua;
	SET_LE_U16(&cfg_buf[4], bb);
	SET_LE_U16(&cfg_buf[6], ba);
	
	return usbtoxxx_conf_command(USB_TO_JTAG_HL, interface_index, cfg_buf, 8);
}

RESULT usbtojtaghl_ir(uint8_t interface_index, uint8_t *ir, uint16_t bitlen, 
					  uint8_t idle, uint8_t want_ret)
{
	uint16_t bytelen = (bitlen + 7) >> 3;
	uint16_t processed_len = 0;
	
#if PARAM_CHECK
	if (interface_index > 7)
	{
		LOG_BUG(ERRMSG_INVALID_INTERFACE_NUM, interface_index);
		return ERROR_FAIL;
	}
#endif
	
	usbtojtaghl_ir_backup = 0;
	if (bytelen > 4)
	{
		memcpy(&usbtojtaghl_ir_backup, ir, 4);
	}
	else
	{
		memcpy(&usbtojtaghl_ir_backup, ir, bytelen);
	}
	
	bitlen |= 0x8000;		// indicate ir
	SET_LE_U16(&versaloon_cmd_buf[0], bitlen);
	versaloon_cmd_buf[2] = idle;
	
	if (usbtojtaghl_send_callback != NULL)
	{
		usbtojtaghl_send_callback(0, JTAG_SCANTYPE_IR, usbtojtaghl_ir_backup, 
						versaloon_cmd_buf + 3, ir, bytelen, &processed_len);
	}
	
	if (processed_len)
	{
		bytelen = processed_len;
	}
	else
	{
		memcpy(versaloon_cmd_buf + 3, ir, bytelen);
	}
	
	// clear MSB to indicate IR
	versaloon_set_pending_id(usbtojtaghl_ir_backup & 0x7FFFFFFF);
	versaloon_set_callback(usbtojtaghl_callback);
	if (want_ret)
	{
		return usbtoxxx_inout_command(USB_TO_JTAG_HL, interface_index, 
					versaloon_cmd_buf, bytelen + 3, bytelen, ir, 0, bytelen, 1);
	}
	else
	{
		return usbtoxxx_inout_command(USB_TO_JTAG_HL, interface_index, 
					versaloon_cmd_buf, bytelen + 3, bytelen, NULL, 0, 0, 1);
	}
}

RESULT usbtojtaghl_dr(uint8_t interface_index, uint8_t *dr, uint16_t bitlen, 
					  uint8_t idle, uint8_t want_ret)
{
	uint16_t bytelen = (bitlen + 7) >> 3;
	uint16_t processed_len = 0;
	
#if PARAM_CHECK
	if (interface_index > 7)
	{
		LOG_BUG(ERRMSG_INVALID_INTERFACE_NUM, interface_index);
		return ERROR_FAIL;
	}
#endif
	
	SET_LE_U16(&versaloon_cmd_buf[0], bitlen);
	versaloon_cmd_buf[2] = idle;
	
	if (usbtojtaghl_send_callback != NULL)
	{
		usbtojtaghl_send_callback(0, JTAG_SCANTYPE_DR, usbtojtaghl_ir_backup, 
						versaloon_cmd_buf + 3, dr, bytelen, &processed_len);
	}
	
	if (processed_len)
	{
		bytelen = processed_len;
	}
	else
	{
		memcpy(versaloon_cmd_buf + 3, dr, bytelen);
	}
	
	// set MSB to indicate DR
	versaloon_set_pending_id(usbtojtaghl_ir_backup | 0x80000000);
	versaloon_set_callback(usbtojtaghl_callback);
	if (want_ret)
	{
		return usbtoxxx_inout_command(USB_TO_JTAG_HL, interface_index, 
					versaloon_cmd_buf, bytelen + 3, bytelen, dr, 0, bytelen, 1);
	}
	else
	{
		return usbtoxxx_inout_command(USB_TO_JTAG_HL, interface_index, 
					versaloon_cmd_buf, bytelen + 3, bytelen, NULL, 0, 0, 1);
	}
}

RESULT usbtojtaghl_tms(uint8_t interface_index, uint8_t *tms, uint16_t bitlen)
{
#if PARAM_CHECK
	if (interface_index > 7)
	{
		LOG_BUG(ERRMSG_INVALID_INTERFACE_NUM, interface_index);
		return ERROR_FAIL;
	}
	if (bitlen > 256)
	{
		LOG_BUG(ERRMSG_INVALID_VALUE, bitlen, "tms bit size(1..256)");
		return ERROR_FAIL;
	}
#endif
	
	versaloon_cmd_buf[0] = (uint8_t)(bitlen - 1);
	memcpy(versaloon_cmd_buf + 1, tms, bitlen);
	
	return usbtoxxx_out_command(USB_TO_JTAG_HL, interface_index, 
							versaloon_cmd_buf, ((bitlen + 7) >> 3) + 1, 0);
}

RESULT usbtojtaghl_runtest(uint8_t interface_index, uint32_t cycles)
{
	uint8_t tms[256 / 8];
	uint16_t cur_cycles;
	
#if PARAM_CHECK
	if (interface_index > 7)
	{
		LOG_BUG(ERRMSG_INVALID_INTERFACE_NUM, interface_index);
		return ERROR_FAIL;
	}
#endif
	
	memset(tms, 0, sizeof(tms));
	while (cycles > 0)
	{
		if (cycles > 256)
		{
			cur_cycles = 256;
		}
		else
		{
			cur_cycles = (uint8_t)cycles;
		}
		
		if (ERROR_OK != usbtojtaghl_tms(interface_index, tms, cur_cycles))
		{
			return ERROR_FAIL;
		}
		
		cycles -= cur_cycles;
	}
	return ERROR_OK;
}

