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

#include "app_cfg.h"
#include "app_type.h"
#include "app_io.h"

#include "../mal/mal.h"
#include "../mal/mal_internal.h"
#include "df25xx_drv_cfg.h"
#include "df25xx_drv.h"

#define DF25XX_CMD_WRSR						0x01	//write status register 
#define DF25XX_CMD_PGWR						0x02	//page program
#define DF25XX_CMD_PGRD						0x03	//read data
#define DF25XX_CMD_WRDI						0x04	//write disable
#define DF25XX_CMD_RDSR						0x05	//read status register 1(7 to 0)
#define DF25XX_CMD_WREN						0x06	//write enable
#define DF25XX_CMD_ER4K						0x20	//sector erase
#define DF25XX_CMD_CHER						0x60	//chip erase
#define DF25XX_CMD_RDID						0x9F	//jedec id

#define DF25XX_STATE_BUSY					(1 << 0)
#define DF25XX_STATE_WEL					(1 << 1)

static struct df25xx_drv_interface_t df25xx_drv_ifs;
static struct df25xx_drv_param_t df25xx_drv_param;

static RESULT df25xx_drv_cs_assert(void)
{
	return interfaces->gpio.config(df25xx_drv_ifs.cs_port, 
		df25xx_drv_ifs.cs_pin, df25xx_drv_ifs.cs_pin, 0, 0);
}

static RESULT df25xx_drv_cs_deassert(void)
{
	return interfaces->gpio.config(df25xx_drv_ifs.cs_port, 
		df25xx_drv_ifs.cs_pin, 0, df25xx_drv_ifs.cs_pin, df25xx_drv_ifs.cs_pin);
}

static RESULT df25xx_drv_io(uint8_t *out, uint8_t *in, uint16_t len)
{
	df25xx_drv_cs_assert();
	interfaces->spi.io(df25xx_drv_ifs.spi_port, out, in, len);
	df25xx_drv_cs_deassert();
	return ERROR_OK;
}

static RESULT df25xx_drv_config_interface(void *ifs)
{
	if (NULL == ifs)
	{
		return ERROR_FAIL;
	}
	
	memcpy(&df25xx_drv_ifs, ifs, sizeof(df25xx_drv_ifs));
	return ERROR_OK;
}

static RESULT df25xx_drv_init(void *param)
{
	if (NULL == param)
	{
		return ERROR_FAIL;
	}
	memcpy(&df25xx_drv_param, param, sizeof(df25xx_drv_param));
	
	if (!df25xx_drv_param.spi_khz)
	{
		df25xx_drv_param.spi_khz = 9000;
	}
	interfaces->gpio.init(df25xx_drv_ifs.cs_port);
	interfaces->gpio.config(df25xx_drv_ifs.cs_port, df25xx_drv_ifs.cs_pin, 0, 
							df25xx_drv_ifs.cs_pin, df25xx_drv_ifs.cs_pin);
	interfaces->spi.init(df25xx_drv_ifs.spi_port);
	interfaces->spi.config(df25xx_drv_ifs.spi_port, df25xx_drv_param.spi_khz, 
							SPI_CPOL_HIGH, SPI_CPHA_2EDGE, SPI_MSB_FIRST);
	
	return ERROR_OK;
}

static RESULT df25xx_drv_getinfo(void *info)
{
	struct df25xx_drv_info_t *pinfo = (struct df25xx_drv_info_t *)info;
	uint8_t out_buff[13], in_buff[13];
	
	out_buff[0] = DF25XX_CMD_RDID;
	df25xx_drv_io(out_buff, in_buff, 4);
	if (ERROR_OK != interfaces->peripheral_commit())
	{
		return ERROR_FAIL;
	}
	
	pinfo->manufacturer_id = in_buff[1];
	pinfo->device_id = GET_BE_U16(&in_buff[2]);
	return ERROR_OK;
}

static RESULT df25xx_drv_fini(void)
{
	interfaces->gpio.fini(df25xx_drv_ifs.cs_port);
	interfaces->spi.fini(df25xx_drv_ifs.spi_port);
	return ERROR_OK;
}

