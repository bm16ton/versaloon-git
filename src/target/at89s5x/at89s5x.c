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

#include "at89s5x.h"
#include "at89s5x_internal.h"

#define CUR_TARGET_STRING			S5X_STRING
#define CUR_DEFAULT_FREQ			S5X_DEFAULT_FREQ
#define cur_chip_param				target_chip_param
#define cur_chips_param				target_chips.chips_param
#define cur_chips_num				target_chips.num_of_chips
#define cur_flash_offset			s5x_flash_offset
#define cur_prog_mode				program_mode
#define cur_target_defined			target_defined
#define cur_program_area_map		s5x_program_area_map

const struct program_area_map_t s5x_program_area_map[] = 
{
	{APPLICATION_CHAR, 1, 0, 0, 0, 0},
	{LOCK_CHAR, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

static uint8_t s5x_lock = 0;
static uint8_t s5x_fuse = 0;

static uint16_t s5x_flash_offset = 0;

static uint16_t s5x_byte_delay_us = 500;

void s5x_usage(void)
{
	printf("\
Usage of %s:\n\
  -F,  --frequency <FREQUENCY>              set ISP frequency, in KHz\n\
  -l,  --lock <LOCK>                        set lock(1..4)\n\
  -f,  --fuse <FUSE>                        set fuse\n\
  -m,  --mode <MODE>                        set program mode<b|p>\n\n", 
			CUR_TARGET_STRING);
}

RESULT s5x_parse_argument(char cmd, const char *argu)
{
	switch(cmd)
	{
	case 'h':
		s5x_usage();
		break;
	case 'w':
		// set Wait time
		if (NULL == argu)
		{
			LOG_ERROR(_GETTEXT(ERRMSG_INVALID_OPTION), cmd);
			return ERRCODE_INVALID_OPTION;
		}
		
		s5x_byte_delay_us = (uint16_t)strtoul(argu, NULL, 0);
		if (s5x_byte_delay_us & 0x8000)
		{
			LOG_ERROR(_GETTEXT(ERRMSG_INVALID_OPTION), cmd);
			return ERRCODE_INVALID_OPTION;
		}
		
		break;
	case 'l':
		// define Lock
		if (NULL == argu)
		{
			LOG_ERROR(_GETTEXT(ERRMSG_INVALID_OPTION), cmd);
			return ERRCODE_INVALID_OPTION;
		}
		
		s5x_lock = (uint8_t)strtoul(argu, NULL, 0);
		if ((s5x_lock < 1) || (s5x_lock > 4))
		{
			LOG_ERROR(_GETTEXT(ERRMSG_INVALID_OPTION), cmd);
			return ERRCODE_INVALID_OPTION;
		}
		s5x_lock--;
		cur_target_defined |= LOCK;
		
		break;
	case 'f':
		// define Fuse
		if (NULL == argu)
		{
			LOG_ERROR(_GETTEXT(ERRMSG_INVALID_OPTION), cmd);
			return ERRCODE_INVALID_OPTION;
		}
		
		s5x_fuse = (uint8_t)strtoul(argu, NULL, 0);
		if (s5x_fuse > 0x0F)
		{
			LOG_ERROR(_GETTEXT(ERRMSG_INVALID_HEX_MESSAGE), s5x_fuse, 
					  "s5x fuse", "MUST be <= 0x0F!!");
			return ERROR_FAIL;
		}
		cur_target_defined |= FUSE;
		
		break;
	default:
		return ERROR_FAIL;
		break;
	}

	return ERROR_OK;
}

RESULT s5x_prepare_buffer(struct program_info_t *pi)
{
	if (pi->program_areas[APPLICATION_IDX].buff != NULL)
	{
		memset(pi->program_areas[APPLICATION_IDX].buff, S5X_FLASH_CHAR, 
				pi->program_areas[APPLICATION_IDX].size);
	}
	else
	{
		return ERROR_FAIL;
	}
	
	pi->program_areas[LOCK_IDX].value = s5x_lock;
	pi->program_areas[FUSE_IDX].value = s5x_fuse;
	
	return ERROR_OK;
}

RESULT s5x_write_buffer_from_file_callback(uint32_t address, uint32_t seg_addr, 
								uint8_t* data, uint32_t length, void* buffer)
{
	struct program_info_t *pi = (struct program_info_t *)buffer;
	uint32_t mem_addr = address & 0x0000FFFF, page_size;
	RESULT ret;
	uint8_t *tbuff;
	struct chip_area_info_t *areas;
	
#ifdef PARAM_CHECK
	if ((length > 0) && (NULL == data))
	{
		LOG_ERROR(_GETTEXT(ERRMSG_INVALID_PARAMETER), __FUNCTION__);
		return ERRCODE_INVALID_PARAMETER;
	}
#endif
	
	if (seg_addr != 0)
	{
		LOG_ERROR(_GETTEXT(ERRMSG_NOT_SUPPORT_BY), 
				  "segment address", CUR_TARGET_STRING);
		return ERRCODE_NOT_SUPPORT;
	}
	
	areas = cur_chip_param.chip_areas;
	// flash from 0x00000000
	switch (address >> 16)
	{
	case 0x0000:
		tbuff = pi->program_areas[APPLICATION_IDX].buff;
		if (NULL == tbuff)
		{
			LOG_ERROR(_GETTEXT(ERRMSG_INVALID_BUFFER), "pi->app");
			return ERRCODE_INVALID_BUFFER;
		}
		
		if ((0 == areas[APPLICATION_IDX].page_num) 
			|| (0 == areas[APPLICATION_IDX].page_size))
		{
			LOG_ERROR(_GETTEXT(ERRMSG_INVALID), "Flash", 
					  cur_chip_param.chip_name);
			return ERRCODE_INVALID;
		}
		
		mem_addr += cur_flash_offset;
		if ((mem_addr >= areas[APPLICATION_IDX].size) 
			|| (length > areas[APPLICATION_IDX].size) 
			|| ((mem_addr + length) > areas[APPLICATION_IDX].size))
		{
			LOG_ERROR(_GETTEXT(ERRMSG_INVALID_RANGE), "flash memory");
			return ERRCODE_INVALID;
		}
		cur_target_defined |= APPLICATION;
		
		memcpy(tbuff + mem_addr, data, length);
		
		if (cur_prog_mode & S5X_PAGE_MODE)
		{
			page_size = areas[APPLICATION_IDX].page_size;
		}
		else
		{
			// use 256 bytes buffer for byte mode
			page_size = 256;
		}
		
		ret = MEMLIST_Add(&pi->program_areas[APPLICATION_IDX].memlist, 
								mem_addr, length, page_size);
		if (ret != ERROR_OK)
		{
			LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), "add memory list");
			return ERRCODE_FAILURE_OPERATION;
		}
		break;
	default:
		LOG_ERROR(_GETTEXT(ERRMSG_INVALID_ADDRESS), address, CUR_TARGET_STRING);
		return ERRCODE_INVALID;
		break;
	}
	
	return ERROR_OK;
}

