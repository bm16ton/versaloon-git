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
#ifndef __VERSALOON_INTERNAL_H_INCLUDED__
#define __VERSALOON_INTERNAL_H_INCLUDED__

#define VERSALOON_VID					0x0483
#define VERSALOON_PID					0x5740
#define VERSALOON_INP					0x82
#define VERSALOON_OUTP					0x03

#define VERSALOON_FULL					1
#define VERSALOON_MINI					2
#define VERSALOON_NANO					3

#define VERSALOON_TIMEOUT				5000

// USB Commands
// Common Commands
#define VERSALOON_COMMON_CMD_START		0x00
#define VERSALOON_COMMON_CMD_END		0x0F

#define VERSALOON_GET_INFO				0x00
#define VERSALOON_GET_TVCC				0x01
#define VERSALOON_GET_HARDWARE			0x02
#define VERSALOON_GET_OFFLINE_SIZE		0x08
#define VERSALOON_ERASE_OFFLINE_DATA	0x09
#define VERSALOON_WRITE_OFFLINE_DATA	0x0A
#define VERSALOON_GET_OFFLINE_CHECKSUM	0x0B
#define VERSALOON_FW_UPDATE				0x0F

// MCU Command
#define VERSALOON_MCU_CMD_START			0x10
#define VERSALOON_MCU_CMD_END			0x1F

// USB_TO_XXX Command
#define VERSALOON_USB_TO_XXX_CMD_START	0x20
#define VERSALOON_USB_TO_XXX_CMD_END	0x7F

// VSLLink Command
#define VERSALOON_VSLLINK_CMD_START		0x80
#define VERSALOON_VSLLINK_CMD_END		0xFF



// Mass-product
#define MP_OK							0x00
#define MP_FAIL							0x01

#define MP_ISSP							0x11



// interfaces
#define VERSALOON_SPI_PORT				0
#define VERSALOON_GPIO_PORT				0
#define VERSALOON_ISSP_PORT				0
#define VERSALOON_JTAGHL_PORT			0
#define VERSALOON_C2_PORT				0
#define VERSALOON_I2C_PORT				0
#define VERSALOON_MSP430_JTAG_PORT		0



// pending struct
#define VERSALOON_MAX_PENDING_NUMBER	4096//1024
typedef struct
{
	uint8 type;
	uint8 cmd;
	uint16 want_data_pos;
	uint16 want_data_size;
	uint16 actual_data_size;
	uint8 *data_buffer;
	uint8 collect;
}versaloon_pending_t;
extern versaloon_pending_t versaloon_pending[VERSALOON_MAX_PENDING_NUMBER];
extern uint16 versaloon_pending_idx;
RESULT versaloon_add_pending(uint8 type, uint8 cmd, uint16 actual_szie, 
							 uint16 want_pos, uint16 want_size, uint8 *buffer, 
							 uint8 collect);




RESULT versaloon_get_target_voltage(uint16 *voltage);
RESULT versaloon_get_hardware(uint8 *hardware);
const char* versaloon_get_hardware_name(uint8 idx);
RESULT versaloon_send_command(uint16 out_len, uint16 *inlen);
RESULT versaloon_init(void);
RESULT versaloon_fini(void);

RESULT versaloon_download_mass_product_data(const char *name, uint8 *buffer, 
											uint32 len);
RESULT versaloon_query_mass_product_data_size(uint32 *size);

extern uint8 *versaloon_buf;
extern uint16 versaloon_buf_size;

#endif /* __VERSALOON_INTERNAL_H_INCLUDED__ */

