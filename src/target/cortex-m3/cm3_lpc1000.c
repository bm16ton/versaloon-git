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

#include "port.h"
#include "app_cfg.h"
#include "app_type.h"
#include "app_io.h"
#include "app_err.h"
#include "app_log.h"

#include "timer.h"
#include "pgbar.h"

#include "vsprog.h"
#include "programmer.h"
#include "target.h"
#include "scripts.h"

#include "cm3.h"
#include "cm3_lpc1000.h"
#include "lpc1000.h"

#include "cm3_internal.h"
#include "lpc1000_internal.h"

#include "adi_v5p1.h"
#include "cm3_common.h"

#include "timer.h"

ENTER_PROGRAM_MODE_HANDLER(lpc1000swj);
LEAVE_PROGRAM_MODE_HANDLER(lpc1000swj);
ERASE_TARGET_HANDLER(lpc1000swj);
WRITE_TARGET_HANDLER(lpc1000swj);
READ_TARGET_HANDLER(lpc1000swj);
const struct program_functions_t lpc1000swj_program_functions =
{
	NULL,
	ENTER_PROGRAM_MODE_FUNCNAME(lpc1000swj),
	LEAVE_PROGRAM_MODE_FUNCNAME(lpc1000swj),
	ERASE_TARGET_FUNCNAME(lpc1000swj),
	WRITE_TARGET_FUNCNAME(lpc1000swj),
	READ_TARGET_FUNCNAME(lpc1000swj)
};

#define LPC1000_IAP_BASE				LPC1000_SRAM_ADDR
#define LPC1000_IAP_COMMAND_OFFSET		0x80
#define LPC1000_IAP_COMMAND_ADDR		(LPC1000_IAP_BASE + LPC1000_IAP_COMMAND_OFFSET)
#define LPC1000_IAP_RESULT_OFFSET		0x9C
#define LPC1000_IAP_RESULT_ADDR			(LPC1000_IAP_BASE + LPC1000_IAP_RESULT_OFFSET)
#define LPC1000_IAP_ENTRY_OFFSET		0x3C
#define LPC1000_IAP_SYNC_ADDR			(LPC1000_IAP_BASE + 0x98)
#define LPC1000_IAP_CNT_ADDR			(LPC1000_IAP_BASE + 0xB0)
static uint32_t lpc1000swj_iap_cnt = 0;
static uint8_t iap_code[] = {
							// wait_start:
	0x25, 0x4C,				// ldr		r4, [PC, #0x94]		// load sync
	0x00, 0x2C,				// cmp		r4, #0
	0xFC, 0xD0,				// beq 		wait_start
							// init:
	0x0D, 0x4A,				// ldr		r2, [PC, #0x38]		// r2: iap_entry
	0x1D, 0xA3,				// add.n	r3, PC, #0x7C		// r3: address of command
	0x24, 0xA1,				// add.n	r1, PC, #0x94		// r4: address of result
	0x0C, 0xA0,				// add.n	r0, PC, #0x38		// r5: address of buff command
	0x22, 0xA6,				// add.n	r6, PC, #0x8C		// r6: address of sync
	0x27, 0xA7,				// add.n	r7, PC, #0xA4		// r7: address of iap_cnt
							// copy command
	0x1C, 0x68,				// ldr		r4, [r3, #0x00]
	0x04, 0x60,				// str		r4, [r0, #0x00]
	0x5C, 0x68,				// ldr		r4, [r3, #0x04]
	0x44, 0x60,				// str		r4, [r0, #0x04]
	0x9C, 0x68,				// ldr		r4, [r3, #0x08]
	0x84, 0x60,				// str		r4, [r0, #0x08]
	0xDC, 0x68,				// ldr		r4, [r3, #0x0C]
	0xC4, 0x60,				// str		r4, [r0, #0x0C]
	0x1C, 0x69,				// ldr		r4, [r3, #0x10]
	0x04, 0x61,				// str		r4, [r0, #0x10]
	0x5C, 0x69,				// ldr		r4, [r3, #0x14]
	0x44, 0x61,				// str		r4, [r0, #0x14]
							// clear_sync:
	0x00, 0x24,				// movs		r4, #0
	0x34, 0x60,				// str		r4, [r6]
							// call_iap:
	0x90, 0x47,				// blx		r2
							// increase_iap_cnt:
	0x3C, 0x68,				// ldr		r4, [r7]
	0x64, 0x1C,				// adds		r4, r4, #1
	0x3C, 0x60,				// str		r4, [r7]
	0xE3, 0xE7,				// b.n		wait_start
							// exit:
							// deadloop:
	0xFE, 0xE7,				// b.n		dead_loop
	0, 0,					// fill
							// parameter
	0, 0, 0, 0,				// address of iap_entry: 0x3C
							// buff command: 0x40
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
							// empty
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
							// command: 0x80
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
							// sync: 0x98
	0, 0, 0, 0,
							// result: 0x9C
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
							// iap_cnt: 0xB0
	0, 0, 0, 0
};