RESULT s5x_fini(struct program_info_t *pi, struct programmer_info_t *prog)
{
	pi = pi;
	prog = prog;
	
	return ERROR_OK;
}

RESULT s5x_init(struct program_info_t *pi, struct programmer_info_t *prog)
{
	uint8_t i;
	struct operation_t opt_tmp;
	
	memset(&opt_tmp, 0, sizeof(opt_tmp));
	
	if (strcmp(pi->chip_type, CUR_TARGET_STRING))
	{
		LOG_BUG(_GETTEXT(ERRMSG_INVALID_HANDLER), CUR_TARGET_STRING, 
				pi->chip_type);
		return ERRCODE_INVALID_HANDLER;
	}
	
	if (NULL == pi->chip_name)
	{
		// auto detect
		LOG_INFO(_GETTEXT(INFOMSG_TRY_AUTODETECT));
		opt_tmp.read_operations = CHIPID;
		cur_chip_param.param[S5X_PARAM_PE_OUT] = 0x69;
		
		if (ERROR_OK != s5x_program(opt_tmp, pi, prog))
		{
			LOG_ERROR(_GETTEXT(ERRMSG_AUTODETECT_FAIL), pi->chip_type);
			return ERRCODE_AUTODETECT_FAIL;
		}
		
		LOG_INFO(_GETTEXT(INFOMSG_AUTODETECT_SIGNATURE), pi->chip_id);
		for (i = 0; i < cur_chips_num; i++)
		{
			if (pi->chip_id == cur_chips_param[i].chip_id)
			{
				pi->chip_name = (char *)cur_chips_param[i].chip_name;
				LOG_INFO(_GETTEXT(INFOMSG_CHIP_FOUND), pi->chip_name);
				
				goto Post_Init;
			}
		}
		
		LOG_ERROR(_GETTEXT(ERRMSG_AUTODETECT_FAIL), pi->chip_type);
		return ERRCODE_AUTODETECT_FAIL;
	}
	else
	{
		for (i = 0; i < cur_chips_num; i++)
		{
			if (!strcmp(cur_chips_param[i].chip_name, pi->chip_name))
			{
				goto Post_Init;
			}
		}
		
		return ERROR_FAIL;
	}
Post_Init:
	memcpy(&cur_chip_param, cur_chips_param + i, sizeof(cur_chip_param));
	
	pi->program_areas[APPLICATION_IDX].size = 
				cur_chip_param.chip_areas[APPLICATION_IDX].size;
	
	return ERROR_OK;
}

