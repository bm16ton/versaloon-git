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

#include "c8051f.h"
#include "c8051f_internal.h"

#define CUR_TARGET_STRING		C8051F_STRING
#define CUR_DEFAULT_FREQ		4500

ENTER_PROGRAM_MODE_HANDLER(c8051fjtag);
LEAVE_PROGRAM_MODE_HANDLER(c8051fjtag);
ERASE_TARGET_HANDLER(c8051fjtag);
WRITE_TARGET_HANDLER(c8051fjtag);
READ_TARGET_HANDLER(c8051fjtag);
struct program_functions_t c8051fjtag_program_functions = 
{
	NULL,			// execute
	ENTER_PROGRAM_MODE_FUNCNAME(c8051fjtag), 
	LEAVE_PROGRAM_MODE_FUNCNAME(c8051fjtag), 
	ERASE_TARGET_FUNCNAME(c8051fjtag), 
	WRITE_TARGET_FUNCNAME(c8051fjtag), 
	READ_TARGET_FUNCNAME(c8051fjtag)
};

static struct interfaces_info_t *interfaces = NULL;


#define jtag_init()					interfaces->jtag_hl.init()
#define jtag_fini()					interfaces->jtag_hl.fini()
#define jtag_config(kHz,a,b,c,d)	\
	interfaces->jtag_hl.config((kHz), (a), (b), (c), (d))
#define jtag_runtest(len)			interfaces->jtag_hl.runtest(len)
#define jtag_ir_write(i, len)		\
	interfaces->jtag_hl.ir((uint8_t*)(i), (len), 1, 0)
#define jtag_dr_write(d, len)		\
	interfaces->jtag_hl.dr((uint8_t*)(d), (len), 1, 0)
#define jtag_dr_read(d, len)		\
	interfaces->jtag_hl.dr((uint8_t*)(d), (len), 1, 1)

#if 0
#define jtag_poll_busy()			c8051f_jtag_poll_busy()
#define jtag_poll_flbusy(dly, int)	c8051f_jtag_poll_flbusy((dly), (int))
#else
#define jtag_poll_busy()			interfaces->delay.delayus(20)
#define jtag_poll_flbusy(dly, int)	jtag_delay_us((dly) * ((int) + 1))
#endif

#define jtag_delay_us(us)			interfaces->delay.delayus((us))
#define jtag_delay_ms(ms)			interfaces->delay.delayms((ms))

#define poll_start(cnt, int)		interfaces->poll.start((cnt), (int))
#define poll_end()					interfaces->poll.end()
#define poll_ok(o, m, v)			\
	interfaces->poll.checkok(POLL_CHECK_EQU, (o), 1, (m), (v))

#define jtag_commit()				interfaces->peripheral_commit()

uint32_t dummy;

RESULT c8051f_jtag_poll_busy(void)
{
	poll_start(C8051F_JTAG_MAX_POLL_COUNT, 0);
	
	jtag_dr_read(&dummy, 1);
	poll_ok(0, 0x01, 0x00);
	
	poll_end();
	
	return ERROR_OK;
}

RESULT c8051f_jtag_ind_read(uint8_t addr, uint32_t *value, uint8_t num_bits)
{
	uint16_t ir, dr;

#ifdef PARAM_CHECK
	if ((addr < C8051F_IR_FLASHCON) || (addr > C8051F_IR_FLASHSCL))
	{
		LOG_BUG(ERRMSG_INVALID_ADDRESS, addr, CUR_TARGET_STRING);
		return ERRCODE_INVALID;
	}
#endif
	
	ir = addr | C8051F_IR_STATECNTL_SUSPEND;
	jtag_ir_write(&ir, C8051F_IR_LEN);
	
	dr = C8051F_INDOPTCODE_READ;
	jtag_dr_write(&dr, 2);
	
	dr = 0;
	jtag_poll_busy();
	
	jtag_dr_read(value, num_bits + 1);
	
	return ERROR_OK;
}