static RESULT lpc1000swj_iap_init(void)
{
	uint32_t *para_ptr = (uint32_t*)&iap_code[LPC1000_IAP_ENTRY_OFFSET];
	uint8_t verify_buff[sizeof(iap_code)];
	uint32_t reg;
	
	para_ptr[0] = SYS_TO_LE_U32(LPC1000_IAP_ENTRY);
	
	if (ERROR_OK != cm3_dp_halt())
	{
		LOG_ERROR(ERRMSG_FAILURE_OPERATION, "halt lpc1000");
		return ERRCODE_FAILURE_OPERATION;
	}
	
	// write sp
	reg = LPC1000_IAP_BASE + 512;
	if (ERROR_OK != cm3_write_core_register(CM3_COREREG_SP, &reg))
	{
		LOG_ERROR(ERRMSG_FAILURE_OPERATION, "write SP");
		return ERRCODE_FAILURE_OPERATION;
	}
	
	// write iap_code to target SRAM
	if (ERROR_OK != adi_memap_write_buf(LPC1000_IAP_BASE, (uint8_t*)iap_code,
											sizeof(iap_code)))
	{
		LOG_ERROR(ERRMSG_FAILURE_OPERATION, "load iap_code to SRAM");
		return ERRCODE_FAILURE_OPERATION;
	}
	// verify iap_code
	memset(verify_buff, 0, sizeof(iap_code));
	if (ERROR_OK != adi_memap_read_buf(LPC1000_IAP_BASE, verify_buff,
										sizeof(iap_code)))
	{
		LOG_ERROR(ERRMSG_FAILURE_OPERATION, "read flash_loader");
		return ERRCODE_FAILURE_OPERATION;
	}
	if (memcmp(verify_buff, iap_code, sizeof(iap_code)))
	{
		LOG_ERROR(ERRMSG_FAILURE_OPERATION, "verify flash_loader");
		return ERRCODE_FAILURE_OPERATION;
	}
	
	// write pc
	reg = LPC1000_IAP_BASE + 1;
	if (ERROR_OK != cm3_write_core_register(CM3_COREREG_PC, &reg))
	{
		LOG_ERROR(ERRMSG_FAILURE_OPERATION, "write PC");
		return ERRCODE_FAILURE_OPERATION;
	}
	
	if (ERROR_OK != cm3_dp_resume())
	{
		LOG_ERROR(ERRMSG_FAILURE_OPERATION, "run iap");
		return ERRCODE_FAILURE_OPERATION;
	}
	lpc1000swj_iap_cnt = 0;
	return ERROR_OK;
}

static RESULT lpc1000swj_iap_run(uint32_t cmd, uint32_t param_table[5])
{
	uint32_t buff_tmp[7];
	
	buff_tmp[0] = SYS_TO_LE_U32(cmd);				// iap command
	buff_tmp[1] = SYS_TO_LE_U32(param_table[0]);	// iap parameters
	buff_tmp[2] = SYS_TO_LE_U32(param_table[1]);
	buff_tmp[3] = SYS_TO_LE_U32(param_table[2]);
	buff_tmp[4] = SYS_TO_LE_U32(param_table[3]);
	buff_tmp[5] = SYS_TO_LE_U32(param_table[4]);
	// sync is word AFTER command in sram
	buff_tmp[6] = SYS_TO_LE_U32(1);					// sync
	
	// write iap command with sync to target SRAM
	if (ERROR_OK != adi_memap_write_buf(LPC1000_IAP_COMMAND_ADDR,
										(uint8_t*)buff_tmp, sizeof(buff_tmp)))
	{
		LOG_ERROR(ERRMSG_FAILURE_OPERATION, "load iap cmd to SRAM");
		return ERRCODE_FAILURE_OPERATION;
	}
	lpc1000swj_iap_cnt++;
	
	return ERROR_OK;
}

static RESULT lpc1000swj_iap_poll_result(uint32_t result_table[7], bool *failed)
{
	uint8_t i;
	
	*failed = false;
	
	// read result and sync
	// sync is 4-byte BEFORE result
	if (ERROR_OK != adi_memap_read_buf(LPC1000_IAP_SYNC_ADDR,
										(uint8_t *)result_table, 28))
	{
		*failed = true;
		LOG_ERROR(ERRMSG_FAILURE_OPERATION, "read iap sync");
		return ERRCODE_FAILURE_OPERATION;
	}
	for (i = 0; i < 7; i++)
	{
		result_table[i] = LE_TO_SYS_U32(result_table[i]);
	}
	return (0 == result_table[0]) ? ERROR_OK : ERROR_FAIL;
}