static RESULT df25xx_drv_eraseall_nb_start(void)
{
	uint8_t buff[2];
	
	df25xx_drv_cs_assert();
	buff[0] = DF25XX_CMD_WREN;
	buff[1] = DF25XX_CMD_CHER;
	interfaces->spi.io(df25xx_drv_ifs.spi_port, buff, NULL, 2);
	return df25xx_drv_cs_deassert();
}

static RESULT df25xx_drv_eraseall_nb_isready(void)
{
	uint8_t buff[2];
	
	df25xx_drv_cs_assert();
	buff[0] = DF25XX_CMD_RDSR;
	interfaces->spi.io(df25xx_drv_ifs.spi_port, buff, buff, 2);
	df25xx_drv_cs_deassert();
	
	if ((ERROR_OK != interfaces->peripheral_commit()) || 
		(buff[1] & DF25XX_STATE_BUSY))
	{
		return ERROR_FAIL;
	}
	return ERROR_OK;
}

static RESULT df25xx_drv_eraseall_nb_end(void)
{
	uint8_t buff[1];
	
	df25xx_drv_cs_assert();
	buff[0] = DF25XX_CMD_WRDI;
	interfaces->spi.io(df25xx_drv_ifs.spi_port, buff, NULL, 1);
	return df25xx_drv_cs_deassert();
}

static RESULT df25xx_drv_eraseblock_nb_start(uint64_t address, uint64_t count)
{
	uint8_t buff[1];
	
	REFERENCE_PARAMETER(count);
	REFERENCE_PARAMETER(address);
	
	df25xx_drv_cs_assert();
	buff[0] = DF25XX_CMD_WREN;
	interfaces->spi.io(df25xx_drv_ifs.spi_port, buff, NULL, 1);
	return df25xx_drv_cs_deassert();
}

static RESULT df25xx_drv_eraseblock_nb(uint64_t address)
{
	uint8_t buff[4];
	
	df25xx_drv_cs_assert();
	buff[0] = DF25XX_CMD_ER4K;
	buff[1] = (address >> 16) & 0xFF;
	buff[2] = (address >> 8 ) & 0xFF;
	buff[3] = (address >> 0 ) & 0xFF;
	interfaces->spi.io(df25xx_drv_ifs.spi_port, buff, NULL, 4);
	return df25xx_drv_cs_deassert();
}

static RESULT df25xx_drv_eraseblock_nb_isready(void)
{
	uint8_t buff[2];
	
	df25xx_drv_cs_assert();
	buff[0] = DF25XX_CMD_RDSR;
	interfaces->spi.io(df25xx_drv_ifs.spi_port, buff, buff, 2);
	df25xx_drv_cs_deassert();
	
	if ((ERROR_OK != interfaces->peripheral_commit()) || 
		(buff[1] & DF25XX_STATE_BUSY))
	{
		return ERROR_FAIL;
	}
	return ERROR_OK;
}

static RESULT df25xx_drv_eraseblock_nb_end(void)
{
	uint8_t buff[1];
	
	df25xx_drv_cs_assert();
	buff[0] = DF25XX_CMD_WRDI;
	interfaces->spi.io(df25xx_drv_ifs.spi_port, buff, NULL, 1);
	return df25xx_drv_cs_deassert();
}

static RESULT df25xx_drv_readblock_nb_start(uint64_t address, uint64_t count)
{
	uint8_t buff[4];
	
	REFERENCE_PARAMETER(count);
	
	df25xx_drv_cs_assert();
	buff[0] = DF25XX_CMD_PGRD;
	buff[1] = (address >> 16) & 0xFF;
	buff[2] = (address >> 8 ) & 0xFF;
	buff[3] = (address >> 0 ) & 0xFF;
	return interfaces->spi.io(df25xx_drv_ifs.spi_port, buff, NULL, 4);
}

static RESULT df25xx_drv_readblock_nb(uint64_t address, uint8_t *buff)
{
	REFERENCE_PARAMETER(address);
	return interfaces->spi.io(df25xx_drv_ifs.spi_port, NULL, buff, 
						(uint16_t)df25xx_drv.capacity.block_size);
}

static RESULT df25xx_drv_readblock_nb_isready(void)
{
	return ERROR_OK;
}

