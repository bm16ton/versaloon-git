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

#include "pgbar.h"

#include "vsprog.h"
#include "programmer.h"
#include "target.h"
#include "scripts.h"

#include "dal.h"
#include "mal/mal.h"
#include "ee93cx6/ee93cx6_drv.h"

#include "93cx6.h"
#include "93cx6_internal.h"

#define CUR_TARGET_STRING			EE93CX6_STRING

struct program_area_map_t ee93cx6_program_area_map[] = 
{
	{EEPROM_CHAR, 1, 0, 0, 0, AREA_ATTR_EWR | AREA_ATTR_EP},
	{0, 0, 0, 0, 0, 0}
};

const struct program_mode_t ee93cx6_program_mode[] = 
{
	{'b', SET_FREQUENCY, MICROWIRE},
	{'w', SET_FREQUENCY, MICROWIRE},
	{0, NULL, 0}
};

ENTER_PROGRAM_MODE_HANDLER(ee93cx6);
LEAVE_PROGRAM_MODE_HANDLER(ee93cx6);
ERASE_TARGET_HANDLER(ee93cx6);
WRITE_TARGET_HANDLER(ee93cx6);
READ_TARGET_HANDLER(ee93cx6);
const struct program_functions_t ee93cx6_program_functions = 
{
	NULL,			// execute
	ENTER_PROGRAM_MODE_FUNCNAME(ee93cx6), 
	LEAVE_PROGRAM_MODE_FUNCNAME(ee93cx6), 
	ERASE_TARGET_FUNCNAME(ee93cx6), 
	WRITE_TARGET_FUNCNAME(ee93cx6), 
	READ_TARGET_FUNCNAME(ee93cx6)
};

static uint8_t ee93cx6_origination_mode;

VSS_HANDLER(ee93cx6_help)
{
	VSS_CHECK_ARGC(1);
	printf("\
Usage of %s:\n\
  -F,  --frequency <FREQUENCY>              set MicroWire frequency, in KHz\n\
  -m,  --mode <MODE>                        set mode<b|w>\n\n", 
			CUR_TARGET_STRING);
	return ERROR_OK;
}

VSS_HANDLER(ee93cx6_mode)
{
	uint8_t mode;
	
	VSS_CHECK_ARGC(2);
	mode = (uint8_t)strtoul(argv[1], NULL,0);
	switch (mode)
	{
	case EE93CX6_MODE_BYTE:
	case EE93CX6_MODE_WORD:
		ee93cx6_origination_mode = mode;
		break;
	default:
		return ERROR_FAIL;
		break;
	}
	return ERROR_OK;
}

const struct vss_cmd_t ee93cx6_notifier[] = 
{
	VSS_CMD(	"help",
				"print help information of current target for internal call",
				ee93cx6_help),
	VSS_CMD(	"mode",
				"set programming mode of target for internal call",
				ee93cx6_mode),
	VSS_CMD_END
};





static struct interfaces_info_t *interfaces = NULL;
#define commit()					interfaces->peripheral_commit()

ENTER_PROGRAM_MODE_HANDLER(ee93cx6)
{
	struct chip_param_t *param = context->param;
	struct program_info_t *pi = context->pi;
	struct ee93cx6_drv_param_t drv_param;
	
	interfaces = &(context->prog->interfaces);
	if (ERROR_OK != dal_init(interfaces))
	{
		return ERROR_FAIL;
	}
	
	drv_param.addr_bitlen = (uint8_t)param->param[EE93CX6_PARAM_ADDR_BITLEN];
	drv_param.cmd_bitlen = (uint8_t)param->param[EE93CX6_PARAM_OPCODE_BITLEN];
	drv_param.iic_khz = pi->frequency;
	if (EE93CX6_MODE_BYTE == ee93cx6_origination_mode)
	{
		drv_param.origination_mode = EE93CX6_ORIGINATION_BYTE;
	}
	else
	{
		drv_param.origination_mode = EE93CX6_ORIGINATION_WORD;
	}
	if (ERROR_OK != mal.init(MAL_IDX_EE93CX6, &drv_param))
	{
		return ERROR_FAIL;
	}
	if (ERROR_OK != mal.setcapacity(MAL_IDX_EE93CX6, 
			param->chip_areas[EEPROM_IDX].page_size, 
			param->chip_areas[EEPROM_IDX].page_num))
	{
		return ERROR_FAIL;
	}
	
	return commit();
}

LEAVE_PROGRAM_MODE_HANDLER(ee93cx6)
{
	REFERENCE_PARAMETER(context);
	REFERENCE_PARAMETER(success);
	
	mal.fini(MAL_IDX_EE93CX6);
	return commit();
}

ERASE_TARGET_HANDLER(ee93cx6)
{
	REFERENCE_PARAMETER(context);
	REFERENCE_PARAMETER(area);
	REFERENCE_PARAMETER(addr);
	REFERENCE_PARAMETER(size);
	
	if (ERROR_OK != mal.eraseall(MAL_IDX_EE93CX6))
	{
		return ERROR_FAIL;
	}
	return commit();
}

WRITE_TARGET_HANDLER(ee93cx6)
{
	struct chip_param_t *param = context->param;
	
	switch (area)
	{
	case EEPROM_CHAR:
		if (size % param->chip_areas[EEPROM_IDX].page_size)
		{
			return ERROR_FAIL;
		}
		size /= param->chip_areas[EEPROM_IDX].page_size;
		
		if (ERROR_OK != mal.writeblock(MAL_IDX_EE93CX6, addr, buff, size))
		{
			return ERROR_FAIL;
		}
		return commit();
		break;
	default:
		return ERROR_FAIL;
	}
}

READ_TARGET_HANDLER(ee93cx6)
{
	struct chip_param_t *param = context->param;
	
	switch (area)
	{
	case EEPROM_CHAR:
		if (size % param->chip_areas[EEPROM_IDX].page_size)
		{
			return ERROR_FAIL;
		}
		size /= param->chip_areas[EEPROM_IDX].page_size;
		
		if (ERROR_OK != mal.readblock(MAL_IDX_EE93CX6, addr, buff, size))
		{
			return ERROR_FAIL;
		}
		return commit();
		break;
	default:
		return ERROR_FAIL;
	}
}