static RESULT lpc1000swj_iap_wait_ready(uint32_t result_table[4], bool last)
{
	bool failed = false;
	uint32_t start, end;
	uint32_t buff_tmp[7];
	
	start = get_time_in_ms();
	while (1)
	{
		if ((ERROR_OK != lpc1000swj_iap_poll_result(buff_tmp, &failed)) ||
			(last && (buff_tmp[6] != lpc1000swj_iap_cnt)))
		{
			if (failed)
			{
				LOG_ERROR(ERRMSG_FAILURE_OPERATION, "poll iap result");
				return ERROR_FAIL;
			}
			else
			{
				end = get_time_in_ms();
				// wait 1s at most
				if ((end - start) > 1000)
				{
					cm3_dump(LPC1000_IAP_BASE, sizeof(iap_code));
					LOG_ERROR(ERRMSG_TIMEOUT, "wait for iap ready");
					return ERRCODE_FAILURE_OPERATION;
				}
			}
		}
		else if (buff_tmp[1] != 0)
		{
			LOG_ERROR(ERRMSG_FAILURE_OPERATION_ERRCODE, "run iap", buff_tmp[1]);
			return ERROR_FAIL;
		}
		else
		{
			memcpy(result_table, &buff_tmp[2], 16);
			break;
		}
	}
	
	return ERROR_OK;
}

static RESULT lpc1000swj_iap_call(uint32_t cmd, uint32_t param_table[5],
									uint32_t result_table[4], bool last)
{	
	if ((ERROR_OK != lpc1000swj_iap_run(cmd, param_table))
		|| (ERROR_OK != lpc1000swj_iap_wait_ready(result_table, last)))
	{
		LOG_ERROR(ERRMSG_FAILURE_OPERATION, "run iap command");
		return ERROR_FAIL;
	}
	return ERROR_OK;
}

ENTER_PROGRAM_MODE_HANDLER(lpc1000swj)
{
	uint32_t reg;
	struct chip_param_t *param = context->param;
	
	if (ERROR_OK != lpc1000swj_iap_init())
	{
		return ERROR_FAIL;
	}
	
	// SYSMEMREMAP should be written to LPC1000_SYSMEMREMAP_USERFLASH
	// to access first sector successfully
	if (param->param[LPC1000_PARAM_SYSMEMREMAP_ADDR])
	{
		reg = param->param[LPC1000_PARAM_FLASHREMAP_VALUE];
		if (ERROR_OK != adi_memap_write_reg32(
				param->param[LPC1000_PARAM_SYSMEMREMAP_ADDR], &reg, 1))
		{
			LOG_ERROR(ERRMSG_FAILURE_OPERATION, "write PC");
			return ERRCODE_FAILURE_OPERATION;
		}
	}
	
	return ERROR_OK;
}

LEAVE_PROGRAM_MODE_HANDLER(lpc1000swj)
{
	uint32_t result_table[4];
	
	REFERENCE_PARAMETER(context);
	REFERENCE_PARAMETER(success);
	
	return lpc1000swj_iap_wait_ready(result_table, true);
}

