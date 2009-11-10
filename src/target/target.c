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
#include <libxml/parser.h>

#include "vsprog.h"
#include "programmer.h"
#include "target.h"

#include "target.h"
#include "at89s5x/at89s5x.h"
#include "psoc1/psoc1.h"
#include "lpc900/lpc900.h"
#include "msp430/msp430.h"
#include "c8051f/c8051f.h"
#include "avr8/avr8.h"
#include "comisp/comisp.h"
#include "svf_player/svf_player.h"
#include "cortex-m3/cm3.h"

chip_series_t target_chips = {0, NULL};
chip_param_t target_chip_param;

target_info_t targets_info[] = 
{
	// at89s5x
	{
		S5X_STRING,							// name
		APPLICATION | LOCK,					// areas
		s5x_program_area_map,				// program_area_map
		S5X_PROGRAM_MODE_STR,				// program_mode_str
		s5x_parse_argument,					// parse_argument
		s5x_probe_chip,						// probe_chip
		s5x_init,							// init
		s5x_fini,							// fini
		s5x_interface_needed,				// interfaces_needed
		s5x_prepare_buffer,					// prepare_buffer
		s5x_write_buffer_from_file_callback,
											// write_buffer_from_file_callback
		NULL,								// write_file_from_buffer_callback
		s5x_program,						// program
		
		NULL,								// get_mass_product_data_size
		NULL,								// prepare_mass_product_data
	},
	// psoc
	{
		PSOC1_STRING,						// name
		APPLICATION | LOCK | CHECKSUM,		// areas
		psoc1_program_area_map,				// program_area_map
		PSOC1_PROGRAM_MODE_STR,				// program_mode_str
		psoc1_parse_argument,				// parse_argument
		psoc1_probe_chip,					// probe_chip
		psoc1_init,							// init
		psoc1_fini,							// fini
		psoc1_interface_needed,				// interfaces_needed
		psoc1_prepare_buffer,				// prepare_buffer
		psoc1_write_buffer_from_file_callback,
											// write_buffer_from_file_callback
		NULL,								// write_file_from_buffer_callback
		psoc1_program,						// program
		
		psoc1_get_mass_product_data_size,	// get_mass_product_data_size
		psoc1_prepare_mass_product_data,		// prepare_mass_product_data
	},
	// msp430
	{
		MSP430_STRING,						// name
		APPLICATION,						// areas
		msp430_program_area_map,			// program_area_map
		MSP430_PROGRAM_MODE_STR,			// program_mode_str
		msp430_parse_argument,				// parse_argument
		msp430_probe_chip,					// probe_chip
		msp430_init,						// init
		msp430_fini,						// fini
		msp430_interface_needed,			// interfaces_needed
		msp430_prepare_buffer,				// prepare_buffer
		msp430_write_buffer_from_file_callback,
											// write_buffer_from_file_callback
		NULL,								// write_file_from_buffer_callback
		msp430_program,						// program
		
		NULL,								// get_mass_product_data_size
		NULL,								// prepare_mass_product_data
	},
	// c8051f
	{
		C8051F_STRING,						// name
		APPLICATION,						// areas
		c8051f_program_area_map,			// program_area_map
		C8051F_PROGRAM_MODE_STR,			// program_mode_str
		c8051f_parse_argument,				// parse_argument
		c8051f_probe_chip,					// probe_chip
		c8051f_init,						// init
		c8051f_fini,						// fini
		c8051f_interface_needed,			// interfaces_needed
		c8051f_prepare_buffer,				// prepare_buffer
		c8051f_write_buffer_from_file_callback,
											// write_buffer_from_file_callback
		NULL,								// write_file_from_buffer_callback
		c8051f_program,						// program
		
		NULL,								// get_mass_product_data_size
		NULL,								// prepare_mass_product_data
	},
	// avr8
	{
		AVR8_STRING,						// name
		APPLICATION | EEPROM | LOCK | FUSE,	// areas
		avr8_program_area_map,				// program_area_map
		AVR8_PROGRAM_MODE_STR,				// program_mode_str
		avr8_parse_argument,				// parse_argument
		avr8_probe_chip,					// probe_chip
		avr8_init,							// init
		avr8_fini,							// fini
		avr8_interface_needed,				// interfaces_needed
		avr8_prepare_buffer,				// prepare_buffer
		avr8_write_buffer_from_file_callback,
											// write_buffer_from_file_callback
		NULL,								// write_file_from_buffer_callback
		avr8_program,						// program
		
		NULL,								// get_mass_product_data_size
		NULL,								// prepare_mass_product_data
	},
	// comisp
	{
		COMISP_STRING,						// name
		APPLICATION | EEPROM | LOCK | FUSE,	// areas
		comisp_program_area_map,			// program_area_map
		"",									// program_mode_str
		comisp_parse_argument,				// parse_argument
		comisp_probe_chip,					// probe_chip
		comisp_init,						// init
		comisp_fini,						// fini
		comisp_interface_needed,			// interfaces_needed
		comisp_prepare_buffer,				// prepare_buffer
		comisp_write_buffer_from_file_callback,
											// write_buffer_from_file_callback
		NULL,								// write_file_from_buffer_callback
		comisp_program,						// program
		
		NULL,								// get_mass_product_data_size
		NULL,								// prepare_mass_product_data
	},
	// svf_player
	{
		SVFP_STRING,						// name
		0,									// areas
		svfp_program_area_map,				// program_area_map
		"",									// program_mode_str
		svfp_parse_argument,				// parse_argument
		svfp_probe_chip,					// probe_chip
		svfp_init,							// init
		svfp_fini,							// fini
		svfp_interface_needed,				// interfaces_needed
		svfp_prepare_buffer,				// prepare_buffer
		svfp_write_buffer_from_file_callback,
											// write_buffer_from_file_callback
		NULL,								// write_file_from_buffer_callback
		svfp_program,						// program
		
		NULL,								// get_mass_product_data_size
		NULL,								// prepare_mass_product_data
	},
	// lpc900
	{
		LPC900_STRING,						// name
		APPLICATION,						// areas
		lpc900_program_area_map,			// program_area_map
		LPC900_PROGRAM_MODE_STR,			// program_mode_str
		lpc900_parse_argument,				// parse_argument
		lpc900_probe_chip,					// probe_chip
		lpc900_init,						// init
		lpc900_fini,						// fini
		lpc900_interface_needed,			// interfaces_needed
		lpc900_prepare_buffer,				// prepare_buffer
		lpc900_write_buffer_from_file_callback,
											// write_buffer_from_file_callback
		NULL,								// write_file_from_buffer_callback
		lpc900_program,						// program
		
		NULL,								// get_mass_product_data_size
		NULL,								// prepare_mass_product_data
	},
	// cortex-m3
	{
		CM3_STRING,							// name
		APPLICATION,						// areas
		cm3_program_area_map,				// program_area_map
		CM3_PROGRAM_MODE_STR,				// program_mode_str
		cm3_parse_argument,					// parse_argument
		cm3_probe_chip,						// probe_chip
		cm3_init,							// init
		cm3_fini,							// fini
		cm3_interface_needed,				// interfaces_needed
		cm3_prepare_buffer,					// prepare_buffer
		cm3_write_buffer_from_file_callback,
											// write_buffer_from_file_callback
		NULL,								// write_file_from_buffer_callback
		cm3_program,						// program
		
		NULL,								// get_mass_product_data_size
		NULL,								// prepare_mass_product_data
	},
	{
		NULL,								// name
		0,									// areas
		NULL,								// program_area_map
		NULL,								// program_mode_str
		NULL,								// parse_argument
		NULL,								// probe_chip
		NULL,								// init
		NULL,								// fini
		NULL,								// interfaces_needed
		NULL,								// prepare_buffer
		NULL,								// write_buffer_from_file_callback
		NULL,								// write_file_from_buffer_callback
		NULL,								// program
		
		NULL,								// get_mass_product_data_size
		NULL,								// prepare_mass_product_data
	}
};
target_info_t *cur_target = NULL;
program_info_t program_info;

