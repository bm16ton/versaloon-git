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
#include <stdlib.h>

#include "app_cfg.h"
#include "app_type.h"
#include "app_io.h"
#include "app_err.h"
#include "app_log.h"

#include "vsprog.h"
#include "programmer.h"
#include "target.h"
#include "scripts.h"

#include "avr8.h"
#include "avr8_internal.h"

#define CUR_TARGET_STRING		AVR8_STRING

ENTER_PROGRAM_MODE_HANDLER(avr8isp);
LEAVE_PROGRAM_MODE_HANDLER(avr8isp);
ERASE_TARGET_HANDLER(avr8isp);
WRITE_TARGET_HANDLER(avr8isp);
READ_TARGET_HANDLER(avr8isp);
struct program_functions_t avr8isp_program_functions = 
{
	NULL,			// execute
	ENTER_PROGRAM_MODE_FUNCNAME(avr8isp), 
	LEAVE_PROGRAM_MODE_FUNCNAME(avr8isp), 
	ERASE_TARGET_FUNCNAME(avr8isp), 
	WRITE_TARGET_FUNCNAME(avr8isp), 
	READ_TARGET_FUNCNAME(avr8isp)
};



#define spi_init()				interfaces->spi.init(0)
#define spi_fini()				interfaces->spi.fini(0)
#define spi_conf(speed)			\
	interfaces->spi.config(0, (speed), SPI_CPOL_LOW, SPI_CPHA_1EDGE, SPI_MSB_FIRST)
#define spi_io(out, bytelen, in)\
	interfaces->spi.io(0, (out), (in), (bytelen))

#define reset_init()			interfaces->gpio.init(0)
#define reset_fini()			interfaces->gpio.fini(0)
#define reset_output()			\
	interfaces->gpio.config(0, GPIO_SRST, GPIO_SRST, 0, 0)
#define reset_input()			\
	interfaces->gpio.config(0, GPIO_SRST, 0, GPIO_SRST, GPIO_SRST)
#define reset_set()				reset_input()
#define reset_clr()				reset_output()

#define poll_start()			interfaces->poll.start(20, 500)
#define poll_start_once()		interfaces->poll.start(0, 0)
#define poll_end()				interfaces->poll.end()
#define poll_ok(o, m, v)		\
	interfaces->poll.checkok(POLL_CHECK_EQU, (o), 1, (m), (v))
#define poll_fail_unequ(o, m, v)\
	interfaces->poll.checkfail(POLL_CHECK_UNEQU, (o), 1, (m), (v))

#define delay_ms(ms)			interfaces->delay.delayms((ms) | 0x8000)
#define delay_us(us)			interfaces->delay.delayus((us) & 0x7FFF)

#define commit()				interfaces->peripheral_commit()

static struct interfaces_info_t *interfaces = NULL;

static RESULT avr8_isp_pollready(void)
{
	uint8_t cmd_buf[4], ret_buf[4];
	
	if (interfaces->support_mask & IFS_POLL)
	{
		poll_start();
		cmd_buf[0] = 0xF0;
		cmd_buf[1] = 0x00;
		cmd_buf[2] = 0x00;
		cmd_buf[3] = 0x00;
		spi_io(cmd_buf, 4, NULL);
		poll_ok(0, 0x01, 0x00);
		poll_end();
		return ERROR_OK;
	}
	else
	{
		uint8_t i = 20;
		while (i--)
		{
			cmd_buf[0] = 0xF0;
			cmd_buf[1] = 0x00;
			cmd_buf[2] = 0x00;
			cmd_buf[3] = 0x00;
			spi_io(cmd_buf, 4, ret_buf);
			if (ERROR_OK != commit())
			{
				return ERROR_FAIL;
			}
			if ((ret_buf[3] & 0x01) == 0x00)
			{
				return ERROR_OK;
			}
			delay_us(500);
		}
		return ERROR_FAIL;
	}
}