uint32_t s5x_interface_needed(void)
{
	return S5X_INTERFACE_NEEDED;
}

RESULT s5x_get_mass_product_data_size(struct operation_t operations, 
									  struct program_info_t pi, uint32_t *size)
{
	operations = operations;
	pi = pi;
	size = size;
	
	return ERRCODE_NOT_SUPPORT;
}

RESULT s5x_prepare_mass_product_data(struct operation_t operations, 
									 struct program_info_t pi, uint8_t *buff)
{
	operations = operations;
	pi = pi;
	buff = buff;
	
	return ERRCODE_NOT_SUPPORT;
}








RESULT s5x_program(struct operation_t operations, struct program_info_t *pi, 
				   struct programmer_info_t *prog)
{
#define get_target_voltage(v)	prog->get_target_voltage(v)

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
#define reset_set()				prog->gpio_out(GPIO_SRST, GPIO_SRST)
#define reset_clr()				reset_input()

#define delay_ms(ms)			prog->delayms((ms) | 0x8000)
#define delay_us(us)			prog->delayus((us) & 0x7FFF)

#define commit()				prog->peripheral_commit()

	uint16_t voltage;
	uint8_t cmd_buf[4];
	uint8_t poll_value, tmp8;
	int32_t i;
	uint32_t j, k, len_current_list, page_size;
	uint8_t page_buf[256];
	RESULT ret = ERROR_OK;
	struct memlist *ml_tmp;
	uint32_t target_size;
	uint8_t *tbuff;
	struct memlist **ml;

	pi = pi;
	
	if (!program_frequency)
	{
		program_frequency = CUR_DEFAULT_FREQ;
	}

#ifdef PARAM_CHECK
	if (NULL == prog)
	{
		LOG_BUG(_GETTEXT(ERRMSG_INVALID_PARAMETER), __FUNCTION__);
		return ERRCODE_INVALID_PARAMETER;
	}
	if ((   (operations.read_operations & APPLICATION) 
			&& (NULL == pi->program_areas[APPLICATION_IDX].buff)) 
		|| ((   (operations.write_operations & APPLICATION) 
				|| (operations.verify_operations & APPLICATION)) 
			&& (NULL == pi->program_areas[APPLICATION_IDX].buff)))
	{
		LOG_ERROR(_GETTEXT(ERRMSG_INVALID_BUFFER), "for flash");
		return ERRCODE_INVALID_BUFFER;
	}
	if ((   (operations.write_operations & LOCK) 
			|| (operations.verify_operations & LOCK)) 
		&& (pi->program_areas[LOCK_IDX].value > 3))
	{
		LOG_ERROR(_GETTEXT(ERRMSG_INVALID_VALUE), 
					pi->program_areas[LOCK_IDX].value, "lock_value");
		return ERRCODE_INVALID;
	}
#endif
	