RESULT target_alloc_data_buffer(void)
{
	if ((NULL == program_info.boot) && (program_info.boot_size > 0))
	{
		program_info.boot = malloc(program_info.boot_size);
		if (NULL == program_info.boot)
		{
			return ERRCODE_NOT_ENOUGH_MEMORY;
		}
	}
	if ((NULL == program_info.boot_checksum) 
	   && (program_info.boot_checksum_size > 0))
	{
		program_info.boot_checksum = malloc(program_info.boot_checksum_size);
		if (NULL == program_info.boot_checksum)
		{
			return ERRCODE_NOT_ENOUGH_MEMORY;
		}
	}
	if ((NULL == program_info.app) && (program_info.app_size > 0))
	{
		program_info.app = malloc(program_info.app_size);
		if (NULL == program_info.app)
		{
			return ERRCODE_NOT_ENOUGH_MEMORY;
		}
	}
	if ((NULL == program_info.app_checksum) 
	   && (program_info.app_checksum_size > 0))
	{
		program_info.app_checksum = malloc(program_info.app_checksum_size);
		if (NULL == program_info.app_checksum)
		{
			return ERRCODE_NOT_ENOUGH_MEMORY;
		}
	}
	if ((NULL == program_info.eeprom) && (program_info.eeprom_size > 0))
	{
		program_info.eeprom = malloc(program_info.eeprom_size);
		if (NULL == program_info.eeprom)
		{
			return ERRCODE_NOT_ENOUGH_MEMORY;
		}
	}
	if ((NULL == program_info.eeprom_checksum) 
	   && (program_info.eeprom_checksum_size > 0))
	{
		program_info.eeprom_checksum = \
									malloc(program_info.eeprom_checksum_size);
		if (NULL == program_info.eeprom_checksum)
		{
			return ERRCODE_NOT_ENOUGH_MEMORY;
		}
	}
	if ((NULL == program_info.otp_rom) && (program_info.otp_rom_size > 0))
	{
		program_info.otp_rom = malloc(program_info.otp_rom_size);
		if (NULL == program_info.otp_rom)
		{
			return ERRCODE_NOT_ENOUGH_MEMORY;
		}
	}
	if ((NULL == program_info.otp_rom_checksum) 
	   && (program_info.otp_rom_checksum_size > 0))
	{
		program_info.otp_rom_checksum = \
									malloc(program_info.otp_rom_checksum_size);
		if (NULL == program_info.otp_rom_checksum)
		{
			return ERRCODE_NOT_ENOUGH_MEMORY;
		}
	}
	if ((NULL == program_info.fuse) && (program_info.fuse_size > 0))
	{
		program_info.fuse = malloc(program_info.fuse_size);
		if (NULL == program_info.fuse)
		{
			return ERRCODE_NOT_ENOUGH_MEMORY;
		}
	}
	if ((NULL == program_info.fuse_checksum) 
	   && (program_info.fuse_checksum_size > 0))
	{
		program_info.fuse_checksum = malloc(program_info.fuse_checksum_size);
		if (NULL == program_info.fuse_checksum)
		{
			return ERRCODE_NOT_ENOUGH_MEMORY;
		}
	}
	if ((NULL == program_info.lock) && (program_info.lock_size > 0))
	{
		program_info.lock = malloc(program_info.lock_size);
		if (NULL == program_info.lock)
		{
			return ERRCODE_NOT_ENOUGH_MEMORY;
		}
	}
	if ((NULL == program_info.lock_checksum) 
	   && (program_info.lock_checksum_size > 0))
	{
		program_info.lock_checksum = malloc(program_info.lock_checksum_size);
		if (NULL == program_info.lock_checksum)
		{
			return ERRCODE_NOT_ENOUGH_MEMORY;
		}
	}
	if ((NULL == program_info.user_area) && (program_info.user_area_size > 0))
	{
		program_info.user_area = malloc(program_info.user_area_size);
		if (NULL == program_info.user_area)
		{
			return ERRCODE_NOT_ENOUGH_MEMORY;
		}
	}
	if ((NULL == program_info.user_area_checksum) 
	   && (program_info.user_area_checksum_size > 0))
	{
		program_info.user_area_checksum = \
								malloc(program_info.user_area_checksum_size);
		if (NULL == program_info.user_area_checksum)
		{
			return ERRCODE_NOT_ENOUGH_MEMORY;
		}
	}
	
	return ERROR_OK;
}

