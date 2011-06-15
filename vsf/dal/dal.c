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

#include "app_cfg.h"
#include "app_type.h"

#include "interfaces.h"
#include "dal.h"

#include "dal/mal/mal.h"

#if DAL_MIC2826_EN
#include "dal/mic2826/mic2826_drv.h"
#endif

#if DAL_NRF24L01_EN
#include "nrf24l01/nrf24l01_drv.h"
#endif

struct dal_driver_t *dal_drivers[] = 
{
#if DAL_EE93CX6_EN
	(struct dal_driver_t *)&ee93cx6_drv,
#endif
#if DAL_EE24CXX_EN
	(struct dal_driver_t *)&ee24cxx_drv,
#endif
#if DAL_DF25XX_EN
	(struct dal_driver_t *)&df25xx_drv,
#endif
#if DAL_DF45XX_EN
	(struct dal_driver_t *)&df45xx_drv,
#endif
#if DAL_SD_SPI_EN
	(struct dal_driver_t *)&sd_spi_drv,
#endif
#if DAL_SD_SDIO_EN
	(struct dal_driver_t *)&sd_sdio_drv,
#endif
#if DAL_MIC2826_EN
	(struct dal_driver_t *)&mic2826_drv,
#endif
#if DAL_NRF24L01_EN
	(struct dal_driver_t *)&nrf24l01_drv,
#endif
	NULL
};

