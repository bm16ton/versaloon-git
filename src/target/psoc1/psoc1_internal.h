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
#ifndef __PSOC1_INTERNAL_H_INCLUDED__
#define __PSOC1_INTERNAL_H_INCLUDED__

#define PSOC1_MAX_FLASH_SIZE			(32 * 1024)
#define PSOC1_MAX_SECURE_SIZE			(4 * 32)
#define PSOC1_MIN_SECURE_SIZE			64
#define PSOC1_MAX_CHECKSUM_SIZE			(4 * 2)
#define PSOC1_MAX_BANK_NUM				4
#define PSOC1_MIN_BLOCK_NUM_IN_BANK		32
#define PSOC1_MAX_BLOCK_SIZE			64

#define PSOC1_FLASH_CHAR				0x00
#define PSOC1_SECURE_CHAR				0x00
#define PSOC1_CHECKSUM_CHAR				0x00
#define PSOC1_ID_MASK					0xFFFF

#define PSOC1_PARAM_BANK_NUM			0

// parameters
#define PSOC1_RESET_MODE				0
#define PSOC1_POWERON_MODE				1

#endif /* __PSOC1_INTERNAL_H_INCLUDED__ */