ENTER_PROGRAM_MODE_HANDLER(avr8isp)
{
	struct program_info_t *pi = context->pi;
	uint8_t cmd_buf[4], ret_buf[4];
	
	interfaces = context->prog;
	
	if (!pi->frequency)
	{
		pi->frequency = 560;
	}
	
	// init
	spi_init();
	reset_init();
	
try_frequency:
	// use avr8_isp_frequency
	spi_conf(pi->frequency);
	
	// toggle reset
	reset_clr();
	delay_ms(1);
	reset_input();
	delay_ms(1);
	reset_clr();
	delay_ms(10);
	
	// enter into program mode command
	cmd_buf[0] = 0xAC;
	cmd_buf[1] = 0x53;
	cmd_buf[2] = 0x00;
	cmd_buf[3] = 0x00;
	// ret[2] should be 0x53
	if (interfaces->support_mask & IFS_POLL)
	{
		poll_start_once();
		spi_io(cmd_buf, 4, NULL);
		poll_fail_unequ(1, 0xFF, 0x53);
		poll_end();
		
		LOG_PUSH();
		LOG_MUTE();
		if (ERROR_OK != commit())
		{
			LOG_POP();
			if (pi->frequency > 1)
			{
				pi->frequency /= 2;
				LOG_WARNING("frequency too fast, try slower: %d", pi->frequency);
				goto try_frequency;
			}
			else
			{
				return ERRCODE_FAILURE_ENTER_PROG_MODE;
			}
		}
		LOG_POP();
	}
	else
	{
		spi_io(cmd_buf, 4, ret_buf);
		if (ERROR_OK != commit())
		{
			return ERROR_FAIL;
		}
		if (ret_buf[2] != 0x53)
		{
			if (pi->frequency > 1)
			{
				pi->frequency /= 2;
				LOG_WARNING("frequency too fast, try slower: %d", pi->frequency);
				goto try_frequency;
			}
			else
			{
				return ERRCODE_FAILURE_ENTER_PROG_MODE;
			}
		}
	}
	
	return ERROR_OK;
}

LEAVE_PROGRAM_MODE_HANDLER(avr8isp)
{
	REFERENCE_PARAMETER(context);
	REFERENCE_PARAMETER(success);
	
	reset_input();
	reset_fini();
	spi_fini();
	return commit();
}

ERASE_TARGET_HANDLER(avr8isp)
{
	struct chip_param_t *param = context->param;
	uint8_t cmd_buf[4];
	
	REFERENCE_PARAMETER(area);
	REFERENCE_PARAMETER(addr);
	REFERENCE_PARAMETER(size);
	
	cmd_buf[0] = 0xAC;
	cmd_buf[1] = 0x80;
	cmd_buf[2] = 0x00;
	cmd_buf[3] = 0x00;
	spi_io(cmd_buf, 4, NULL);
	
	if (param->param[AVR8_PARAM_ISP_POLL])
	{
		avr8_isp_pollready();
	}
	else
	{
		delay_ms(10);
	}
	
	return commit();
}