void target_free_data_buffer(void)
{
	target_release_chip_series(&target_chips);
	
	if (program_info.boot != NULL)
	{
		free(program_info.boot);
		program_info.boot = NULL;
		program_info.boot_size = 0;
	}
	if (program_info.boot_checksum != NULL)
	{
		free(program_info.boot_checksum);
		program_info.boot_checksum = NULL;
		program_info.boot_checksum_size = 0;
	}
	if (program_info.boot_memlist != NULL)
	{
		MEMLIST_Free(&program_info.boot_memlist);
	}
	
	if (program_info.app != NULL)
	{
		free(program_info.app);
		program_info.app = NULL;
		program_info.app_size = 0;
	}
	if (program_info.app_checksum != NULL)
	{
		free(program_info.app_checksum);
		program_info.app_checksum = NULL;
		program_info.app_checksum_size = 0;
	}
	if (program_info.app_memlist != NULL)
	{
		MEMLIST_Free(&program_info.app_memlist);
	}
	
	if (program_info.otp_rom != NULL)
	{
		free(program_info.otp_rom);
		program_info.otp_rom = NULL;
		program_info.otp_rom_size = 0;
	}
	if (program_info.otp_rom_checksum != NULL)
	{
		free(program_info.otp_rom_checksum);
		program_info.otp_rom_checksum = NULL;
		program_info.otp_rom_checksum_size = 0;
	}
	if (program_info.opt_rom_memlist != NULL)
	{
		MEMLIST_Free(&program_info.opt_rom_memlist);
	}
	
	if (program_info.eeprom != NULL)
	{
		free(program_info.eeprom);
		program_info.eeprom = NULL;
		program_info.eeprom_size = 0;
	}
	if (program_info.eeprom_checksum != NULL)
	{
		free(program_info.eeprom_checksum);
		program_info.eeprom_checksum = NULL;
		program_info.eeprom_checksum_size = 0;
	}
	if (program_info.eeprom_memlist != NULL)
	{
		MEMLIST_Free(&program_info.eeprom_memlist);
	}
	
	if (program_info.fuse != NULL)
	{
		free(program_info.fuse);
		program_info.fuse = NULL;
		program_info.fuse_size = 0;
	}
	if (program_info.fuse_checksum != NULL)
	{
		free(program_info.fuse_checksum);
		program_info.fuse_checksum = NULL;
		program_info.fuse_checksum_size = 0;
	}
	if (program_info.fuse_memlist != NULL)
	{
		MEMLIST_Free(&program_info.fuse_memlist);
	}
	
	if (program_info.lock != NULL)
	{
		free(program_info.lock);
		program_info.lock = NULL;
		program_info.lock_size = 0;
	}
	if (program_info.lock_checksum != NULL)
	{
		free(program_info.lock_checksum);
		program_info.lock_checksum = NULL;
		program_info.lock_checksum_size = 0;
	}
	if (program_info.lock_memlist != NULL)
	{
		MEMLIST_Free(&program_info.lock_memlist);
	}
	
	if (program_info.user_area != NULL)
	{
		free(program_info.user_area);
		program_info.user_area = NULL;
		program_info.user_area_size = 0;
	}
	if (program_info.user_area_checksum != NULL)
	{
		free(program_info.user_area_checksum);
		program_info.user_area_checksum = NULL;
		program_info.user_area_checksum_size = 0;
	}
	if (program_info.user_area_memlist != NULL)
	{
		MEMLIST_Free(&program_info.user_area_memlist);
	}
}

