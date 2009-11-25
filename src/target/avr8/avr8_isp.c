/***************************************************************************
 *   Copyright (C) 2009 by Simon Qian <SimonQian@SimonQian.com>            *
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
#include <stdlib.h>

#include "app_cfg.h"
#include "app_type.h"
#include "app_err.h"
#include "app_log.h"
#include "prog_interface.h"

#include "memlist.h"
#include "pgbar.h"

#include "vsprog.h"
#include "programmer.h"
#include "target.h"

#include "avr8.h"
#include "avr8_internal.h"

#define CUR_TARGET_STRING		AVR8_STRING
#define cur_chip_param			target_chip_param
#define cur_frequency			avr8_isp_frequency

RESULT avr8_isp_program(operation_t operations, program_info_t *pi, 
						programmer_info_t *prog)
{
#define spi_init()				prog->spi_init()
#define spi_fini()				prog->spi_fini()
#define spi_conf(speed)			prog->spi_config((speed), SPI_CPOL_LOW, \
												 SPI_CPHA_1EDGE, SPI_MSB_FIRST)
#define spi_io(out, outlen, in, inpos, inlen)	\
								prog->spi_io((out), (in), (outlen), \
											 (inpos), (inlen))

#define reset_init()			prog->gpio_init()
#define reset_fini()			prog->gpio_fini()
#define reset_output()			prog->gpio_config(GPIO_SRST, 1, 0)
#define reset_input()			prog->gpio_config(GPIO_SRST, 0, GPIO_SRST)
#define reset_set()				reset_input()
#define reset_clr()				prog->gpio_out(GPIO_SRST, 0)

#define delay_ms(ms)			prog->delayms((ms) | 0x8000)
#define delay_us(us)			prog->delayus((us) & 0x7FFF)

#define commit()				prog->peripheral_commit()


	uint8_t data_tmp[4];
	uint8_t cmd_buf[4];
	uint8_t poll_byte;
	int32_t i;
	uint32_t j, k, page_size, len_current_list;
	uint8_t page_buf[256];
	RESULT ret = ERROR_OK;
	memlist *ml_tmp;
	
	// here we go
	// init
	spi_init();
	reset_init();
	
try_frequency:
	// enter program mode
	if (cur_frequency > 0)
	{
		// use avr8_isp_frequency
		spi_conf(cur_frequency);
		
		// toggle reset
		reset_clr();
		reset_output();
		delay_ms(1);
		reset_input();
		delay_ms(1);
		reset_clr();
		reset_output();
		delay_ms(10);
		
		// enter into program mode command
		cmd_buf[0] = 0xAC;
		cmd_buf[1] = 0x53;
		cmd_buf[2] = 0x00;
		cmd_buf[3] = 0x00;
		poll_byte = 0;
		// ret[2] should be 0x53
		spi_io(cmd_buf, 4, &poll_byte, 2, 1);
		if (ERROR_OK != commit())
		{
			LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_ENTER_PROG_MODE));
			ret = ERRCODE_FAILURE_ENTER_PROG_MODE;
			goto leave_program_mode;
		}
		if (poll_byte != 0x53)
		{
			if (cur_frequency > 1)
			{
				cur_frequency /= 2;
				LOG_WARNING(_GETTEXT("frequency too fast, try slower: %d\n"), 
							cur_frequency);
				goto try_frequency;
			}
			else
			{
				LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_ENTER_PROG_MODE));
				ret = ERRCODE_FAILURE_ENTER_PROG_MODE;
				goto leave_program_mode;
			}
		}
	}
	else
	{
		// TODO: auto check frequency
		LOG_ERROR(_GETTEXT(ERRMSG_INVALID_VALUE), 
				  cur_frequency, "ISP frequency");
		ret = ERRCODE_INVALID;
		goto leave_program_mode;
	}
	
	// read chip_id
	cmd_buf[0] = 0x30;
	cmd_buf[1] = 0x00;
	cmd_buf[2] = 0x00;
	cmd_buf[3] = 0x00;
	spi_io(cmd_buf, 4, &data_tmp[2], 3, 1);
	cmd_buf[0] = 0x30;
	cmd_buf[1] = 0x00;
	cmd_buf[2] = 0x01;
	cmd_buf[3] = 0x00;
	spi_io(cmd_buf, 4, &data_tmp[1], 3, 1);
	cmd_buf[0] = 0x30;
	cmd_buf[1] = 0x00;
	cmd_buf[2] = 0x02;
	cmd_buf[3] = 0x00;
	spi_io(cmd_buf, 4, &data_tmp[0], 3, 1);
	if (ERROR_OK != commit())
	{
		LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), "read signature");
		ret = ERRCODE_FAILURE_OPERATION;
		goto leave_program_mode;
	}
	pi->chip_id = ((data_tmp[2] & 0xFF) << 16) | ((data_tmp[1] & 0xFF) << 8) 
				   | ((data_tmp[0] & 0xFF) << 0);
	LOG_INFO(_GETTEXT(INFOMSG_TARGET_CHIP_ID), pi->chip_id);
	if (!(operations.read_operations & CHIP_ID))
	{
		if (pi->chip_id != cur_chip_param.chip_id)
		{
			LOG_WARNING(_GETTEXT(ERRMSG_INVALID_CHIP_ID), pi->chip_id, 
						cur_chip_param.chip_id);
		}
	}
	else
	{
		goto leave_program_mode;
	}
	
	if (operations.erase_operations > 0)
	{
		LOG_INFO(_GETTEXT(INFOMSG_ERASING), "chip");
		pgbar_init("erasing chip |", "|", 0, 1, PROGRESS_STEP, '=');
		
		cmd_buf[0] = 0xAC;
		cmd_buf[1] = 0x80;
		cmd_buf[2] = 0x00;
		cmd_buf[3] = 0x00;
		spi_io(cmd_buf, 4, NULL, 0, 0);
		delay_ms(20);
		if (ERROR_OK != commit())
		{
			pgbar_fini();
			LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), "erase chip");
			ret = ERRCODE_FAILURE_OPERATION;
			goto leave_program_mode;
		}
		
		pgbar_update(1);
		pgbar_fini();
		LOG_INFO(_GETTEXT(INFOMSG_ERASED), "chip");
	}
	
	// set page size for flash
	if (cur_chip_param.app_page_num > 1)
	{
		page_size = cur_chip_param.app_page_size;
	}
	else
	{
		page_size = 256;
	}
	
	if (operations.write_operations & APPLICATION)
	{
		// program flash
		LOG_INFO(_GETTEXT(INFOMSG_PROGRAMMING), "flash");
		pgbar_init("writing flash |", "|", 0, pi->app_size_valid, 
				   PROGRESS_STEP, '=');
		
		ml_tmp = pi->app_memlist;
		while(ml_tmp != NULL)
		{
			if ((ml_tmp->addr + ml_tmp->len) 
				<= (ml_tmp->addr - (ml_tmp->addr % page_size) + page_size))
			{
				k = ml_tmp->len;
			}
			else
			{
				k = page_size - (ml_tmp->addr % page_size);
			}
			
			len_current_list = (uint32_t)ml_tmp->len;
			for (i = -(int32_t)(ml_tmp->addr % page_size); 
				 i < ((int32_t)ml_tmp->len - (int32_t)(ml_tmp->addr % page_size)); 
				 i += page_size)
			{
				if (cur_chip_param.app_page_num > 1)
				{
					// Page mode
					for (j = 0; j < page_size; j++)
					{
						if (j & 1)
						{
							// high byte
							cmd_buf[0] = 0x40 | 0x08;
						}
						else
						{
							// low byte
							cmd_buf[0] = 0x40;
						}
						cmd_buf[1] = (uint8_t)(j >> 9);
						cmd_buf[2] = (uint8_t)(j >> 1);
						cmd_buf[3] = pi->app[ml_tmp->addr + i + j];
						spi_io(cmd_buf, 4, NULL, 0, 0);
					}
					
					// write page
					cmd_buf[0] = 0x4C;
					cmd_buf[1] = (uint8_t)((ml_tmp->addr + i) >> 9);
					cmd_buf[2] = (uint8_t)((ml_tmp->addr + i) >> 1);
					cmd_buf[3] = 0x00;
					spi_io(cmd_buf, 4, NULL, 0, 0);
					delay_ms(5);
					
					if (ERROR_OK != commit())
					{
						pgbar_fini();
						LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION_ADDR), 
								  "program flash in page mode", 
								  ml_tmp->addr + i);
						ret = ERRCODE_FAILURE_OPERATION;
						goto leave_program_mode;
					}
				}
				else
				{
					// Byte mode
					for (j = 0; j < page_size; j++)
					{
						if (j & 1)
						{
							// high byte
							cmd_buf[0] = 0x40 | 0x08;
						}
						else
						{
							cmd_buf[0] = 0x40;
						}
						cmd_buf[1] = (uint8_t)((ml_tmp->addr + i + j) >> 9);
						cmd_buf[2] = (uint8_t)((ml_tmp->addr + i + j) >> 1);
						cmd_buf[3] = pi->app[ml_tmp->addr + i + j];
						spi_io(cmd_buf, 4, NULL, 0, 0);
						delay_ms(5);
					}
					
					if (ERROR_OK != commit())
					{
						pgbar_fini();
						LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION_ADDR), 
								  "program flash in byte mode", 
								  ml_tmp->addr + i);
						ret = ERRCODE_FAILURE_OPERATION;
						goto leave_program_mode;
					}
				}
				
				pgbar_update(k);
				len_current_list -= k;
				if (len_current_list >= page_size)
				{
					k = page_size;
				}
				else
				{
					k = len_current_list;
				}
			}
			
			ml_tmp = MEMLIST_GetNext(ml_tmp);
		}
		
		pgbar_fini();
		LOG_INFO(_GETTEXT(INFOMSG_PROGRAMMED_SIZE), "flash", 
				 pi->app_size_valid);
	}
	
	if (operations.read_operations & APPLICATION)
	{
		if (operations.verify_operations & APPLICATION)
		{
			LOG_INFO(_GETTEXT(INFOMSG_VERIFYING), "flash");
		}
		else
		{
			pi->app_size_valid = cur_chip_param.app_size;
			LOG_INFO(_GETTEXT(INFOMSG_READING), "flash");
		}
		pgbar_init("reading flash |", "|", 0, pi->app_size_valid, 
				   PROGRESS_STEP, '=');
		
		ml_tmp = pi->app_memlist;
		while (ml_tmp != NULL)
		{
			if ((ml_tmp->addr + ml_tmp->len)
				<= (ml_tmp->addr - (ml_tmp->addr % page_size) + page_size))
			{
				k = ml_tmp->len;
			}
			else
			{
				k = page_size - (ml_tmp->addr % page_size);
			}
			
			len_current_list = (uint32_t)ml_tmp->len;
			for (i = -(int32_t)(ml_tmp->addr % page_size); 
				 i < ((int32_t)ml_tmp->len - (int32_t)(ml_tmp->addr % page_size)); 
				 i += page_size)
			{
				for (j = 0; j < page_size; j++)
				{
					if (j & 1)
					{
						// high byte
						cmd_buf[0] = 0x20 | 0x08;
					}
					else
					{
						// low byte
						cmd_buf[0] = 0x20;
					}
					cmd_buf[1] = (uint8_t)((ml_tmp->addr + i + j) >> 9);
					cmd_buf[2] = (uint8_t)((ml_tmp->addr + i + j) >> 1);
					cmd_buf[3] = 0;
					spi_io(cmd_buf, 4, page_buf + j, 3, 1);
				}
				
				if (ERROR_OK != commit())
				{
					pgbar_fini();
					LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION_ADDR), 
								  "read flash in byte mode", ml_tmp->addr + i);
						ret = ERRCODE_FAILURE_OPERATION;
					goto leave_program_mode;
				}
				
				for (j = 0; j < page_size; j++)
				{
					if (operations.verify_operations & APPLICATION)
					{
						if (page_buf[j] != pi->app[ml_tmp->addr + i + j])
						{
							pgbar_fini();
							LOG_ERROR(
								_GETTEXT(ERRMSG_FAILURE_VERIFY_TARGET_AT_02X), 
								"flash", 
								ml_tmp->addr + i + j, page_buf[j], 
								pi->app[ml_tmp->addr + i + j]);
							ret = ERRCODE_FAILURE_VERIFY_TARGET;
							goto leave_program_mode;
						}
					}
					else
					{
						memcpy(&pi->app[ml_tmp->addr + i], page_buf, 
							   page_size);
					}
				}
				
				pgbar_update(k);
				len_current_list -= k;
				if (len_current_list >= page_size)
				{
					k = page_size;
				}
				else
				{
					k = len_current_list;
				}
			}
			
			ml_tmp = MEMLIST_GetNext(ml_tmp);
		}
		
		pgbar_fini();
		if (operations.verify_operations & APPLICATION)
		{
			LOG_INFO(_GETTEXT(INFOMSG_VERIFIED_SIZE), "flash", 
					 pi->app_size_valid);
		}
		else
		{
			LOG_INFO(_GETTEXT(INFOMSG_READ), "flash");
		}
	}
	
	// set page size for eeprom
	if (cur_chip_param.ee_page_num > 1)
	{
		page_size = cur_chip_param.ee_page_size;
	}
	else
	{
		page_size = 256;
	}
	
	if (operations.write_operations & EEPROM)
	{
		// program eeprom
		LOG_INFO(_GETTEXT(INFOMSG_PROGRAMMING), "eeprom");
		pgbar_init("writing eeprom |", "|", 0, pi->eeprom_size_valid, 
				   PROGRESS_STEP, '=');
		
		ml_tmp = pi->eeprom_memlist;
		while(ml_tmp != NULL)
		{
			if ((ml_tmp->addr + ml_tmp->len) 
				<= (ml_tmp->addr - (ml_tmp->addr % page_size) + page_size))
			{
				k = ml_tmp->len;
			}
			else
			{
				k = page_size - (ml_tmp->addr % page_size);
			}
			
			len_current_list = (uint32_t)ml_tmp->len;
			for (i = -(int32_t)(ml_tmp->addr % page_size); 
				 i < ((int32_t)ml_tmp->len - (int32_t)(ml_tmp->addr % page_size)); 
				 i += page_size)
			{
				if ((cur_chip_param.ee_page_num > 1) 
					&& (cur_chip_param.param[AVR8_PARAM_ISP_EERPOM_PAGE_EN]))
				{
					// Page mode
					for (j = 0; j < page_size; j++)
					{
						cmd_buf[0] = 0xC1;
						cmd_buf[1] = 0x00;
						cmd_buf[2] = (uint8_t)j;
						cmd_buf[3] = pi->eeprom[ml_tmp->addr + i + j];
						spi_io(cmd_buf, 4, NULL, 0, 0);
					}
					
					// write page
					cmd_buf[0] = 0xC2;
					cmd_buf[1] = (uint8_t)((ml_tmp->addr + i) >> 8);
					cmd_buf[2] = (uint8_t)((ml_tmp->addr + i) >> 0);
					cmd_buf[3] = 0x00;
					spi_io(cmd_buf, 4, NULL, 0, 0);
					delay_ms(5);
					
					if (ERROR_OK != commit())
					{
						pgbar_fini();
						LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION_ADDR), 
								  "program eeprom in page mode", 
								  ml_tmp->addr + i);
						ret = ERRCODE_FAILURE_OPERATION;
						goto leave_program_mode;
					}
				}
				else
				{
					// Byte mode
					for (j = 0; j < page_size; j++)
					{
						cmd_buf[0] = 0xC0;
						cmd_buf[1] = (uint8_t)((ml_tmp->addr + i + j) >> 8);
						cmd_buf[2] = (uint8_t)((ml_tmp->addr + i + j) >> 0);
						cmd_buf[3] = pi->eeprom[ml_tmp->addr + i + j];
						spi_io(cmd_buf, 4, NULL, 0, 0);
						delay_ms(8);
					}
					
					if (ERROR_OK != commit())
					{
						pgbar_fini();
						LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION_ADDR), 
								  "program eeprom in byte mode", 
								  ml_tmp->addr + i);
						ret = ERRCODE_FAILURE_OPERATION;
						goto leave_program_mode;
					}
				}
				
				pgbar_update(k);
				len_current_list -= k;
				if (len_current_list >= page_size)
				{
					k = page_size;
				}
				else
				{
					k = len_current_list;
				}
			}
			
			ml_tmp = MEMLIST_GetNext(ml_tmp);
		}
		
		pgbar_fini();
		LOG_INFO(_GETTEXT(INFOMSG_PROGRAMMED_SIZE), "eeprom", 
					pi->eeprom_size_valid);
	}
	
	if (operations.read_operations & EEPROM)
	{
		if (operations.verify_operations & EEPROM)
		{
			LOG_INFO(_GETTEXT(INFOMSG_VERIFYING), "eeprom");
		}
		else
		{
			pi->eeprom_size_valid = cur_chip_param.ee_size;
			LOG_INFO(_GETTEXT(INFOMSG_READING), "eeprom");
		}
		pgbar_init("reading eeprom |", "|", 0, pi->eeprom_size_valid, 
				   PROGRESS_STEP, '=');
		
		ml_tmp = pi->eeprom_memlist;
		page_size = 256;
		while (ml_tmp != NULL)
		{
			if ((ml_tmp->addr + ml_tmp->len)
				<= (ml_tmp->addr - (ml_tmp->addr % page_size) + page_size))
			{
				k = ml_tmp->len;
			}
			else
			{
				k = page_size - (ml_tmp->addr % page_size);
			}
			
			len_current_list = (uint32_t)ml_tmp->len;
			for (i = -(int32_t)(ml_tmp->addr % page_size); 
				 i < ((int32_t)ml_tmp->len - (int32_t)(ml_tmp->addr % page_size)); 
				 i += page_size)
			{
				for (j = 0; j < page_size; j++)
				{
					cmd_buf[0] = 0xA0;
					cmd_buf[1] = (uint8_t)((ml_tmp->addr + i + j) >> 8);
					cmd_buf[2] = (uint8_t)((ml_tmp->addr + i + j) >> 0);
					cmd_buf[3] = 0;
					spi_io(cmd_buf, 4, page_buf + j, 3, 1);
				}
				
				if (ERROR_OK != commit())
				{
					pgbar_fini();
					LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION_ADDR), 
								  "read eeprom in byte mode", ml_tmp->addr + i);
						ret = ERRCODE_FAILURE_OPERATION;
					goto leave_program_mode;
				}
				
				for (j = 0; j < page_size; j++)
				{
					if (operations.verify_operations & EEPROM)
					{
						if (page_buf[j] != pi->eeprom[ml_tmp->addr + i + j])
						{
							pgbar_fini();
							LOG_ERROR(
								_GETTEXT(ERRMSG_FAILURE_VERIFY_TARGET_AT_02X), 
								"eeprom", 
								ml_tmp->addr + i + j, page_buf[j], 
								pi->eeprom[ml_tmp->addr + i + j]);
							ret = ERRCODE_FAILURE_VERIFY_TARGET;
							goto leave_program_mode;
						}
					}
					else
					{
						memcpy(&pi->eeprom[ml_tmp->addr + i], page_buf, 
							   page_size);
					}
				}
				
				pgbar_update(k);
				len_current_list -= k;
				if (len_current_list >= page_size)
				{
					k = page_size;
				}
				else
				{
					k = len_current_list;
				}
			}
			
			ml_tmp = MEMLIST_GetNext(ml_tmp);
		}
		
		pgbar_fini();
		if (operations.verify_operations & EEPROM)
		{
			LOG_INFO(_GETTEXT(INFOMSG_VERIFIED_SIZE), "eeprom", 
					 pi->eeprom_size_valid);
		}
		else
		{
			LOG_INFO(_GETTEXT(INFOMSG_READ), "eeprom");
		}
	}
	
	if (operations.write_operations & FUSE)
	{
		LOG_INFO(_GETTEXT(INFOMSG_PROGRAMMING), "fuse");
		pgbar_init("writing fuse |", "|", 0, 1, PROGRESS_STEP, '=');
		
		// write fuse
		// low bits
		if (cur_chip_param.fuse_size > 0)
		{
			cmd_buf[0] = 0xAC;
			cmd_buf[1] = 0xA0;
			cmd_buf[2] = 0x00;
			cmd_buf[3] = (pi->fuse_value >> 0) & 0xFF;
			spi_io(cmd_buf, 4, NULL, 0, 0);
			delay_ms(5);
		}
		// high bits
		if (cur_chip_param.fuse_size > 1)
		{
			cmd_buf[0] = 0xAC;
			cmd_buf[1] = 0xA8;
			cmd_buf[2] = 0x00;
			cmd_buf[3] = (pi->fuse_value >> 8) & 0xFF;
			spi_io(cmd_buf, 4, NULL, 0, 0);
			delay_ms(5);
		}
		// extended bits
		if (cur_chip_param.fuse_size > 2)
		{
			cmd_buf[0] = 0xAC;
			cmd_buf[1] = 0xA4;
			cmd_buf[2] = 0x00;
			cmd_buf[3] = (pi->fuse_value >> 16) & 0xFF;
			spi_io(cmd_buf, 4, NULL, 0, 0);
			delay_ms(5);
		}
		if (cur_chip_param.fuse_size> 0)
		{
			if (ERROR_OK != commit())
			{
				pgbar_fini();
				LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), "write fuse");
				ret = ERRCODE_FAILURE_OPERATION;
				goto leave_program_mode;
			}
		}
		else
		{
			pgbar_fini();
			LOG_ERROR(_GETTEXT(ERRMSG_NOT_SUPPORT_BY), "fuse", 
						cur_chip_param.chip_name);
			ret = ERRCODE_NOT_SUPPORT;
			goto leave_program_mode;
		}
		pgbar_update(1);
		
		pgbar_fini();
		LOG_INFO(_GETTEXT(INFOMSG_PROGRAMMED), "fuse");
	}
	
	if (operations.read_operations & FUSE)
	{
		if (operations.verify_operations & FUSE)
		{
			LOG_INFO(_GETTEXT(INFOMSG_VERIFYING), "fuse");
		}
		else
		{
			LOG_INFO(_GETTEXT(INFOMSG_READING), "fuse");
		}
		pgbar_init("reading fuse |", "|", 0, 1, PROGRESS_STEP, '=');
		
		memset(data_tmp, 0, sizeof(data_tmp));
		// read fuse
		// low bits
		if (cur_chip_param.fuse_size > 0)
		{
			cmd_buf[0] = 0x50;
			cmd_buf[1] = 0x00;
			cmd_buf[2] = 0x00;
			cmd_buf[3] = 0x00;
			spi_io(cmd_buf, 4, &data_tmp[0], 3, 1);
		}
		// high bits
		if (cur_chip_param.fuse_size > 1)
		{
			cmd_buf[0] = 0x58;
			cmd_buf[1] = 0x08;
			cmd_buf[2] = 0x00;
			cmd_buf[3] = 0x00;
			spi_io(cmd_buf, 4, &data_tmp[1], 3, 1);
		}
		// extended bits
		if (cur_chip_param.fuse_size > 2)
		{
			cmd_buf[0] = 0x50;
			cmd_buf[1] = 0x08;
			cmd_buf[2] = 0x00;
			cmd_buf[3] = 0x00;
			spi_io(cmd_buf, 4, &data_tmp[2], 3, 1);
		}
		if (cur_chip_param.fuse_size > 0)
		{
			if (ERROR_OK != commit())
			{
				pgbar_fini();
				LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), "read fuse");
				ret = ERRCODE_FAILURE_OPERATION;
				goto leave_program_mode;
			}
		}
		else
		{
			pgbar_fini();
			LOG_ERROR(_GETTEXT(ERRMSG_NOT_SUPPORT_BY), "fuse", 
						cur_chip_param.chip_name);
			ret = ERRCODE_NOT_SUPPORT;
			goto leave_program_mode;
		}
		pgbar_update(1);
		
		pgbar_fini();
		i = (uint32_t)(data_tmp[0] + (data_tmp[1] << 8) 
								   + (data_tmp[2] << 16));
		if (operations.verify_operations & FUSE)
		{
			if ((uint32_t)i == pi->fuse_value)
			{
				LOG_INFO(_GETTEXT(INFOMSG_VERIFIED), "fuse");
			}
			else
			{
				if (cur_chip_param.fuse_size > 2)
				{
					LOG_INFO(_GETTEXT(ERRMSG_FAILURE_VERIFY_TARGET_06X), 
							 "fuse", i, pi->fuse_value);
				}
				else if (cur_chip_param.fuse_size > 1)
				{
					LOG_INFO(_GETTEXT(ERRMSG_FAILURE_VERIFY_TARGET_04X), 
							 "fuse", i, pi->fuse_value);
				}
				else if (cur_chip_param.fuse_size > 0)
				{
					LOG_INFO(_GETTEXT(ERRMSG_FAILURE_VERIFY_TARGET_02X), 
							 "fuse", i, pi->fuse_value);
				}
			}
		}
		else
		{
			if (cur_chip_param.fuse_size > 2)
			{
				LOG_INFO(_GETTEXT(INFOMSG_READ_VALUE_06X), "fuse", i);
			}
			else if (cur_chip_param.fuse_size > 1)
			{
				LOG_INFO(_GETTEXT(INFOMSG_READ_VALUE_04X), "fuse", i);
			}
			else if (cur_chip_param.fuse_size > 0)
			{
				LOG_INFO(_GETTEXT(INFOMSG_READ_VALUE_02X), "fuse", i);
			}
		}
	}
	
	if (operations.write_operations & LOCK)
	{
		LOG_INFO(_GETTEXT(INFOMSG_PROGRAMMING), "lock");
		pgbar_init("writing lock |", "|", 0, 1, PROGRESS_STEP, '=');
		
		// write lock
		if (cur_chip_param.lock_size > 0)
		{
			cmd_buf[0] = 0xAC;
			cmd_buf[1] = 0xE0;
			cmd_buf[2] = 0x00;
			cmd_buf[3] = (pi->lock_value >> 0) & 0xFF;
			spi_io(cmd_buf, 4, NULL, 0, 0);
			delay_ms(5);
			if (ERROR_OK != commit())
			{
				pgbar_fini();
				LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), "write lock");
				ret = ERRCODE_FAILURE_OPERATION;
				goto leave_program_mode;
			}
		}
		else
		{
			pgbar_fini();
			LOG_ERROR(_GETTEXT(ERRMSG_NOT_SUPPORT_BY), "lock", 
						cur_chip_param.chip_name);
			ret = ERRCODE_NOT_SUPPORT;
			goto leave_program_mode;
		}
		pgbar_update(1);
		
		pgbar_fini();
		LOG_INFO(_GETTEXT(INFOMSG_PROGRAMMED), "lock");
	}
	
	if (operations.read_operations & LOCK)
	{
		if (operations.verify_operations & LOCK)
		{
			LOG_INFO(_GETTEXT(INFOMSG_VERIFYING), "lock");
		}
		else
		{
			LOG_INFO(_GETTEXT(INFOMSG_READING), "lock");
		}
		pgbar_init("reading lock |", "|", 0, 1, PROGRESS_STEP, '=');
		
		memset(data_tmp, 0, sizeof(data_tmp));
		// read lock
		if (cur_chip_param.lock_size > 0)
		{
			cmd_buf[0] = 0x58;
			cmd_buf[1] = 0x00;
			cmd_buf[2] = 0x00;
			cmd_buf[3] = 0x00;
			spi_io(cmd_buf, 4, &data_tmp[0], 3, 1);
			if (ERROR_OK != commit())
			{
				pgbar_fini();
				LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), "read lock");
				ret = ERRCODE_FAILURE_OPERATION;
				goto leave_program_mode;
			}
		}
		else
		{
			pgbar_fini();
			LOG_ERROR(_GETTEXT(ERRMSG_NOT_SUPPORT_BY), "lock", 
						cur_chip_param.chip_name);
			ret = ERRCODE_NOT_SUPPORT;
			goto leave_program_mode;
		}
		pgbar_update(1);
		
		pgbar_fini();
		if (operations.verify_operations & LOCK)
		{
			if (data_tmp[0] == (uint8_t)pi->lock_value)
			{
				LOG_INFO(_GETTEXT(INFOMSG_VERIFIED), "lock");
			}
			else
			{
				LOG_INFO(_GETTEXT(ERRMSG_FAILURE_VERIFY_TARGET_02X), 
						 "lock", data_tmp[0], pi->lock_value);
			}
		}
		else
		{
			LOG_INFO(_GETTEXT(INFOMSG_READ_VALUE_02X), "lock", data_tmp[0]);
		}
	}
	
	if (operations.read_operations & CALIBRATION)
	{
		LOG_INFO(_GETTEXT(INFOMSG_READING), "calibration");
		pgbar_init("reading calibration |", "|", 0, 1, PROGRESS_STEP, '=');
		
		memset(data_tmp, 0, sizeof(data_tmp));
		// read calibration
		if (cur_chip_param.cali_size > 0)
		{
			cmd_buf[0] = 0x38;
			cmd_buf[1] = 0x00;
			cmd_buf[2] = 0x00;
			cmd_buf[3] = 0x00;
			spi_io(cmd_buf, 4, &data_tmp[0], 3, 1);
		}
		if (cur_chip_param.cali_size > 1)
		{
			cmd_buf[0] = 0x38;
			cmd_buf[1] = 0x00;
			cmd_buf[2] = 0x01;
			cmd_buf[3] = 0x00;
			spi_io(cmd_buf, 4, &data_tmp[1], 3, 1);
		}
		if (cur_chip_param.cali_size > 2)
		{
			cmd_buf[0] = 0x38;
			cmd_buf[1] = 0x00;
			cmd_buf[2] = 0x02;
			cmd_buf[3] = 0x00;
			spi_io(cmd_buf, 4, &data_tmp[2], 3, 1);
		}
		if (cur_chip_param.cali_size > 3)
		{
			cmd_buf[0] = 0x38;
			cmd_buf[1] = 0x00;
			cmd_buf[2] = 0x03;
			cmd_buf[3] = 0x00;
			spi_io(cmd_buf, 4, &data_tmp[3], 3, 1);
		}
		if (cur_chip_param.cali_size > 0)
		{
			if (ERROR_OK != commit())
			{
				pgbar_fini();
				LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), "read calibration");
				ret = ERRCODE_FAILURE_OPERATION;
				goto leave_program_mode;
			}
		}
		else
		{
			pgbar_fini();
			LOG_ERROR(_GETTEXT(ERRMSG_NOT_SUPPORT_BY), "calibration", 
						cur_chip_param.chip_name);
			ret = ERRCODE_NOT_SUPPORT;
			goto leave_program_mode;
		}
		pgbar_update(1);
		
		pgbar_fini();
		i = (uint32_t)(data_tmp[0] + (data_tmp[1] << 8) 
					+ (data_tmp[2] << 16) + (data_tmp[3] << 24));
		if (cur_chip_param.fuse_size > 3)
		{
			LOG_INFO(_GETTEXT(INFOMSG_READ_VALUE_08X), "calibration", i);
		}
		else if (cur_chip_param.fuse_size > 2)
		{
			LOG_INFO(_GETTEXT(INFOMSG_READ_VALUE_06X), "calibration", i);
		}
		else if (cur_chip_param.fuse_size > 1)
		{
			LOG_INFO(_GETTEXT(INFOMSG_READ_VALUE_04X), "calibration", i);
		}
		else if (cur_chip_param.fuse_size > 0)
		{
			LOG_INFO(_GETTEXT(INFOMSG_READ_VALUE_02X), "calibration", i);
		}
	}
	
leave_program_mode:
	// leave program mode
	reset_input();
	reset_fini();
	spi_fini();
	if (ERROR_OK != commit())
	{
		LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), 
				  "exit program mode");
		ret = ERRCODE_FAILURE_OPERATION;
	}
	
	return ret;
}

