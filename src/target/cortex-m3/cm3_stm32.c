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
#include <stdlib.h>

#include "port.h"
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

#include "cm3.h"
#include "cm3_stm32.h"

#include "cm3_internal.h"
#include "stm32_internal.h"

#include "adi_v5p1.h"
#include "cm3_common.h"

#include "timer.h"

#define STM32SWJ_BUFFER_SIZE		512

ENTER_PROGRAM_MODE_HANDLER(stm32swj);
LEAVE_PROGRAM_MODE_HANDLER(stm32swj);
ERASE_TARGET_HANDLER(stm32swj);
WRITE_TARGET_HANDLER(stm32swj);
READ_TARGET_HANDLER(stm32swj);
const struct program_functions_t stm32swj_program_functions = 
{
	NULL, 
	ENTER_PROGRAM_MODE_FUNCNAME(stm32swj), 
	LEAVE_PROGRAM_MODE_FUNCNAME(stm32swj), 
	ERASE_TARGET_FUNCNAME(stm32swj), 
	WRITE_TARGET_FUNCNAME(stm32swj), 
	READ_TARGET_FUNCNAME(stm32swj)
};

RESULT stm32_wait_status_busy(uint32_t *status, uint32_t timeout)
{
	uint32_t reg;
	
	if (ERROR_OK != adi_memap_read_reg(STM32_FLASH_SR, &reg, 1))
	{
		return ERROR_FAIL;
	}
	while ((reg & STM32_FLASH_SR_BSY) && timeout)
	{
		timeout--;
		sleep_ms(1);
		if (ERROR_OK != adi_memap_read_reg(STM32_FLASH_SR, &reg, 1))
		{
			return ERROR_FAIL;
		}
	}
	*status = reg;
	if (reg & (STM32_FLASH_SR_PGERR | STM32_FLASH_SR_WRPRTERR))
	{
		reg = STM32_FLASH_SR_PGERR | STM32_FLASH_SR_WRPRTERR;
		adi_memap_write_reg(STM32_FLASH_SR, &reg, 1);
	}
	
	return ERROR_OK;
}

RESULT stm32_mass_erase(void)
{
	uint32_t reg;
	
	// mass erase flash memory
	reg = STM32_FLASH_CR_MER;
	adi_memap_write_reg(STM32_FLASH_CR, &reg, 0);
	reg = STM32_FLASH_CR_MER | STM32_FLASH_CR_STRT;
	adi_memap_write_reg(STM32_FLASH_CR, &reg, 0);
	
	// wait busy
	if (ERROR_OK != stm32_wait_status_busy(&reg, 10))
	{
		return ERROR_FAIL;
	}
	if (reg & STM32_FLASH_SR_WRPRTERR)
	{
		return ERROR_FAIL;
	}
	else
	{
		return ERROR_OK;
	}
}

ENTER_PROGRAM_MODE_HANDLER(stm32swj)
{
	uint32_t reg;
	struct operation_t *op = context->op;
	
	if ((op->erase_operations & APPLICATION) 
		|| (op->write_operations & APPLICATION))
	{
		reg = STM32_FLASH_UNLOCK_KEY1;
		adi_memap_write_reg(STM32_FLASH_KEYR, &reg, 0);
		reg = STM32_FLASH_UNLOCK_KEY2;
		if (ERROR_OK != adi_memap_write_reg(STM32_FLASH_KEYR, &reg, 1))
		{
			return ERROR_FAIL;
		}
	}
	return ERROR_OK;
}

LEAVE_PROGRAM_MODE_HANDLER(stm32swj)
{
	REFERENCE_PARAMETER(context);
	REFERENCE_PARAMETER(success);
	
	return ERROR_OK;
}