void target_print_fl(char *type)
{
	chip_fl_t fl;
	uint32_t i, j;
	
	if (strcmp(type, "fuse") && strcmp(type, "lock"))
	{
		LOG_ERROR(_GETTEXT(ERRMSG_NOT_SUPPORT_BY), type, 
				  program_info.chip_name);
		return;
	}
	
	memset(&fl, 0, sizeof(chip_fl_t));
	target_build_chip_fl(program_info.chip_type, program_info.chip_name, 
						 type, &fl);
	
	// print fl
	printf("%s of %s:\n", type, program_info.chip_name);
	printf("init = 0x%08X, num_of_warnings = %d, num_of_settings = %d\n", 
		fl.init_value, fl.num_of_fl_warnings, fl.num_of_fl_settings);
	for (i = 0; i < fl.num_of_fl_warnings; i++)
	{
		printf("warning: mask = 0x%08X, value = 0x%08X, info = %s\n", 
			fl.warnings[i].mask, fl.warnings[i].value, fl.warnings[i].msg);
	}
	for (i = 0; i < fl.num_of_fl_settings; i++)
	{
		printf("setting: name = %s, mask = 0x%08X, num_of_choices = %d\n", 
			fl.settings[i].name, fl.settings[i].mask, fl.settings[i].num_of_choices);
		for (j = 0; j < fl.settings[i].num_of_choices; j++)
		{
			printf("chioce: value = 0x%08X, msg = %s\n", 
				fl.settings[i].choices[j].value, fl.settings[i].choices[j].msg);
		}
	}
	
	target_release_chip_fl(&fl);
}

void target_print_target(uint32_t i)
{
	target_build_chip_series(targets_info[i].name, 
							 targets_info[i].program_mode_str, 
							 &target_chips);
	targets_info[i].parse_argument('S', NULL);
	target_release_chip_series(&target_chips);
}

void target_print_list(void)
{
	uint32_t i;
	
	printf(_GETTEXT("Supported targets:\n"));
	for (i = 0; targets_info[i].name != NULL; i++)
	{
		target_print_target(i);
	}
}

void target_print_help(void)
{
	uint32_t i;
	
	for (i = 0; targets_info[i].name != NULL; i++)
	{
		targets_info[i].parse_argument('h', NULL);
	}
}

uint32_t target_get_number(void)
{
	return sizeof(targets_info) / sizeof(targets_info[0]) - 1;
}

RESULT target_init(program_info_t *pi)
{
	uint32_t i;
	
#if PARAM_CHECK
	if ((NULL == pi) || ((NULL == pi->chip_name) && (NULL == pi->chip_type)))
	{
		LOG_BUG(_GETTEXT(ERRMSG_INVALID_PARAMETER), __FUNCTION__);
		return ERRCODE_INVALID_PARAMETER;
	}
#endif
	
	target_release_chip_series(&target_chips);
	
	if (NULL == pi->chip_type)
	{
		// find which series of target contain current chip_name
		for (i = 0; targets_info[i].name != NULL; i++)
		{
			target_build_chip_series(targets_info[i].name, 
									 targets_info[i].program_mode_str, 
									 &target_chips);
			if (targets_info[i].probe_chip(pi->chip_name) == ERROR_OK)
			{
				cur_target = &targets_info[i];
				pi->chip_type = (char *)targets_info[i].name;
				LOG_DEBUG(_GETTEXT("%s initialized for %s.\n"), 
						 cur_target->name, pi->chip_name);
				
				return ERROR_OK;
			}
			target_release_chip_series(&target_chips);
		}
		
		LOG_ERROR(_GETTEXT(ERRMSG_NOT_SUPPORT), pi->chip_name);
	}
	else
	{
		// find current series of chip_type
		for (i = 0; targets_info[i].name != NULL; i++)
		{
			if (!strcmp(targets_info[i].name, pi->chip_type))
			{
				target_build_chip_series(targets_info[i].name, 
										 targets_info[i].program_mode_str, 
										 &target_chips);
				
				if ((pi->chip_name != NULL) 
				   && (ERROR_OK != targets_info[i].probe_chip(pi->chip_name)))
				{
					LOG_ERROR(_GETTEXT(ERRMSG_NOT_SUPPORT_BY), 
							  pi->chip_name, targets_info[i].name);
					target_release_chip_series(&target_chips);
					cur_target = NULL;
					return ERRCODE_NOT_SUPPORT;
				}
				else
				{
					cur_target = &targets_info[i];
					LOG_DEBUG(_GETTEXT("%s initialized.\n"), cur_target->name);
					return ERROR_OK;
				}
			}
		}
		
		LOG_ERROR(_GETTEXT(ERRMSG_NOT_SUPPORT), pi->chip_type);
	}
	
	cur_target = NULL;
	return ERRCODE_NOT_SUPPORT;
}