	// get target voltage
	if (ERROR_OK != get_target_voltage(&voltage))
	{
		return ERROR_FAIL;
	}
	LOG_DEBUG(_GETTEXT(INFOMSG_TARGET_VOLTAGE), voltage / 1000.0);
	if (voltage < 2700)
	{
		LOG_WARNING(_GETTEXT(INFOMSG_TARGET_LOW_POWER));
	}
	
	// check mode
	if ((operations.write_operations & APPLICATION) 
		|| (operations.verify_operations & APPLICATION) 
		|| (operations.read_operations & APPLICATION))
	{
		switch (cur_prog_mode & S5X_MODE_MASK)
		{
		case 0:
			LOG_WARNING(_GETTEXT(INFOMSG_USE_DEFAULT), "Program mode", 
						"page mode");
			cur_prog_mode = S5X_PAGE_MODE;
		case S5X_PAGE_MODE:
			if (!(cur_chip_param.program_mode & S5X_PAGE_MODE))
			{
				LOG_ERROR(_GETTEXT(ERRMSG_NOT_SUPPORT_BY), "page mode", 
					  cur_chip_param.chip_name);
				return ERRCODE_NOT_SUPPORT;
			}
			break;
		case S5X_BYTE_MODE:
			if (!(cur_chip_param.program_mode & S5X_BYTE_MODE))
			{
				LOG_ERROR(_GETTEXT(ERRMSG_NOT_SUPPORT_BY), "byte mode", 
					  cur_chip_param.chip_name);
				return ERRCODE_NOT_SUPPORT;
			}
			break;
		default:
			LOG_ERROR(_GETTEXT(ERRMSG_INVALID_PROG_MODE), cur_prog_mode, 
					  CUR_TARGET_STRING);
			return ERRCODE_INVALID_PROG_MODE;
			break;
		}
	}
	
	// here we go
	// init
	spi_init();
	reset_init();
	