static RESULT df25xx_drv_readblock_nb_end(void)
{
	return df25xx_drv_cs_deassert();
}

static RESULT df25xx_drv_writeblock_nb_start(uint64_t address, uint64_t count)
{
	uint8_t buff[1];
	
	REFERENCE_PARAMETER(count);
	REFERENCE_PARAMETER(address);
	
	df25xx_drv_cs_assert();
	buff[0] = DF25XX_CMD_WREN;
	interfaces->spi.io(df25xx_drv_ifs.spi_port, buff, NULL, 1);
	return df25xx_drv_cs_deassert();
}

static RESULT df25xx_drv_writeblock_nb(uint64_t address, uint8_t *buff)
{
	uint8_t cmd[4];
	
	df25xx_drv_cs_assert();
	cmd[0] = DF25XX_CMD_PGWR;
	cmd[1] = (address >> 16) & 0xFF;
	cmd[2] = (address >> 8 ) & 0xFF;
	cmd[3] = (address >> 0 ) & 0xFF;
	interfaces->spi.io(df25xx_drv_ifs.spi_port, cmd, NULL, 4);
	interfaces->spi.io(df25xx_drv_ifs.spi_port, buff, NULL, 
						(uint16_t)df25xx_drv.capacity.block_size);
	return df25xx_drv_cs_deassert();
}

static RESULT df25xx_drv_writeblock_nb_isready(void)
{
	uint8_t buff[2];
	
	df25xx_drv_cs_assert();
	buff[0] = DF25XX_CMD_RDSR;
	interfaces->spi.io(df25xx_drv_ifs.spi_port, buff, buff, 2);
	df25xx_drv_cs_deassert();
	
	if ((ERROR_OK != interfaces->peripheral_commit()) || 
		(buff[1] & DF25XX_STATE_BUSY))
	{
		return ERROR_FAIL;
	}
	return ERROR_OK;
}

static RESULT df25xx_drv_writeblock_nb_end(void)
{
	uint8_t buff[1];
	
	df25xx_drv_cs_assert();
	buff[0] = DF25XX_CMD_WRDI;
	interfaces->spi.io(df25xx_drv_ifs.spi_port, buff, NULL, 1);
	return df25xx_drv_cs_deassert();
}

#if DAL_INTERFACE_PARSER_EN
static RESULT df25xx_drv_parse_interface(uint8_t *buff)
{
	if (NULL == buff)
	{
		return ERROR_FAIL;
	}
	df25xx_drv_ifs.spi_port = buff[0];
	df25xx_drv_ifs.cs_port = buff[1];
	df25xx_drv_ifs.cs_pin = *(uint32_t *)&buff[2];
	return ERROR_OK;
}
#endif

struct mal_driver_t df25xx_drv = 
{
	{
		"df25xx",
#if DAL_INTERFACE_PARSER_EN
		"spi:%1dcs:%1d,%4x",
		df25xx_drv_parse_interface,
#endif
		df25xx_drv_config_interface,
	},
	
	MAL_IDX_DF25XX,
	MAL_SUPPORT_READBLOCK | MAL_SUPPORT_WRITEBLOCK | MAL_SUPPORT_ERASEBLOCK,
	{0, 0},
	
	df25xx_drv_init,
	df25xx_drv_fini,
	df25xx_drv_getinfo,
	NULL,
	
	df25xx_drv_eraseall_nb_start,
	df25xx_drv_eraseall_nb_isready,
	NULL,
	df25xx_drv_eraseall_nb_end,
	
	df25xx_drv_eraseblock_nb_start,
	df25xx_drv_eraseblock_nb,
	df25xx_drv_eraseblock_nb_isready,
	NULL,
	df25xx_drv_eraseblock_nb_end,
	
	df25xx_drv_readblock_nb_start,
	df25xx_drv_readblock_nb,
	df25xx_drv_readblock_nb_isready,
	NULL,
	df25xx_drv_readblock_nb_end,
	
	df25xx_drv_writeblock_nb_start,
	df25xx_drv_writeblock_nb,
	df25xx_drv_writeblock_nb_isready,
	NULL,
	df25xx_drv_writeblock_nb_end
};