extern char *program_dir;
RESULT target_build_chip_fl(const char *chip_series, const char *chip_module, 
							char *type, chip_fl_t *fl)
{
	xmlDocPtr doc = NULL;
	xmlNodePtr curNode = NULL;
	char *filename = NULL;
	uint32_t i, j, k, num_of_chips;
	RESULT ret = ERROR_OK;
	uint8_t parsed;
	FILE *fp;
	uint32_t str_len;
	char *m;
	
#if PARAM_CHECK
	if ((NULL == chip_series) || (NULL == chip_module) || (NULL == fl))
	{
		LOG_BUG(_GETTEXT(ERRMSG_INVALID_PARAMETER), __FUNCTION__);
		return ERRCODE_INVALID_PARAMETER;
	}
#endif
	
	// release first if necessary
	target_release_chip_fl(fl);
	
	filename = (char *)malloc(strlen(program_dir)
							  + strlen(TARGET_CONF_FILE_PATH) 
							  + strlen(chip_series) 
							  + strlen(TARGET_CONF_FILE_EXT) + 1);
	if (NULL == filename)
	{
		LOG_ERROR(_GETTEXT(ERRMSG_NOT_ENOUGH_MEMORY));
		return ERRCODE_NOT_ENOUGH_MEMORY;
	}
	strcpy(filename, program_dir);
	strcat(filename, TARGET_CONF_FILE_PATH);
	strcat(filename, chip_series);
	strcat(filename, TARGET_CONF_FILE_EXT);
	fp = fopen(filename, "r");
	if (NULL == fp)
	{
		// no error message, just return error
		ret = ERROR_FAIL;
		goto free_and_exit;
	}
	else
	{
		fclose(fp);
		fp = NULL;
	}
	
	doc = xmlReadFile(filename, "", XML_PARSE_RECOVER);
	if (NULL == doc)
	{
		LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPEN), filename);
		ret = ERRCODE_FAILURE_OPEN;
		goto free_and_exit;
	}
	curNode = xmlDocGetRootElement(doc);
	if (NULL == curNode)
	{
		LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), "read config file");
		ret = ERRCODE_FAILURE_OPERATION;
		goto free_and_exit;
	}
	
	// valid check
	if (xmlStrcmp(curNode->name, BAD_CAST "series") 
		|| !xmlHasProp(curNode, BAD_CAST "name") 
		|| xmlStrcmp(xmlGetProp(curNode, BAD_CAST "name"), 
					 (const xmlChar *)chip_series) 
		|| !xmlHasProp(curNode, BAD_CAST "number"))
	{
		LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), "read config file");
		ret = ERRCODE_FAILURE_OPERATION;
		goto free_and_exit;
	}
	
	num_of_chips = 
		strtoul((const char *)xmlGetProp(curNode, BAD_CAST "number"), NULL, 0);
	if (0 == num_of_chips)
	{
		LOG_ERROR(_GETTEXT(ERRMSG_NOT_SUPPORT), chip_series);
		ret = ERRCODE_NOT_SUPPORT;
		goto free_and_exit;
	}
	
	parsed = 0;
	// read data
	curNode = curNode->children->next;
	for (i = 0; !parsed && (i < num_of_chips); i++)
	{
		xmlNodePtr paramNode, settingNode;
		
		// check
		if ((NULL == curNode) 
			|| xmlStrcmp(curNode->name, BAD_CAST "chip")
			|| !xmlHasProp(curNode, BAD_CAST "name"))
		{
			LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), "read config file");
			ret = ERRCODE_FAILURE_OPERATION;
			goto free_and_exit;
		}
		
		if (strcmp((const char *)chip_module, 
				   (const char *)xmlGetProp(curNode, BAD_CAST "name")))
		{
			// not the chip I want
			curNode = curNode->next->next;
			continue;
		}
		
		paramNode = curNode->children->next;
		// read parameters
		while(!parsed && (paramNode != NULL))
		{
			if (!xmlStrcmp(paramNode->name, BAD_CAST type))
			{
				// we found the parameter
				// valid check
				if (!xmlHasProp(paramNode, BAD_CAST "number") 
					|| !xmlHasProp(paramNode, BAD_CAST "init"))
				{
					LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), 
							  "read config file");
					ret = ERRCODE_FAILURE_OPERATION;
					goto free_and_exit;
				}
				
				// read fl number
				fl->num_of_fl_settings = 
					(uint16_t)strtoul((const char *)xmlGetProp(paramNode, BAD_CAST "number"), NULL, 0);
				if (0 == fl->num_of_fl_settings)
				{
					LOG_ERROR(_GETTEXT(ERRMSG_NOT_SUPPORT_BY), type, 
							  chip_module);
					ret = ERRCODE_NOT_SUPPORT;
					goto free_and_exit;
				}
				
				// read fl init value
				fl->init_value = 
					strtoul((const char *)xmlGetProp(paramNode, BAD_CAST "init"), NULL, 0);
				
				// alloc memory for settings
				fl->settings = (chip_fl_setting_t*)malloc(
					fl->num_of_fl_settings * sizeof(chip_fl_setting_t));
				if (NULL == fl->settings)
				{
					LOG_ERROR(_GETTEXT(ERRMSG_NOT_ENOUGH_MEMORY));
					ret = ERRCODE_NOT_ENOUGH_MEMORY;
					goto free_and_exit;
				}
				
				settingNode = paramNode->children->next;
				// has warning?
				if ((settingNode != NULL) && 
					!strcmp((const char *)settingNode->name, "warning"))
				{
					xmlNodePtr warningNode = settingNode;
					xmlNodePtr wNode;
					
					settingNode = settingNode->next->next;
					// parse warning
					fl->num_of_fl_warnings = 
						(uint16_t)strtoul((const char *)xmlGetProp(warningNode, BAD_CAST "number"), NULL, 0);
					if (fl->num_of_fl_warnings != 0)
					{
						fl->warnings = (chip_fl_warning_t*)malloc(
							fl->num_of_fl_warnings * sizeof(chip_fl_warning_t));
						if (NULL == fl->warnings)
						{
							LOG_ERROR(_GETTEXT(ERRMSG_NOT_ENOUGH_MEMORY));
							ret = ERRCODE_NOT_ENOUGH_MEMORY;
							goto free_and_exit;
						}
						
						wNode = warningNode->children->next;
						for (j = 0; j < fl->num_of_fl_warnings; j++)
						{
							// check
							if (strcmp((const char *)wNode->name, "w"))
							{
								LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), 
										  "read config file");
								ret = ERRCODE_FAILURE_OPERATION;
								goto free_and_exit;
							}
							
							fl->warnings[j].mask = 
								strtoul((const char *)xmlGetProp(wNode, BAD_CAST "mask"), NULL, 0);
							fl->warnings[j].value = 
								strtoul((const char *)xmlGetProp(wNode, BAD_CAST "value"), NULL, 0);
							m = (char *)xmlNodeGetContent(wNode->children);
							str_len = strlen(m);
							fl->warnings[j].msg = (char *)malloc(str_len + 1);
							if (NULL == fl->warnings[j].msg)
							{
								LOG_ERROR(_GETTEXT(ERRMSG_NOT_ENOUGH_MEMORY));
								ret = ERRCODE_NOT_ENOUGH_MEMORY;
								goto free_and_exit;
							}
							strcpy(fl->warnings[j].msg, (const char *)m);
							
							wNode = wNode->next->next;
						}
					}
				}
				
				// parse settings
				for (j = 0; j < fl->num_of_fl_settings; j++)
				{
					xmlNodePtr choiceNode;
					
					// check
					if (strcmp((const char *)settingNode->name, "setting"))
					{
						LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), 
								  "read config file");
						ret = ERRCODE_FAILURE_OPERATION;
						goto free_and_exit;
					}
					
					fl->settings[j].num_of_choices = 
						(uint16_t)strtoul((const char *)xmlGetProp(settingNode, BAD_CAST "number"), NULL, 0);
					if (0 == fl->settings[j].num_of_choices)
					{
						// why this setting exists? Ignore
						LOG_WARNING(_GETTEXT("Settings has no choice.\n"));
						continue;
					}
					
					// malloc memory for choices
					fl->settings[j].choices = 
						(chip_fl_choice_t*)malloc(fl->settings[j].num_of_choices * sizeof(chip_fl_choice_t));
					if (NULL == fl->settings[j].choices)
					{
						LOG_ERROR(_GETTEXT(ERRMSG_NOT_ENOUGH_MEMORY));
						ret = ERRCODE_NOT_ENOUGH_MEMORY;
						goto free_and_exit;
					}
					
					// parse
					m = (char *)xmlGetProp(settingNode, BAD_CAST "name");
					str_len = strlen(m);
					fl->settings[j].name = (char *)malloc(str_len + 1);
					if (NULL == fl->settings[j].name)
					{
						LOG_ERROR(_GETTEXT(ERRMSG_NOT_ENOUGH_MEMORY));
						ret = ERRCODE_NOT_ENOUGH_MEMORY;
						goto free_and_exit;
					}
					strcpy(fl->settings[j].name, m);
					fl->settings[j].mask = 
						strtoul((const char *)xmlGetProp(settingNode, BAD_CAST "mask"), NULL, 0);
					
					choiceNode = settingNode->children->next;
					// parse choices
					for (k = 0; k < fl->settings[j].num_of_choices; k++)
					{
						// check
						if (strcmp((const char *)choiceNode->name, "choice"))
						{
							LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), 
									  "read config file");
							ret = ERRCODE_FAILURE_OPERATION;
							goto free_and_exit;
						}
						
						// parse
						fl->settings[j].choices[k].value = 
							strtoul((const char *)xmlGetProp(choiceNode, BAD_CAST "value"), NULL, 0);
						m = (char *)xmlNodeGetContent(choiceNode->children);
						str_len = strlen(m);
						fl->settings[j].choices[k].msg = (char *)malloc(str_len + 1);
						if (NULL == fl->settings[j].choices[k].msg)
						{
							LOG_ERROR(_GETTEXT(ERRMSG_NOT_ENOUGH_MEMORY));
							ret = ERRCODE_NOT_ENOUGH_MEMORY;
							goto free_and_exit;
						}
						strcpy(fl->settings[j].choices[k].msg, m);
						
						choiceNode = choiceNode->next->next;
					}
					settingNode = settingNode->next->next;
				}
				
				parsed = 1;
				break;
			}
			
			paramNode = paramNode->next->next;
		}
		
		curNode = curNode->next->next;
	}
	