WRITE_TARGET_HANDLER(avr8isp)
{
	struct chip_param_t *param = context->param;
	uint8_t cmd_buf[4];
	uint32_t i;
	uint32_t ee_page_size;
	RESULT ret = ERROR_OK;
	
	switch (area)
	{
	case APPLICATION_CHAR:
		if (param->chip_areas[APPLICATION_IDX].page_num > 1)
		{
			// page mode
			for (i = 0; i < size; i++)
			{
				if (i & 1)
				{
					// high byte
					cmd_buf[0] = 0x40 | 0x08;
				}
				else
				{
					// low byte
					cmd_buf[0] = 0x40;
				}
				cmd_buf[1] = (uint8_t)(i >> 9);
				cmd_buf[2] = (uint8_t)(i >> 1);
				cmd_buf[3] = buff[i];
				spi_io(cmd_buf, 4, NULL);
			}
			
			// write page
			cmd_buf[0] = 0x4C;
			cmd_buf[1] = (uint8_t)(addr >> 9);
			cmd_buf[2] = (uint8_t)(addr >> 1);
			cmd_buf[3] = 0x00;
			spi_io(cmd_buf, 4, NULL);
			
			if (param->param[AVR8_PARAM_ISP_POLL])
			{
				avr8_isp_pollready();
			}
			else
			{
				delay_ms(5);
			}
			
			if (ERROR_OK != commit())
			{
				ret = ERRCODE_FAILURE_OPERATION;
				break;
			}
		}
		else
		{
			// byte mode
			for (i = 0; i < size; i++)
			{
				if (i & 1)
				{
					// high byte
					cmd_buf[0] = 0x40 | 0x08;
				}
				else
				{
					cmd_buf[0] = 0x40;
				}
				cmd_buf[1] = (uint8_t)((addr + i) >> 9);
				cmd_buf[2] = (uint8_t)((addr + i) >> 1);
				cmd_buf[3] = buff[i];
				spi_io(cmd_buf, 4, NULL);
				
				if (param->param[AVR8_PARAM_ISP_POLL])
				{
					avr8_isp_pollready();
				}
				else
				{
					delay_ms(5);
				}
			}
			
			if (ERROR_OK != commit())
			{
				ret = ERRCODE_FAILURE_OPERATION;
				break;
			}
		}
		break;
	case EEPROM_CHAR:
		ee_page_size = param->chip_areas[EEPROM_IDX].page_size;
		if ((param->chip_areas[EEPROM_IDX].page_num > 1) 
			&& (param->param[AVR8_PARAM_ISP_EERPOM_PAGE_EN]))
		{
			while (size > 0)
			{
				// Page mode
				for (i = 0; i < ee_page_size; i++)
				{
					cmd_buf[0] = 0xC1;
					cmd_buf[1] = 0x00;
					cmd_buf[2] = (uint8_t)i;
					cmd_buf[3] = buff[i];
					spi_io(cmd_buf, 4, NULL);
				}
				
				// write page
				cmd_buf[0] = 0xC2;
				cmd_buf[1] = (uint8_t)(addr >> 8);
				cmd_buf[2] = (uint8_t)(addr >> 0);
				cmd_buf[3] = 0x00;
				spi_io(cmd_buf, 4, NULL);
				
				if (param->param[AVR8_PARAM_ISP_POLL])
				{
					avr8_isp_pollready();
				}
				else
				{
					delay_ms(5);
				}
				size -= ee_page_size;
				addr += ee_page_size;
				buff += ee_page_size;
			}
			
			if (ERROR_OK != commit())
			{
				ret = ERRCODE_FAILURE_OPERATION;
				break;
			}
		}
		else
		{
			while (size > 0)
			{
				// Byte mode
				for (i = 0; i < ee_page_size; i++)
				{
					cmd_buf[0] = 0xC0;
					cmd_buf[1] = (uint8_t)((addr + i) >> 8);
					cmd_buf[2] = (uint8_t)((addr + i) >> 0);
					cmd_buf[3] = buff[i];
					spi_io(cmd_buf, 4, NULL);
					
					if (param->param[AVR8_PARAM_ISP_POLL])
					{
						avr8_isp_pollready();
					}
					else
					{
						delay_ms(10);
					}
				}
				size -= ee_page_size;
				addr += ee_page_size;
				buff += ee_page_size;
			}
			
			if (ERROR_OK != commit())
			{
				ret = ERRCODE_FAILURE_OPERATION;
				break;
			}
		}
		break;
	case FUSE_CHAR:
		// low bits
		if (param->chip_areas[FUSE_IDX].size > 0)
		{
			cmd_buf[0] = 0xAC;
			cmd_buf[1] = 0xA0;
			cmd_buf[2] = 0x00;
			cmd_buf[3] = buff[0];
			spi_io(cmd_buf, 4, NULL);
			
			if (param->param[AVR8_PARAM_ISP_POLL])
			{
				avr8_isp_pollready();
			}
			else
			{
				delay_ms(5);
			}
		}
		// high bits
		if (param->chip_areas[FUSE_IDX].size > 1)
		{
			cmd_buf[0] = 0xAC;
			cmd_buf[1] = 0xA8;
			cmd_buf[2] = 0x00;
			cmd_buf[3] = buff[1];
			spi_io(cmd_buf, 4, NULL);
			
			if (param->param[AVR8_PARAM_ISP_POLL])
			{
				avr8_isp_pollready();
			}
			else
			{
				delay_ms(5);
			}
		}
		// extended bits
		if (param->chip_areas[FUSE_IDX].size > 2)
		{
			cmd_buf[0] = 0xAC;
			cmd_buf[1] = 0xA4;
			cmd_buf[2] = 0x00;
			cmd_buf[3] = buff[2];
			spi_io(cmd_buf, 4, NULL);
			
			if (param->param[AVR8_PARAM_ISP_POLL])
			{
				avr8_isp_pollready();
			}
			else
			{
				delay_ms(5);
			}
		}
		if (param->chip_areas[FUSE_IDX].size > 0)
		{
			if (ERROR_OK != commit())
			{
				ret = ERRCODE_FAILURE_OPERATION;
				break;
			}
		}
		else
		{
			LOG_ERROR(ERRMSG_NOT_SUPPORT_BY, "fuse", param->chip_name);
			ret = ERRCODE_NOT_SUPPORT;
			break;
		}
		break;
	case LOCK_CHAR:
		if (param->chip_areas[LOCK_IDX].size > 0)
		{
			cmd_buf[0] = 0xAC;
			cmd_buf[1] = 0xE0;
			cmd_buf[2] = 0x00;
			cmd_buf[3] = buff[0];
			spi_io(cmd_buf, 4, NULL);
			
			if (param->param[AVR8_PARAM_ISP_POLL])
			{
				avr8_isp_pollready();
			}
			else
			{
				delay_ms(5);
			}
			
			if (ERROR_OK != commit())
			{
				ret = ERRCODE_FAILURE_OPERATION;
				break;
			}
		}
		else
		{
			LOG_ERROR(ERRMSG_NOT_SUPPORT_BY, "lock", param->chip_name);
			ret = ERRCODE_NOT_SUPPORT;
			break;
		}
		break;
	default:
		break;
	}
	return ret;
}

