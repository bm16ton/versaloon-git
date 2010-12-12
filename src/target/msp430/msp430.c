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

#include "vsprog.h"
#include "programmer.h"
#include "target.h"
#include "scripts.h"

#include "msp430.h"
#include "msp430_internal.h"

#include "JTAGfunc.h"

#define CUR_TARGET_STRING			MSP430_STRING

struct program_area_map_t msp430_program_area_map[] = 
{
	{APPLICATION_CHAR, 1, 0, 0, 0, AREA_ATTR_EW | AREA_ATTR_V},
	{0, 0, 0, 0, 0, 0}
};

const struct program_mode_t msp430_program_mode[] = 
{
	{'j', "", MSP430_JTAG},
	{'s', "", MSP430_SBW},
	{'b', USE_COMM, 0},
	{0, NULL, 0}
};

RESULT (*enter_program_mode_save)(struct program_context_t *context);
struct program_functions_t msp430_program_functions;

RESULT (*msp430jtagsbw_init)(uint8_t index);
RESULT (*msp430jtagsbw_fini)(uint8_t index);
RESULT (*msp430jtagsbw_config)(uint8_t index, uint8_t has_test);
RESULT (*msp430jtagsbw_ir)(uint8_t index, uint8_t *ir, uint8_t want_ret);
RESULT (*msp430jtagsbw_dr)(uint8_t index, uint32_t *dr, uint8_t len, 
							uint8_t want_ret);
RESULT (*msp430jtagsbw_tclk)(uint8_t index, uint8_t value);
RESULT (*msp430jtagsbw_tclk_strobe)(uint8_t index, uint16_t cnt);
RESULT (*msp430jtagsbw_reset)(uint8_t index);
RESULT (*msp430jtagsbw_poll)(uint8_t index, uint32_t dr, uint32_t mask, 
								uint32_t value, uint8_t len, 
								uint16_t poll_cnt, uint8_t toggle_tclk);

MISC_HANDLER(msp430_help)
{
	MISC_CHECK_ARGC(1);
	printf("\
Usage of %s:\n\
  -m,  --mode <MODE>                        set mode<j|s|b>\n\n", 
			CUR_TARGET_STRING);
	return ERROR_OK;
}

ENTER_PROGRAM_MODE_HANDLER(msp430)
{
	struct program_info_t *pi = context->pi;
	struct interfaces_info_t *interfaces = &context->prog->interfaces;
	
	if (NULL == enter_program_mode_save)
	{
		return ERROR_FAIL;
	}
	
	switch (pi->mode)
	{
	case MSP430_MODE_JTAG:
		msp430jtagsbw_init = interfaces->msp430jtag.init;
		msp430jtagsbw_fini = interfaces->msp430jtag.fini;
		msp430jtagsbw_config = interfaces->msp430jtag.config;
		msp430jtagsbw_ir = interfaces->msp430jtag.ir;
		msp430jtagsbw_dr = interfaces->msp430jtag.dr;
		msp430jtagsbw_tclk = interfaces->msp430jtag.tclk;
		msp430jtagsbw_tclk_strobe = interfaces->msp430jtag.tclk_strobe;
		msp430jtagsbw_reset = interfaces->msp430jtag.reset;
		msp430jtagsbw_poll = interfaces->msp430jtag.poll;
		break;
	case MSP430_MODE_SBW:
		msp430jtagsbw_init = interfaces->msp430sbw.init;
		msp430jtagsbw_fini = interfaces->msp430sbw.fini;
		msp430jtagsbw_config = interfaces->msp430sbw.config;
		msp430jtagsbw_ir = interfaces->msp430sbw.ir;
		msp430jtagsbw_dr = interfaces->msp430sbw.dr;
		msp430jtagsbw_tclk = interfaces->msp430sbw.tclk;
		msp430jtagsbw_tclk_strobe = interfaces->msp430sbw.tclk_strobe;
		msp430jtagsbw_reset = interfaces->msp430sbw.reset;
		msp430jtagsbw_poll = interfaces->msp430sbw.poll;
		break;
	case MSP430_MODE_BSL:
		return ERROR_FAIL;
		break;
	}
	
	return enter_program_mode_save(context);
}

MISC_HANDLER(msp430_mode)
{
	uint8_t mode;
	
	MISC_CHECK_ARGC(2);
	mode = (uint8_t)strtoul(argv[1], NULL,0);
	switch (mode)
	{
	case MSP430_MODE_JTAG:
	case MSP430_MODE_SBW:
		memcpy(&msp430_program_functions, &msp430jtagsbw_program_functions, 
				sizeof(msp430_program_functions));
		enter_program_mode_save = msp430jtagsbw_program_functions.enter_program_mode;
		msp430_program_functions.enter_program_mode = msp430_enter_program_mode;
		break;
	case MSP430_MODE_BSL:
		return ERROR_FAIL;
		break;
	}
	return ERROR_OK;
}

const struct misc_cmd_t msp430_notifier[] = 
{
	MISC_CMD(	"help",
				"print help information of current target for internal call",
				msp430_help),
	MISC_CMD(	"mode",
				"set programming mode of target for internal call",
				msp430_mode),
	MISC_CMD_END
};