RESULT c8051f_jtag_ind_write(uint8_t addr, uint32_t *value, uint8_t num_bits)
{
	uint16_t ir, dr;
	
#ifdef PARAM_CHECK
	if ((addr < C8051F_IR_FLASHCON) || (addr > C8051F_IR_FLASHSCL))
	{
		LOG_BUG(ERRMSG_INVALID_ADDRESS, addr, CUR_TARGET_STRING);
		return ERRCODE_INVALID;
	}
#endif
	
	ir = addr | C8051F_IR_STATECNTL_SUSPEND;
	jtag_ir_write(&ir, C8051F_IR_LEN);
	
	*value |= C8051F_INDOPTCODE_WRITE << num_bits;
	jtag_dr_write(value, num_bits + 2);
	
	dr = 0;
	jtag_poll_busy();
	
	return ERROR_OK;
}

RESULT c8051f_jtag_poll_flbusy(uint16_t poll_cnt, uint16_t interval)
{
	poll_start(poll_cnt, interval);
	
	c8051f_jtag_ind_read(C8051F_IR_FLASHDAT, &dummy, 1);
	poll_ok(0, 0x02, 0x00);
	
	poll_end();
	
	return ERROR_OK;
}

ENTER_PROGRAM_MODE_HANDLER(c8051fjtag)
{
	struct program_info_t *pi = context->pi;
	uint32_t dr;
	
	if (!pi->frequency)
	{
		context->pi->frequency = CUR_DEFAULT_FREQ;
	}
	interfaces = &(context->prog->interfaces);
	
	jtag_init();
	jtag_config(pi->frequency, pi->jtag_pos.ub, pi->jtag_pos.ua, 
					pi->jtag_pos.bb, pi->jtag_pos.ba);
	
	// set FLASHSCL based on SYSCLK (2MHx = 0x86)
	dr = 0x86;
	c8051f_jtag_ind_write(C8051F_IR_FLASHSCL, &dr, C8051F_DR_FLASHSCL_LEN);
	
	return jtag_commit();
}

LEAVE_PROGRAM_MODE_HANDLER(c8051fjtag)
{
	REFERENCE_PARAMETER(context);
	REFERENCE_PARAMETER(success);
	
	jtag_fini();
	return jtag_commit();
}