ERASE_TARGET_HANDLER(lpc1000swj)
{
	struct program_info_t *pi = context->pi;
	RESULT ret= ERROR_OK;
	uint32_t iap_cmd_param[5], iap_reply[4];
	uint32_t sector = lpc1000_get_sector_idx_by_addr(context,
								pi->program_areas[APPLICATION_IDX].size - 1);
	
	REFERENCE_PARAMETER(size);
	REFERENCE_PARAMETER(addr);
	
	switch (area)
	{
	case APPLICATION_CHAR:
		memset(iap_cmd_param, 0, sizeof(iap_cmd_param));
		memset(iap_reply, 0, sizeof(iap_reply));
		iap_cmd_param[0] = 0;				// Start Sector Number
		iap_cmd_param[1] = sector;			// End Sector Number
		iap_cmd_param[2] = pi->kernel_khz;	// CPU Clock Frequency(in kHz)
		if (ERROR_OK != lpc1000swj_iap_call(LPC1000_IAPCMD_PREPARE_SECTOR,
											iap_cmd_param, iap_reply, false))
		{
			LOG_ERROR(ERRMSG_FAILURE_OPERATION, "prepare sectors");
			ret = ERRCODE_FAILURE_OPERATION;
			break;
		}
		
		memset(iap_cmd_param, 0, sizeof(iap_cmd_param));
		memset(iap_reply, 0, sizeof(iap_reply));
		iap_cmd_param[0] = 0;				// Start Sector Number
		iap_cmd_param[1] = sector;			// End Sector Number
		iap_cmd_param[2] = pi->kernel_khz;	// CPU Clock Frequency(in kHz)
		if (ERROR_OK != lpc1000swj_iap_call(LPC1000_IAPCMD_ERASE_SECTOR,
											iap_cmd_param, iap_reply, true))
		{
			LOG_ERROR(ERRMSG_FAILURE_OPERATION, "erase sectors");
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

WRITE_TARGET_HANDLER(lpc1000swj)
{
	struct program_info_t *pi = context->pi;
	struct chip_param_t *param = context->param;
	uint32_t iap_cmd_param[5], iap_reply[4];
	uint32_t start_sector;
	static uint8_t ticktock = 0;
	uint32_t buff_addr;
	uint16_t page_size = (uint16_t)param->chip_areas[APPLICATION_IDX].page_size;
	
	switch (area)
	{
	case APPLICATION_CHAR:
		if (size != page_size)
		{
			return ERROR_FAIL;
		}
		
		memset(iap_cmd_param, 0, sizeof(iap_cmd_param));
		memset(iap_reply, 0, sizeof(iap_reply));
		start_sector = lpc1000_get_sector_idx_by_addr(context, addr);
		iap_cmd_param[0] = start_sector;	// Start Sector Number
		iap_cmd_param[1] = start_sector;	// End Sector Number
		iap_cmd_param[2] = pi->kernel_khz;	// CPU Clock Frequency(in kHz)
		if (ERROR_OK != lpc1000swj_iap_call(LPC1000_IAPCMD_PREPARE_SECTOR,
											iap_cmd_param, iap_reply, false))
		{
			LOG_ERROR(ERRMSG_FAILURE_OPERATION, "prepare sectors");
			return ERRCODE_FAILURE_OPERATION;
		}
		
		if (ticktock & 1)
		{
			buff_addr = LPC1000_IAP_BASE + 512 + page_size;
		}
		else
		{
			buff_addr = LPC1000_IAP_BASE + 512;
		}
		memset(iap_cmd_param, 0, sizeof(iap_cmd_param));
		iap_cmd_param[0] = addr;			// Destination flash address
		iap_cmd_param[1] = buff_addr;		// Source RAM address
		iap_cmd_param[2] = page_size;		// Number of bytes to be written
		iap_cmd_param[3] = pi->kernel_khz;	// CPU Clock Frequency(in kHz)
		if ((ERROR_OK != adi_memap_write_buf(buff_addr, buff, page_size)) ||
			(ERROR_OK != lpc1000swj_iap_call(LPC1000_IAPCMD_RAM_TO_FLASH,
											iap_cmd_param, iap_reply, false)))
		{
			LOG_ERROR(ERRMSG_FAILURE_OPERATION, "run iap");
			return ERRCODE_FAILURE_OPERATION;
		}
		ticktock++;
		break;
	default:
		return ERROR_FAIL;
	}
	
	return ERROR_OK;
}

READ_TARGET_HANDLER(lpc1000swj)
{
	RESULT ret = ERROR_OK;
	uint32_t cur_block_size;
	uint32_t iap_cmd_param[5], iap_reply[4];
	
	REFERENCE_PARAMETER(context);
	
	switch (area)
	{
	case CHIPID_CHAR:
		memset(iap_cmd_param, 0, sizeof(iap_cmd_param));
		memset(iap_reply, 0, sizeof(iap_reply));
		if (ERROR_OK != lpc1000swj_iap_call(LPC1000_IAPCMD_READ_BOOTVER,
											iap_cmd_param, iap_reply, true))
		{
			LOG_ERROR(ERRMSG_FAILURE_OPERATION, "read bootver");
			ret = ERRCODE_FAILURE_OPERATION;
			break;
		}
		LOG_INFO(INFOMSG_BOOTLOADER_VERSION, (iap_reply[0] >> 8) & 0xFF,
					(iap_reply[0] >> 0) & 0xFF);
		
		memset(iap_cmd_param, 0, sizeof(iap_cmd_param));
		memset(iap_reply, 0, sizeof(iap_reply));
		if (ERROR_OK != lpc1000swj_iap_call(LPC1000_IAPCMD_READ_ID,
											iap_cmd_param, iap_reply, true))
		{
			LOG_ERROR(ERRMSG_FAILURE_OPERATION, "read id");
			ret = ERRCODE_FAILURE_OPERATION;
			break;
		}
		*(uint32_t*)buff = iap_reply[0];
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
	case UNIQUEID_CHAR:
		memset(iap_cmd_param, 0, sizeof(iap_cmd_param));
		if (ERROR_OK != lpc1000swj_iap_call(LPC1000_IAPCMD_READ_SERIAL,
										iap_cmd_param, (uint32_t *)buff, true))
		{
			LOG_ERROR(ERRMSG_FAILURE_OPERATION, "read serialnum");
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