free_and_exit:
	if (filename != NULL)
	{
		free(filename);
		filename = NULL;
	}
	if (doc != NULL)
	{
		xmlFreeDoc(doc);
		doc = NULL;
	}
	
	return ret;
}

RESULT target_release_chip_fl(chip_fl_t *fl)
{
	uint32_t i, j;
	
	if (NULL == fl)
	{
		LOG_BUG(_GETTEXT(ERRMSG_INVALID_PARAMETER), __FUNCTION__);
		return ERRCODE_INVALID_PARAMETER;
	}
	
	// free warnings
	if (fl->warnings != NULL)
	{
		for (i = 0; i < fl->num_of_fl_warnings; i++)
		{
			if (fl->warnings[i].msg != NULL)
			{
				free(fl->warnings[i].msg);
				fl->warnings[i].msg = NULL;
			}
		}
		free(fl->warnings);
		fl->warnings = NULL;
	}
	
	// free settings
	if (fl->settings != NULL)
	{
		for (i = 0; i < fl->num_of_fl_settings; i++)
		{
			if (fl->settings[i].name != NULL)
			{
				free(fl->settings[i].name);
				fl->settings[i].name = NULL;
			}
			if (fl->settings[i].choices != NULL)
			{
				for (j = 0; j < fl->settings[i].num_of_choices; j++)
				{
					if (fl->settings[i].choices[j].msg != NULL)
					{
						free(fl->settings[i].choices[j].msg);
						fl->settings[i].choices[j].msg = NULL;
					}
				}
				free(fl->settings[i].choices);
				fl->settings[i].choices = NULL;
			}
		}
		free(fl->settings);
		fl->settings = NULL;
	}
	
	return ERROR_OK;
}