READ_TARGET_HANDLER(avr8isp)
{
	struct chip_param_t *param = context->param;
	uint8_t cmd_buf[4], ret_buf[256 * 4];
	uint32_t i, j;
	uint32_t ee_page_size;
	RESULT ret = ERROR_OK;
	
	switch (area)
	{
	case CHIPID_CHAR:
		cmd_buf[0] = 0x30;
		cmd_buf[1] = 0x00;
		cmd_buf[2] = 0x00;
		cmd_buf[3] = 0x00;
		spi_io(cmd_buf, 4, &ret_buf[0]);
		cmd_buf[0] = 0x30;
		cmd_buf[1] = 0x00;
		cmd_buf[2] = 0x01;
		cmd_buf[3] = 0x00;
		spi_io(cmd_buf, 4, &ret_buf[4]);
		cmd_buf[0] = 0x30;
		cmd_buf[1] = 0x00;
		cmd_buf[2] = 0x02;
		cmd_buf[3] = 0x00;
		spi_io(cmd_buf, 4, &ret_buf[8]);
		if (ERROR_OK != commit())
		{
			ret = ERRCODE_FAILURE_OPERATION;
			break;
		}
		buff[0] = ret_buf[11];
		buff[1] = ret_buf[7];
		buff[2] = ret_buf[3];
		break;
	case APPLICATION_CHAR:
		if (size > 256)
		{
			return ERROR_FAIL;
		}
		
		for (i = 0; i < size; i++)
		{
			if (i & 1)
			{
				// high byte
				cmd_buf[0] = 0x20 | 0x08;
			}
			else
			{
				// low byte
				cmd_buf[0] = 0x20;
			}
			cmd_buf[1] = (uint8_t)((addr + i) >> 9);
			cmd_buf[2] = (uint8_t)((addr + i) >> 1);
			cmd_buf[3] = 0;
			spi_io(cmd_buf, 4, &ret_buf[4 * i]);
		}
		
		if (ERROR_OK != commit())
		{
			ret = ERRCODE_FAILURE_OPERATION;
			break;
		}
		for (i = 0; i < size; i++)
		{
			buff[i] = ret_buf[4 * i + 3];
		}
		break;
	case EEPROM_CHAR:
		if (size > 256)
		{
			return ERROR_FAIL;
		}
		
		ee_page_size = param->chip_areas[EEPROM_IDX].page_size;
		for (j = 0; j < size;)
		{
			for (i = 0; i < ee_page_size; i++)
			{
				cmd_buf[0] = 0xA0;
				cmd_buf[1] = (uint8_t)((addr + i) >> 8);
				cmd_buf[2] = (uint8_t)((addr + i) >> 0);
				cmd_buf[3] = 0;
				spi_io(cmd_buf, 4, &ret_buf[4 * j]);
				j++;
			}
			addr += ee_page_size;
		}
		
		if (ERROR_OK != commit())
		{
			ret = ERRCODE_FAILURE_OPERATION;
			break;
		}
		for (i = 0; i < size; i++)
		{
			buff[i] = ret_buf[4 * i + 3];
		}
		break;
	case FUSE_CHAR:
		memset(ret_buf, 0, 3);
		// low bits
		if (param->chip_areas[FUSE_IDX].size > 0)
		{
			cmd_buf[0] = 0x50;
			cmd_buf[1] = 0x00;
			cmd_buf[2] = 0x00;
			cmd_buf[3] = 0x00;
			spi_io(cmd_buf, 4, &ret_buf[0]);
		}
		// high bits
		if (param->chip_areas[FUSE_IDX].size > 1)
		{
			cmd_buf[0] = 0x58;
			cmd_buf[1] = 0x08;
			cmd_buf[2] = 0x00;
			cmd_buf[3] = 0x00;
			spi_io(cmd_buf, 4, &ret_buf[4]);
		}
		// extended bits
		if (param->chip_areas[FUSE_IDX].size > 2)
		{
			cmd_buf[0] = 0x50;
			cmd_buf[1] = 0x08;
			cmd_buf[2] = 0x00;
			cmd_buf[3] = 0x00;
			spi_io(cmd_buf, 4, &ret_buf[8]);
		}
		if (param->chip_areas[FUSE_IDX].size > 0)
		{
			if (ERROR_OK != commit())
			{
				ret = ERRCODE_FAILURE_OPERATION;
				break;
			}
			buff[0] = ret_buf[3];
			buff[1] = ret_buf[7];
			buff[2] = ret_buf[11];
		}
		else
		{
			LOG_ERROR(ERRMSG_NOT_SUPPORT_BY, "fuse", param->chip_name);
			ret = ERRCODE_NOT_SUPPORT;
			break;
		}
		break;
	case LOCK_CHAR:
		if (param->chip_areas[LOCK_IDX].size > 0)
		{
			cmd_buf[0] = 0x58;
			cmd_buf[1] = 0x00;
			cmd_buf[2] = 0x00;
			cmd_buf[3] = 0x00;
			spi_io(cmd_buf, 4, &ret_buf[0]);
			if (ERROR_OK != commit())
			{
				ret = ERRCODE_FAILURE_OPERATION;
				break;
			}
			buff[0] = ret_buf[3];
		}
		else
		{
			LOG_ERROR(ERRMSG_NOT_SUPPORT_BY, "lock", param->chip_name);
			ret = ERRCODE_NOT_SUPPORT;
			break;
		}
		break;
	case CALIBRATION_CHAR:
		memset(ret_buf, 0, 4);
		if (param->chip_areas[CALIBRATION_IDX].size > 0)
		{
			cmd_buf[0] = 0x38;
			cmd_buf[1] = 0x00;
			cmd_buf[2] = 0x00;
			cmd_buf[3] = 0x00;
			spi_io(cmd_buf, 4, &ret_buf[0]);
		}
		if (param->chip_areas[CALIBRATION_IDX].size > 1)
		{
			cmd_buf[0] = 0x38;
			cmd_buf[1] = 0x00;
			cmd_buf[2] = 0x01;
			cmd_buf[3] = 0x00;
			spi_io(cmd_buf, 4, &ret_buf[4]);
		}
		if (param->chip_areas[CALIBRATION_IDX].size > 2)
		{
			cmd_buf[0] = 0x38;
			cmd_buf[1] = 0x00;
			cmd_buf[2] = 0x02;
			cmd_buf[3] = 0x00;
			spi_io(cmd_buf, 4, &ret_buf[8]);
		}
		if (param->chip_areas[CALIBRATION_IDX].size > 3)
		{
			cmd_buf[0] = 0x38;
			cmd_buf[1] = 0x00;
			cmd_buf[2] = 0x03;
			cmd_buf[3] = 0x00;
			spi_io(cmd_buf, 4, &ret_buf[12]);
		}
		if (param->chip_areas[CALIBRATION_IDX].size > 0)
		{
			if (ERROR_OK != commit())
			{
				ret = ERRCODE_FAILURE_OPERATION;
				break;
			}
			buff[0] = ret_buf[3];
			buff[1] = ret_buf[7];
			buff[2] = ret_buf[11];
			buff[3] = ret_buf[15];
		}
		else
		{
			LOG_ERROR(ERRMSG_NOT_SUPPORT_BY, "calibration", param->chip_name);
			ret = ERRCODE_NOT_SUPPORT;
			break;
		}
		break;
	default:
		break;
	}
	return ret;
}