ERASE_TARGET_HANDLER(stm32swj)
{
	RESULT ret= ERROR_OK;
	REFERENCE_PARAMETER(size);
	REFERENCE_PARAMETER(addr);
	REFERENCE_PARAMETER(context);
	
	switch (area)
	{
	case APPLICATION_CHAR:
		// halt target first
		if (ERROR_OK != cm3_dp_halt())
		{
			LOG_ERROR(ERRMSG_FAILURE_OPERATION, "halt stm32");
			ret = ERRCODE_FAILURE_OPERATION;
			break;
		}
		// erase all flash
		if (ERROR_OK != stm32_mass_erase())
		{
			ret = ERRCODE_FAILURE_OPERATION;
			break;
		}
		break;
	default:
		ret = ERROR_FAIL;
		break;
	}
	return ret;
}

WRITE_TARGET_HANDLER(stm32swj)
{
	uint32_t reg, block_size;
	uint32_t cur_run_size, cur_block_size;
	uint32_t start_time, run_time;
	uint8_t update_setting;
#define FL_PARA_ADDR_BASE			\
					(STM32_SRAM_ADDR + sizeof(stm32_fl_code) - 4 * 4)
#define FL_ADDR_RAM_SRC				(FL_PARA_ADDR_BASE + 0)
#define FL_ADDR_FLASH_DEST			(FL_PARA_ADDR_BASE + 4)
#define FL_ADDR_WORD_LENGTH			(FL_PARA_ADDR_BASE + 8)
#define FL_ADDR_RESULT				(FL_PARA_ADDR_BASE + 12)
#define FL_ADDR_DATA				(STM32_SRAM_ADDR + 1024)
	uint8_t stm32_fl_code[] = {
									/* init: */
		0x16, 0x4C,					/* ldr.n	r4, STM32_FLASH_CR */
		0x17, 0x4D,					/* ldr.n	r5, STM32_FLASH_SR */
		0x17, 0x4F,					/* ldr.n	r7, result */
									/* wait_start: */
		0x57, 0xF8, 0x04, 0x3C,		/* ldr.w	r3, [r7, #-4] */
		0x00, 0x2B,					/* cmp		r3, #0 */
		0xFB, 0xD0,					/* beq 		wait_start */
									/* update: */
		0x16, 0x48,					/* ldr.n	r0, ram_ptr */
		0x16, 0x49,					/* ldr.n	r1, flash_ptr */
		0x4F, 0xF0, 0x00, 0x06,		/* mov.w	r6, #0 */
		0x16, 0x4A,					/* ldr.n	r2, number_of_words */
									/* write: */
		0x4F, 0xF0, 0x01, 0x03,		/* mov.w	r3, #1 */
		0x23, 0x60,					/* str		r3, [r4, #0] */
		0x30, 0xF8, 0x02, 0x3B,		/* ldrh.w	r3, [r0], #2 */
		0x21, 0xF8, 0x02, 0x3B,		/* strh.w	r3, [r1], #2 */
									/* busy: */
		0x2B, 0x68,					/* ldr 		r3, [r5, #0] */
		0x13, 0xF0, 0x01, 0x0F,		/* tst 		r3, #0x01 */
		0xFB, 0xD0,					/* beq 		busy */
		0x13, 0xF0, 0x14, 0x0F,		/* tst		r3, #0x14 */
		0x0F, 0xD1,					/* bne		exit */
		0x06, 0xF1, 0x01, 0x06,		/* add		r6, r6, #1 */
		0x96, 0x42,					/* cmp		r2, r6 */
		0xED, 0xD3,					/* bcc		write */
		0x13, 0x00,					/* movs		r3, r2 */
		0x3A, 0x60,					/* str		r2, [r7] */
									/* wait_data */
		0x0B, 0x4A,					/* ldr.n	r2, number_of_words */
		0x93, 0x42,					/* cmp		r3, r2 */
		0xFC, 0xD2,					/* bcs.n	wait_data */
		0x12, 0x04,					/* lsls		r2, r2, #16 */
		0xE5, 0xD5,					/* bpl.n	write */
		0x52, 0x00,					/* lsls		r2, r2, #1 */
		0x52, 0x0C,					/* lsrs		r2, r2, #17 */
		0x47, 0xF8, 0x04, 0x2C,		/* str.w	r2, [r7, #-4] */
		0xDC, 0xE7,					/* b		update */
									/* exit: */
		0x6F, 0xF0, 0x00, 0x02,		/* mvn.w	r2, #0 */
		0x3A, 0x60,					/* str		r2, [r7] */
		0xFE, 0xE7,					/* b $ */
		0x10, 0x20, 0x02, 0x40,		/* STM32_FLASH_CR:	.word 0x40022010 */
		0x0C, 0x20, 0x02, 0x40,		/* STM32_FLASH_SR:	.word 0x4002200C */
		0xEC, 0x03, 0x00, 0x20,		/* address of result */
		0x00, 0x00, 0x00, 0x00, 	/* ram address */
		0x00, 0x00, 0x00, 0x00,		/* flash address */
		0x00, 0x00, 0x00, 0x00,		/* number_of_words(2-byte), set to 0 */
		0x00, 0x00, 0x00, 0x00		/* result, set to 0 */
	};
	RESULT ret = ERROR_OK;
	REFERENCE_PARAMETER(context);
	
	switch (area)
	{
	case APPLICATION_CHAR:
		block_size = sizeof(stm32_fl_code);
		// last_but_three dword is RAM address for data, set to 1K at SRAM
		*(uint32_t *)(stm32_fl_code + block_size - 4 * 4) = FL_ADDR_DATA;
		// last_but_four dword is SRAM address of last dword
		*(uint32_t *)(stm32_fl_code + block_size - 4 * 5) = FL_ADDR_RESULT;
		
		// write code to target SRAM
		if (ERROR_OK != adi_memap_write_buf(STM32_SRAM_ADDR, stm32_fl_code, 
												block_size))
		{
			LOG_ERROR(ERRMSG_FAILURE_OPERATION, "load flash_loader to SRAM");
			ret = ERRCODE_FAILURE_OPERATION;
			break;
		}
		reg = STM32_SRAM_ADDR;
		if (ERROR_OK != cm3_write_core_register(CM3_COREREG_PC, &reg))
		{
			LOG_ERROR(ERRMSG_FAILURE_OPERATION, "write PC");
			ret = ERRCODE_FAILURE_OPERATION;
			break;
		}
		if (ERROR_OK != cm3_dp_run())
		{
			LOG_ERROR(ERRMSG_FAILURE_OPERATION, "run flash_loader");
			ret = ERRCODE_FAILURE_OPERATION;
			break;
		}
		
		cur_run_size = 0;
		update_setting = 0;
		while (size)
		{
			if (size > STM32SWJ_BUFFER_SIZE)
			{
				cur_block_size = STM32SWJ_BUFFER_SIZE;
			}
			else
			{
				cur_block_size = size;
			}
			
			// write flash content to FL_PARA_ADDR_DATA
			if (ERROR_OK != adi_memap_write_buf(FL_ADDR_DATA + cur_run_size,
														buff, cur_block_size))
			{
				LOG_ERROR(ERRMSG_FAILURE_OPERATION, "download flash data");
				ret = ERRCODE_FAILURE_OPERATION;
				break;
			}
			
			// write flash address
			if (0 == cur_run_size)
			{
				// first run, update flash address
				adi_memap_write_reg(FL_ADDR_FLASH_DEST, &addr, 0);
			}
			// write word length
			cur_run_size += cur_block_size;
			reg = cur_run_size / 2;
			if (update_setting)
			{
				update_setting = 0;
				// not the first run
				// or the length by 0x8000 to indicate reload addresses
				reg |= 0x8000;
			}
			if (ERROR_OK != 
						adi_memap_write_reg(FL_ADDR_WORD_LENGTH, &reg, 1))
			{
				LOG_ERROR(ERRMSG_FAILURE_OPERATION, 
							"download parameters to flash_loader");
				ret = ERRCODE_FAILURE_OPERATION;
				break;
			}
			
			// wait ready
			if ((cur_run_size >=  STM32_PAGE_SIZE_RW) 
				|| (size <= STM32SWJ_BUFFER_SIZE))
			{
				start_time = get_time_in_ms();
				reg = 0;
				do{
					if (ERROR_OK != 
								adi_memap_read_reg(FL_ADDR_RESULT, &reg, 1))
					{
						LOG_ERROR(ERRMSG_FAILURE_OPERATION, 
									"read result from flash_loader");
						ret = ERRCODE_FAILURE_OPERATION;
						break;
					}
					
					run_time = get_time_in_ms();
					if ((run_time - start_time) > 1000)
					{
						LOG_ERROR(ERRMSG_FAILURE_OPERATION, 
									"wait OK from flash_loader");
						ret = ERRCODE_FAILURE_OPERATION;
						break;
					}
				} while(reg != cur_run_size / 2);
				cur_run_size = 0;
				update_setting = 1;		// need to update settings
			}
			
			size -= cur_block_size;
			addr += cur_block_size;
			buff += cur_block_size;
			pgbar_update(cur_block_size);
		}
		break;
	default:
		ret = ERROR_FAIL;
		break;
	}
	
	return ret;
}