	// enter program mode
	if (program_frequency > 0)
	{
		// use program_frequency
		spi_conf(program_frequency);
		
		// toggle reset
		reset_set();
		reset_output();
		delay_ms(100);
		reset_input();
		delay_ms(30);
		reset_set();
		reset_output();
		delay_ms(10);
		
		// enter into program mode command
		cmd_buf[0] = 0xAC;
		cmd_buf[1] = 0x53;
		cmd_buf[2] = 0x00;
		cmd_buf[3] = 0x00;
		poll_value = 0;
		// ret[3] should be 0x69
		spi_io(cmd_buf, 4, &poll_value, 3, 1);
		if ((ERROR_OK != commit()) 
			|| ((cur_chip_param.param[S5X_PARAM_PE_OUT] != 0xFF) 
				&& (cur_chip_param.param[S5X_PARAM_PE_OUT] != poll_value)))
		{
			LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_ENTER_PROG_MODE));
			ret = ERRCODE_FAILURE_ENTER_PROG_MODE;
			goto leave_program_mode;
		}
	}
	else
	{
		// TODO: auto check frequency
		LOG_ERROR(_GETTEXT(ERRMSG_INVALID_VALUE), 
				  program_frequency, "ISP frequency");
		ret = ERRCODE_INVALID;
		goto leave_program_mode;
	}
	
	// read chip_id
	cmd_buf[0] = 0x28;
	cmd_buf[1] = 0x00;
	cmd_buf[2] = 0x00;
	cmd_buf[3] = 0x00;
	spi_io(cmd_buf, 4, &page_buf[2], 3, 1);
	cmd_buf[0] = 0x28;
	cmd_buf[1] = 0x01;
	cmd_buf[2] = 0x00;
	cmd_buf[3] = 0x00;
	spi_io(cmd_buf, 4, &page_buf[1], 3, 1);
	cmd_buf[0] = 0x28;
	cmd_buf[1] = 0x02;
	cmd_buf[2] = 0x00;
	cmd_buf[3] = 0x00;
	spi_io(cmd_buf, 4, &page_buf[0], 3, 1);
	if (ERROR_OK != commit())
	{
		LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), "read signature");
		ret = ERRCODE_FAILURE_OPERATION;
		goto leave_program_mode;
	}
	pi->chip_id = ((page_buf[2] & 0xFF) << 0) | ((page_buf[1] & 0xFF) << 8) 
				   | ((page_buf[0] & 0xFF) << 16);
	LOG_INFO(_GETTEXT(INFOMSG_TARGET_CHIP_ID), pi->chip_id);
	if (!(operations.read_operations & CHIPID))
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
		delay_ms(500);
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
	
	if (cur_prog_mode & S5X_PAGE_MODE)
	{
		page_size = cur_chip_param.chip_areas[APPLICATION_IDX].page_size;
	}
	else
	{
		page_size = 256;
	}
	
	ml = &pi->program_areas[APPLICATION_IDX].memlist;
	target_size = MEMLIST_CalcAllSize(*ml);
	tbuff = pi->program_areas[APPLICATION_IDX].buff;
	if (operations.write_operations & APPLICATION)
	{
		// program
		LOG_INFO(_GETTEXT(INFOMSG_PROGRAMMING), "flash");
		pgbar_init("writing flash |", "|", 0, target_size, PROGRESS_STEP, '=');
		
		ml_tmp = *ml;
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
				 i < ((int32_t)ml_tmp->len 
						- (int32_t)(ml_tmp->addr % page_size)); 
				 i += page_size)
			{
				if (cur_prog_mode & S5X_PAGE_MODE)
				{
					// Page Mode
					cmd_buf[0] = 0x50;
					cmd_buf[1] = ((ml_tmp->addr + i) >> 8) & 0xFF;
					if (page_size == 256)
					{
						spi_io(cmd_buf, 2, NULL, 0, 0);
					}
					else
					{
						cmd_buf[2] = (ml_tmp->addr + i) & 0xFF;
						spi_io(cmd_buf, 3, NULL, 0, 0);
					}
					
					for (j = 0; j < page_size; j++)
					{
						spi_io(&tbuff[ml_tmp->addr + i + j], 1, NULL, 0, 0);
						delay_us(s5x_byte_delay_us);
					}
					
					if (ERROR_OK != commit())
					{
						pgbar_fini();
						LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION_ADDR), 
								"program flash in page mode", ml_tmp->addr + i);
						ret = ERRCODE_FAILURE_OPERATION;
						goto leave_program_mode;
					}
				}
				else
				{
					// Byte Mode
					for (j = 0; j < page_size; j++)
					{
						cmd_buf[0] = 0x40;
						cmd_buf[1] = (uint8_t)((ml_tmp->addr + i + j) >> 8);
						cmd_buf[2] = (uint8_t)((ml_tmp->addr + i + j) >> 0);
						cmd_buf[3] = tbuff[ml_tmp->addr + i + j];
						spi_io(cmd_buf, 4, NULL, 0, 0);
						delay_us(s5x_byte_delay_us);
					}
					
					if (ERROR_OK != commit())
					{
						pgbar_fini();
						LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION_ADDR), 
								"program flash in byte mode", ml_tmp->addr + i);
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
		LOG_INFO(_GETTEXT(INFOMSG_PROGRAMMED_SIZE), "flash", target_size);
	}
	
	if ((operations.read_operations & APPLICATION) 
		|| (operations.verify_operations & APPLICATION))
	{
		if (operations.verify_operations & APPLICATION)
		{
			LOG_INFO(_GETTEXT(INFOMSG_VERIFYING), "flash");
		}
		else
		{
			ret = MEMLIST_Add(ml, 0, pi->program_areas[APPLICATION_IDX].size, 
								page_size);
			if (ret != ERROR_OK)
			{
				LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), "add memory list");
				return ERRCODE_FAILURE_OPERATION;
			}
			target_size = MEMLIST_CalcAllSize(*ml);
			LOG_INFO(_GETTEXT(INFOMSG_READING), "flash");
		}
		pgbar_init("reading flash |", "|", 0, target_size, PROGRESS_STEP, '=');
		
		ml_tmp = *ml;
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
				 i < ((int32_t)ml_tmp->len 
						- (int32_t)(ml_tmp->addr % page_size)); 
				 i += page_size)
			{
				if (cur_prog_mode & S5X_PAGE_MODE)
				{
					// Page Mode
					cmd_buf[0] = 0x30;
					cmd_buf[1] = ((ml_tmp->addr + i) >> 8) & 0xFF;
					if (page_size == 256)
					{
						spi_io(cmd_buf, 2, NULL, 0, 0);
					}
					else
					{
						cmd_buf[2] = (ml_tmp->addr + i) & 0xFF;
						spi_io(cmd_buf, 3, NULL, 0, 0);
					}
					
					memset(page_buf, 0, page_size);
					spi_io(page_buf, (uint16_t)page_size, page_buf, 0, 
						   (uint16_t)page_size);
					
					if (ERROR_OK != commit())
					{
						pgbar_fini();
						LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION_ADDR), 
								  "read flash in page mode", ml_tmp->addr + i);
						ret = ERRCODE_FAILURE_OPERATION;
						goto leave_program_mode;
					}
				}
				else
				{
					// Byte Mode
					for (j = 0; j < page_size; j++)
					{
						cmd_buf[0] = 0x20;
						cmd_buf[1] = (uint8_t)((ml_tmp->addr + i + j) >> 8);
						cmd_buf[2] = (uint8_t)((ml_tmp->addr + i + j) >> 0);
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
				}
				
				if (operations.verify_operations & APPLICATION)
				{
					for (j = 0; j < page_size; j++)
					{
						if (page_buf[j] != tbuff[ml_tmp->addr + i + j])
						{
							pgbar_fini();
							LOG_ERROR(
								_GETTEXT(ERRMSG_FAILURE_VERIFY_TARGET_AT_02X), 
								"flash", ml_tmp->addr + i + j, page_buf[j], 
								tbuff[ml_tmp->addr + i + j]);
							ret = ERROR_FAIL;
							goto leave_program_mode;
						}
					}
				}
				else
				{
					memcpy(&tbuff[ml_tmp->addr + i], page_buf, page_size);
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
			LOG_INFO(_GETTEXT(INFOMSG_VERIFIED_SIZE), "flash", target_size);
		}
		else
		{
			LOG_INFO(_GETTEXT(INFOMSG_READ), "flash");
		}
	}
	
	if (operations.write_operations & FUSE)
	{
		if (0 == cur_chip_param.chip_areas[FUSE_IDX].size)
		{
			LOG_ERROR(_GETTEXT(ERRMSG_NOT_SUPPORT_BY), "Fuse", 
					  cur_chip_param.chip_name);
			ret = ERRCODE_NOT_SUPPORT;
			goto leave_program_mode;
		}
		
		LOG_INFO(_GETTEXT(INFOMSG_PROGRAMMING), "fuse");
		pgbar_init("writing fuse |", "|", 0, 1, PROGRESS_STEP, '=');
		
		cmd_buf[0] = 0xAC;
		cmd_buf[1] = 0x10 + (pi->program_areas[FUSE_IDX].value & 0x0F);
		cmd_buf[2] = 0x00;
		cmd_buf[3] = 0x00;
		spi_io(cmd_buf, 4, NULL, 0, 0);
		delay_ms(100);
		if (ERROR_OK != commit())
		{
			pgbar_fini();
			LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), "program fuse");
			ret = ERRCODE_FAILURE_OPERATION;
			goto leave_program_mode;
		}
		
		pgbar_update(1);
		pgbar_fini();
		LOG_INFO(_GETTEXT(INFOMSG_PROGRAMMED), "fuse");
	}
	
	if ((operations.read_operations & FUSE) 
		|| (operations.verify_operations & FUSE))
	{
		if (0 == cur_chip_param.chip_areas[FUSE_IDX].size)
		{
			LOG_ERROR(_GETTEXT(ERRMSG_NOT_SUPPORT_BY), "Fusebit", 
					  cur_chip_param.chip_name);
			ret = ERRCODE_NOT_SUPPORT;
			goto leave_program_mode;
		}
		
		if (operations.verify_operations & FUSE)
		{
			LOG_INFO(_GETTEXT(INFOMSG_VERIFYING), "fuse");
		}
		else
		{
			LOG_INFO(_GETTEXT(INFOMSG_READING), "fuse");
		}
		pgbar_init("reading fuse |", "|", 0, 1, PROGRESS_STEP, '=');
		
		cmd_buf[0] = 0x21;
		cmd_buf[1] = 0x00;
		cmd_buf[2] = 0x00;
		cmd_buf[3] = 0x00;
		spi_io(cmd_buf, 4, &tmp8, 3, 1);
		if (ERROR_OK != commit())
		{
			pgbar_fini();
			LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), "read fuse");
			ret = ERRCODE_FAILURE_OPERATION;
			goto leave_program_mode;
		}
		
		pgbar_update(1);
		pgbar_fini();
		
		tmp8 &= 0x0F;
		if (operations.verify_operations & FUSE)
		{
			if (tmp8 == pi->program_areas[FUSE_IDX].value)
			{
				LOG_INFO(_GETTEXT(INFOMSG_VERIFIED), "fuse");
			}
			else
			{
				LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_VERIFY_TARGET_02X), 
						  "fuse", tmp8, pi->program_areas[FUSE_IDX].value);
				ret = ERROR_FAIL;
				goto leave_program_mode;
			}
		}
		else
		{
			LOG_INFO(_GETTEXT(INFOMSG_READ_VALUE), "fuse", tmp8);
		}
	}
	
	if (operations.write_operations & LOCK)
	{
		LOG_INFO(_GETTEXT(INFOMSG_PROGRAMMING), "lock");
		pgbar_init("writing lock |", "|", 0, 1, PROGRESS_STEP, '=');
		
		if (pi->program_areas[LOCK_IDX].value > 0)
		{
			for (i = 1; i < 4; i++)
			{
				if (pi->program_areas[LOCK_IDX].value >= (uint32_t)i)
				{
					cmd_buf[0] = 0xAC;
					cmd_buf[1] = 0xE0 + (uint8_t)i;
					cmd_buf[2] = 0x00;
					cmd_buf[3] = 0x00;
					spi_io(cmd_buf, 4, NULL, 0, 0);
					delay_ms(100);
				}
			}
			
			if (ERROR_OK != commit())
			{
				pgbar_fini();
				LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), "program lock");
				ret = ERRCODE_FAILURE_OPERATION;
				goto leave_program_mode;
			}
		}
		
		pgbar_update(1);
		pgbar_fini();
		LOG_INFO(_GETTEXT(INFOMSG_PROGRAMMED), "lock");
	}
	
	if ((operations.read_operations & LOCK) 
		|| (operations.verify_operations & LOCK))
	{
		uint8_t lock;
		
		if (operations.verify_operations & LOCK)
		{
			LOG_INFO(_GETTEXT(INFOMSG_VERIFYING), "lock");
		}
		else
		{
			LOG_INFO(_GETTEXT(INFOMSG_READING), "lock");
		}
		pgbar_init("reading lock |", "|", 0, 1, PROGRESS_STEP, '=');
		
		cmd_buf[0] = 0x24;
		cmd_buf[1] = 0x00;
		cmd_buf[2] = 0x00;
		cmd_buf[3] = 0x00;
		spi_io(cmd_buf, 4, &tmp8, 3, 1);
		if (ERROR_OK != commit())
		{
			pgbar_fini();
			LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), "read lock");
			ret = ERRCODE_FAILURE_OPERATION;
			goto leave_program_mode;
		}
		
		lock = 0;
		for (i = 0; i < 3; i++)
		{
			if (tmp8 & (1 << (i + 2)))
			{
				lock += 1;
			}
		}
		
		pgbar_update(1);
		pgbar_fini();
		
		if (operations.verify_operations & LOCK)
		{
			if (lock == pi->program_areas[LOCK_IDX].value)
			{
				LOG_INFO(_GETTEXT(INFOMSG_VERIFIED), "lock");
			}
			else
			{
				LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_VERIFY_TARGET_D), 
					"lock", lock + 1, pi->program_areas[LOCK_IDX].value + 1);
				ret = ERROR_FAIL;
				goto leave_program_mode;
			}
		}
		else
		{
			LOG_INFO(_GETTEXT(INFOMSG_READ_VALUE), "lock", lock + 1);
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