RESULT target_build_chip_series(const char *chip_series, 
								const char *program_mode, chip_series_t *s)
{
	xmlDocPtr doc = NULL;
	xmlNodePtr curNode = NULL;
	char *filename = NULL;
	uint32_t i, j;
	RESULT ret = ERROR_OK;
	FILE *fp;
	
#if PARAM_CHECK
	if ((NULL == chip_series) || (NULL == s))
	{
		LOG_BUG(_GETTEXT(ERRMSG_INVALID_PARAMETER), __FUNCTION__);
		return ERRCODE_INVALID_PARAMETER;
	}
#endif
	
	// release first if necessary
	target_release_chip_series(s);
	
	filename = (char *)malloc(strlen(program_dir)
							  + strlen(TARGET_CONF_FILE_PATH) 
							  + strlen(chip_series) 
							  + strlen(TARGET_CONF_FILE_EXT) + 1);
	if (NULL == filename)
	{
		LOG_ERROR(_GETTEXT(ERRMSG_NOT_ENOUGH_MEMORY));
		return ERRCODE_NOT_ENOUGH_MEMORY;
	}
	strcpy(filename, program_dir);
	strcat(filename, TARGET_CONF_FILE_PATH);
	strcat(filename, chip_series);
	strcat(filename, TARGET_CONF_FILE_EXT);
	fp = fopen(filename, "r");
	if (NULL == fp)
	{
		// no error message, just return error
		ret = ERROR_FAIL;
		goto free_and_exit;
	}
	else
	{
		fclose(fp);
		fp = NULL;
	}
	
	doc = xmlReadFile(filename, "", XML_PARSE_RECOVER);
	if (NULL == doc)
	{
		LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPEN), filename);
		ret = ERRCODE_FAILURE_OPEN;
		goto free_and_exit;
	}
	curNode = xmlDocGetRootElement(doc);
	if (NULL == curNode)
	{
		LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), "read config file");
		ret = ERRCODE_FAILURE_OPERATION;
		goto free_and_exit;
	}
	
	// valid check
	if (xmlStrcmp(curNode->name, BAD_CAST "series") 
		|| !xmlHasProp(curNode, BAD_CAST "name") 
		|| xmlStrcmp(xmlGetProp(curNode, BAD_CAST "name"), 
					 (const xmlChar *)chip_series) 
		|| !xmlHasProp(curNode, BAD_CAST "number"))
	{
		LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), "read config file");
		ret = ERRCODE_FAILURE_OPERATION;
		goto free_and_exit;
	}
	
	s->num_of_chips = strtoul((const char *)xmlGetProp(curNode, BAD_CAST "number"), NULL, 0);
	if (0 == s->num_of_chips)
	{
		LOG_ERROR(_GETTEXT(ERRMSG_NOT_SUPPORT), chip_series);
		ret = ERRCODE_NOT_SUPPORT;
		goto free_and_exit;
	}
	s->chips_param = (chip_param_t *)malloc(sizeof(chip_param_t) 
											* s->num_of_chips);
	if (NULL == s->chips_param)
	{
		LOG_ERROR(_GETTEXT(ERRMSG_NOT_ENOUGH_MEMORY));
		ret = ERRCODE_NOT_ENOUGH_MEMORY;
		goto free_and_exit;
	}
	
	// read data
	curNode = curNode->children->next;
	for (i = 0; i < s->num_of_chips; i++)
	{
		xmlNodePtr paramNode;
		
		// check
		if ((NULL == curNode) 
			|| xmlStrcmp(curNode->name, BAD_CAST "chip")
			|| !xmlHasProp(curNode, BAD_CAST "name"))
		{
			LOG_ERROR(_GETTEXT(ERRMSG_FAILURE_OPERATION), "read config file");
			ret = ERRCODE_FAILURE_OPERATION;
			goto free_and_exit;
		}
		
		// read name
		strncpy(s->chips_param[i].chip_name, 
				(const char *)xmlGetProp(curNode, BAD_CAST "name"), 
				sizeof(s->chips_param[i].chip_name));
		
		// read parameters
		paramNode = curNode->children->next;
		while(paramNode != NULL)
		{
			if (!xmlStrcmp(paramNode->name, BAD_CAST "chip_id"))
			{
				s->chips_param[i].chip_id = (uint32_t)strtoul(
								(const char *)xmlNodeGetContent(paramNode), 
								NULL, 0);
			}
			else if (!xmlStrcmp(paramNode->name, BAD_CAST "program_mode"))
			{
				char *mode_tmp = (char *)xmlNodeGetContent(paramNode);
				char *first;
				
				s->chips_param[i].program_mode = 0;
				for (j = 0; j < strlen(mode_tmp); j++)
				{
					first = strchr(program_mode, mode_tmp[j]);
					if (first != NULL)
					{
						s->chips_param[i].program_mode |= 
												1 << (first - program_mode);
					}
					else
					{
						LOG_ERROR(_GETTEXT(ERRMSG_NOT_SUPPORT_BY), mode_tmp, 
								  "program_mode of current target");
						ret = ERRCODE_NOT_SUPPORT;
						goto free_and_exit;
					}
				}
			}
			else if (!xmlStrcmp(paramNode->name, BAD_CAST "boot_page_size"))
			{
				s->chips_param[i].boot_page_size = (uint32_t)strtoul(
								(const char *)xmlNodeGetContent(paramNode), 
								NULL, 0);
			}
			else if (!xmlStrcmp(paramNode->name, BAD_CAST "boot_page_num"))
			{
				s->chips_param[i].boot_page_num = (uint32_t)strtoul(
								(const char *)xmlNodeGetContent(paramNode), 
								NULL, 0);
			}
			else if (!xmlStrcmp(paramNode->name, BAD_CAST "app_page_size"))
			{
				s->chips_param[i].app_page_size = (uint32_t)strtoul(
								(const char *)xmlNodeGetContent(paramNode), 
								NULL, 0);
			}
			else if (!xmlStrcmp(paramNode->name, BAD_CAST "app_page_num"))
			{
				s->chips_param[i].app_page_num = (uint32_t)strtoul(
								(const char *)xmlNodeGetContent(paramNode), 
								NULL, 0);
			}
			else if (!xmlStrcmp(paramNode->name, BAD_CAST "ee_page_size"))
			{
				s->chips_param[i].ee_page_size = (uint32_t)strtoul(
								(const char *)xmlNodeGetContent(paramNode), 
								NULL, 0);
			}
			else if (!xmlStrcmp(paramNode->name, BAD_CAST "ee_page_num"))
			{
				s->chips_param[i].ee_page_num = (uint32_t)strtoul(
								(const char *)xmlNodeGetContent(paramNode), 
								NULL, 0);
			}
			else if (!xmlStrcmp(paramNode->name, BAD_CAST "optrom_page_size"))
			{
				s->chips_param[i].optrom_page_size = (uint32_t)strtoul(
								(const char *)xmlNodeGetContent(paramNode), 
								NULL, 0);
			}
			else if (!xmlStrcmp(paramNode->name, BAD_CAST "optrom_page_num"))
			{
				s->chips_param[i].optrom_page_num = (uint32_t)strtoul(
								(const char *)xmlNodeGetContent(paramNode), 
								NULL, 0);
			}
			else if (!xmlStrcmp(paramNode->name, BAD_CAST "usrsig_page_size"))
			{
				s->chips_param[i].usrsig_page_size = (uint32_t)strtoul(
								(const char *)xmlNodeGetContent(paramNode), 
								NULL, 0);
			}
			else if (!xmlStrcmp(paramNode->name, BAD_CAST "usrsig_page_num"))
			{
				s->chips_param[i].usrsig_page_num = (uint32_t)strtoul(
								(const char *)xmlNodeGetContent(paramNode), 
								NULL, 0);
			}
			else if (!xmlStrcmp(paramNode->name, BAD_CAST "fuse_size"))
			{
				s->chips_param[i].fuse_size = (uint32_t)strtoul(
								(const char *)xmlNodeGetContent(paramNode), 
								NULL, 0);
			}
			else if (!xmlStrcmp(paramNode->name, BAD_CAST "lock_size"))
			{
				s->chips_param[i].lock_size = (uint32_t)strtoul(
								(const char *)xmlNodeGetContent(paramNode), 
								NULL, 0);
			}
			else if (!xmlStrcmp(paramNode->name, BAD_CAST "boot_size"))
			{
				s->chips_param[i].boot_size = (uint32_t)strtoul(
								(const char *)xmlNodeGetContent(paramNode), 
								NULL, 0);
			}
			else if (!xmlStrcmp(paramNode->name, BAD_CAST "app_size"))
			{
				s->chips_param[i].app_size = (uint32_t)strtoul(
								(const char *)xmlNodeGetContent(paramNode), 
								NULL, 0);
			}
			else if (!xmlStrcmp(paramNode->name, BAD_CAST "ee_size"))
			{
				s->chips_param[i].ee_size = (uint32_t)strtoul(
								(const char *)xmlNodeGetContent(paramNode), 
								NULL, 0);
			}
			else if (!xmlStrcmp(paramNode->name, BAD_CAST "optrom_size"))
			{
				s->chips_param[i].optrom_size = (uint32_t)strtoul(
								(const char *)xmlNodeGetContent(paramNode), 
								NULL, 0);
			}
			else if (!xmlStrcmp(paramNode->name, BAD_CAST "usrsig_size"))
			{
				s->chips_param[i].usrsig_size = (uint32_t)strtoul(
								(const char *)xmlNodeGetContent(paramNode), 
								NULL, 0);
			}
			else if (!xmlStrcmp(paramNode->name, BAD_CAST "fuse"))
			{
				// No need to parse fuse here
			}
			else if (!xmlStrcmp(paramNode->name, BAD_CAST "lock"))
			{
				// No need to parse lock here
			}
			else
			{
				char *str_tmp = (char *)paramNode->name;
				
				if ((strlen(str_tmp) >= 6) 
					&& ('p' == str_tmp[0]) 
					&& ('a' == str_tmp[1])
					&& ('r' == str_tmp[2])
					&& ('a' == str_tmp[3])
					&& ('m' == str_tmp[4]))
				{
					// parameters
					j = strtoul(&str_tmp[5], NULL, 0);
					s->chips_param[i].param[j] = (uint32_t)strtoul(
								(const char *)xmlNodeGetContent(paramNode), 
								NULL, 0);
				}
				else
				{
					// wrong parameter
					LOG_ERROR(_GETTEXT(ERRMSG_INVALID), 
							  (const char *)xmlNodeGetContent(paramNode), 
							  chip_series);
					ret = ERRCODE_INVALID;
					goto free_and_exit;
				}
			}
			
			paramNode = paramNode->next->next;
		}
		
		curNode = curNode->next->next;
	}
	
free_and_exit:
	if (filename != NULL)
	{
		free(filename);
		filename = NULL;
	}
	if (doc != NULL)
	{
		xmlFreeDoc(doc);
		doc = NULL;
	}
	
	return ret;
}

RESULT target_release_chip_series(chip_series_t *s)
{
	if ((s != NULL) && ((s->num_of_chips > 0) || (s->chips_param != NULL)))
	{
		free(s->chips_param);
		s->chips_param = NULL;
		s->num_of_chips = 0;
	}
	
	return ERROR_OK;
}