READ_TARGET_HANDLER(stm32swj)
{
	struct program_info_t *pi = context->pi;
	uint32_t mcu_id = 0;
	uint32_t cur_block_size;
	RESULT ret = ERROR_OK;
	
	switch (area)
	{
	case CHIPID_CHAR:
		// read MCU ID at STM32_REG_MCU_ID
		if (ERROR_OK != adi_memap_read_reg(STM32_REG_MCU_ID, &mcu_id, 1))
		{
			ret = ERRCODE_FAILURE_OPERATION;
			break;
		}
		stm32_print_device(mcu_id);
		mcu_id &= STM32_DEN_MSK;
		*(uint32_t *)buff = mcu_id;
		
		// read flash and ram size
		if (ERROR_OK != adi_memap_read_reg(STM32_REG_FLASH_RAM_SIZE, &mcu_id, 1))
		{
			LOG_ERROR(ERRMSG_FAILURE_OPERATION, "read stm32 flash_ram size");
			ret = ERRCODE_FAILURE_OPERATION;
			break;
		}
		if ((mcu_id & 0xFFFF) <= 512)
		{
			pi->program_areas[APPLICATION_IDX].size = (mcu_id & 0xFFFF) * 1024;
			LOG_INFO("Flash memory size: %i KB", mcu_id & 0xFFFF);
		}
		else
		{
			LOG_ERROR(ERRMSG_INVALID_VALUE, mcu_id & 0xFFFF, 
						"stm32 flash size");
			ret = ERRCODE_INVALID;
			break;
		}
		if ((mcu_id >> 16) != 0xFFFF)
		{
			LOG_INFO("SRAM memory size: %i KB", mcu_id >> 16);
		}
		break;
	case APPLICATION_CHAR:
		while (size)
		{
			// cm3_get_max_block_size return size in dword(4-byte)
			cur_block_size = cm3_get_max_block_size(addr);
			if (cur_block_size > (size >> 2))
			{
				cur_block_size = size;
			}
			else
			{
				cur_block_size <<= 2;
			}
			if (ERROR_OK != adi_memap_read_buf(addr, buff, 
												   cur_block_size))
			{
				LOG_ERROR(ERRMSG_FAILURE_OPERATION_ADDR, "write flash block", 
							addr);
				ret = ERRCODE_FAILURE_OPERATION_ADDR;
				break;
			}
			
			size -= cur_block_size;
			addr += cur_block_size;
			buff += cur_block_size;
			pgbar_update(cur_block_size);
		}
		break;
	default:
		ret = ERROR_OK;
		break;
	}
	return ret;
}