ERASE_TARGET_HANDLER(c8051fjtag)
{
	struct chip_param_t *param = context->param;
	uint32_t dr;
	RESULT ret = ERROR_OK;
	
	REFERENCE_PARAMETER(context);
	REFERENCE_PARAMETER(size);
	REFERENCE_PARAMETER(addr);
	
	switch (area)
	{
	case APPLICATION_CHAR:
		// set FLASHADR to erase_addr
		dr = param->param[C8051F_PARAM_ERASE_ADDR];
		c8051f_jtag_ind_write(C8051F_IR_FLASHADR, &dr, C8051F_DR_FLASHADR_LEN);
		// set FLASHCON for flash erase operation(0x20)
		dr = 0x20;
		c8051f_jtag_ind_write(C8051F_IR_FLASHCON, &dr, C8051F_DR_FLASHCON_LEN);
		// set FLASHDAT to 0xA5
		dr = 0xA5;
		c8051f_jtag_ind_write(C8051F_IR_FLASHDAT, &dr, C8051F_DR_FLASHDAT_WLEN);
		// set FLASHCON for poll operation
		dr = 0;
		c8051f_jtag_ind_write(C8051F_IR_FLASHCON, &dr, C8051F_DR_FLASHCON_LEN);
		c8051f_jtag_poll_flbusy(1500, 1000);
		if (ERROR_OK != jtag_commit())
		{
			ret = ERRCODE_FAILURE_OPERATION;
			break;
		}
		
		// read FLBusy and FLFail
		c8051f_jtag_ind_read(C8051F_IR_FLASHDAT, &dr, 2);
		
		if ((ERROR_OK != jtag_commit()) || (dr & 0x02))
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

WRITE_TARGET_HANDLER(c8051fjtag)
{
	uint32_t dr, read_buf[512];
	uint32_t i;
	RESULT ret = ERROR_OK;
	
	REFERENCE_PARAMETER(context);
	
	switch (area)
	{
	case APPLICATION_CHAR:
		// set FLASHADR to address to write to
		dr = addr;
		c8051f_jtag_ind_write(C8051F_IR_FLASHADR, &dr, 
								C8051F_DR_FLASHADR_LEN);
		
		for (i = 0; i < size; i++)
		{
			// set FLASHCON for flash write operation(0x10)
			dr = 0x10;
			c8051f_jtag_ind_write(C8051F_IR_FLASHCON, &dr, 
									C8051F_DR_FLASHCON_LEN);
			// initiate the write operation
			dr = buff[i];
			c8051f_jtag_ind_write(C8051F_IR_FLASHDAT, &dr, 
									C8051F_DR_FLASHDAT_WLEN);
			// set FLASHCON for poll operation
			dr = 0;
			c8051f_jtag_ind_write(C8051F_IR_FLASHCON, &dr, 
									C8051F_DR_FLASHCON_LEN);
			// poll for FLBusy
			jtag_poll_flbusy(60, 0);
			c8051f_jtag_ind_read(C8051F_IR_FLASHDAT, read_buf + i, 2);
		}
		
		if (ERROR_OK != jtag_commit())
		{
			LOG_ERROR(ERRMSG_FAILURE_OPERATION_ADDR, "program flash", addr);
			ret = ERRCODE_FAILURE_OPERATION_ADDR;
			break;
		}
		for (i = 0; i < size; i++)
		{
			if ((read_buf[i] & 0x06) != 0)
			{
				LOG_ERROR(ERRMSG_FAILURE_OPERATION_ADDR, "program flash", 
							addr + i);
				ret = ERRCODE_FAILURE_OPERATION;
				break;
			}
		}
		break;
	default:
		ret = ERROR_FAIL;
		break;
	}
	return ret;
}

READ_TARGET_HANDLER(c8051fjtag)
{
	uint16_t ir;
	uint32_t dr, read_buf[512];
	uint32_t i;
	RESULT ret = ERROR_OK;
	
	REFERENCE_PARAMETER(context);
	
	switch (area)
	{
	case CHIPID_CHAR:
		ir = C8051F_IR_STATECNTL_RESET | C8051F_IR_BYPASS;
		jtag_ir_write(&ir, C8051F_IR_LEN);
		ir = C8051F_IR_STATECNTL_HALT | C8051F_IR_IDCODE;
		jtag_ir_write(&ir, C8051F_IR_LEN);
		dr = 0;
		jtag_dr_read(&dr, C8051F_DR_IDCODE_LEN);
		if (ERROR_OK != jtag_commit())
		{
			ret = ERRCODE_FAILURE_OPERATION;
			break;
		}
		*(uint32_t*)buff = dr & C8051F_JTAG_ID_MASK;
		break;
	case APPLICATION_CHAR:
		// set FLASHADR to address to read from
		dr = addr;
		c8051f_jtag_ind_write(C8051F_IR_FLASHADR, &dr, 
								C8051F_DR_FLASHADR_LEN);
		
		for (i = 0; i < size; i++)
		{
			// set FLASHCON for flash read operation(0x02)
			dr = 0x02;
			c8051f_jtag_ind_write(C8051F_IR_FLASHCON, &dr, 
									C8051F_DR_FLASHCON_LEN);
			// initiate the read operation
			dr = 0;
			c8051f_jtag_ind_read(C8051F_IR_FLASHDAT, &dr, 
									C8051F_DR_FLASHDAT_RLEN);
			// set FLASHCON for poll operation
			dr = 0;
			c8051f_jtag_ind_write(C8051F_IR_FLASHCON, &dr, 
									C8051F_DR_FLASHCON_LEN);
			// poll for FLBusy
			jtag_poll_flbusy(0, 0);
			c8051f_jtag_ind_read(C8051F_IR_FLASHDAT, read_buf + i, 
									C8051F_DR_FLASHDAT_RLEN);
		}
		
		if (ERROR_OK != jtag_commit())
		{
			LOG_ERROR(ERRMSG_FAILURE_OPERATION_ADDR, "read flash", addr);
			ret = ERRCODE_FAILURE_OPERATION_ADDR;
			break;
		}
		for (i = 0; i < size; i++)
		{
			if ((read_buf[i] & 0x06) != 0)
			{
				ret = ERRCODE_FAILURE_OPERATION;
				break;
			}
			buff[i] = (read_buf[i] >> 3) & 0xFF;
		}
		break;
	default:
		ret = ERROR_FAIL;
		break;
	}
	return ret;
}

