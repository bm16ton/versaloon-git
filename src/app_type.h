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

#ifndef __APP_TYPE_H_INCLUDED__
#define __APP_TYPE_H_INCLUDED__

#include <stdint.h>
#include <stdbool.h>

#ifndef __CONNECT
#	define __CONNECT(a, b)			a ## b
#endif

#ifndef NULL
#	define NULL						((void *)0)
#endif

typedef enum result_s
{
	ERROR_OK = 0,
	ERROR_FAIL = 1
} RESULT;

#define dimof(arr)					(sizeof(arr) / sizeof((arr)[0]))
#ifndef min
#	define min(a,b)					(((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#	define max(a,b)					(((a) > (b)) ? (a) : (b))
#endif
#define TO_STR(name)				#name
#define REFERENCE_PARAMETER(a)		(a) = (a)



#define GET_U16_MSBFIRST(p)			(	((*((uint8_t *)(p) + 0)) << 8) | \
										((*((uint8_t *)(p) + 1)) << 0))
#define GET_U32_MSBFIRST(p)			(	((*((uint8_t *)(p) + 0)) << 24) | \
										((*((uint8_t *)(p) + 1)) << 16) | \
										((*((uint8_t *)(p) + 2)) << 8) | \
										((*((uint8_t *)(p) + 3)) << 0))
#define GET_U16_LSBFIRST(p)			(	((*((uint8_t *)(p) + 0)) << 0) | \
										((*((uint8_t *)(p) + 1)) << 8))
#define GET_U32_LSBFIRST(p)			(	((*((uint8_t *)(p) + 0)) << 0) | \
										((*((uint8_t *)(p) + 1)) << 8) | \
										((*((uint8_t *)(p) + 2)) << 16) | \
										((*((uint8_t *)(p) + 3)) << 24))

#define SET_U16_MSBFIRST(p, v)		\
	do{\
		*((uint8_t *)(p) + 0) = (((uint16_t)(v)) >> 8) & 0xFF;\
		*((uint8_t *)(p) + 1) = (((uint16_t)(v)) >> 0) & 0xFF;\
	} while (0)
#define SET_U32_MSBFIRST(p, v)		\
	do{\
		*((uint8_t *)(p) + 0) = (((uint32_t)(v)) >> 24) & 0xFF;\
		*((uint8_t *)(p) + 1) = (((uint32_t)(v)) >> 16) & 0xFF;\
		*((uint8_t *)(p) + 2) = (((uint32_t)(v)) >> 8) & 0xFF;\
		*((uint8_t *)(p) + 3) = (((uint32_t)(v)) >> 0) & 0xFF;\
	} while (0)
#define SET_U16_LSBFIRST(p, v)		\
	do{\
		*((uint8_t *)(p) + 0) = (((uint16_t)(v)) >> 0) & 0xFF;\
		*((uint8_t *)(p) + 1) = (((uint16_t)(v)) >> 8) & 0xFF;\
	} while (0)
#define SET_U32_LSBFIRST(p, v)		\
	do{\
		*((uint8_t *)(p) + 0) = (((uint32_t)(v)) >> 0) & 0xFF;\
		*((uint8_t *)(p) + 1) = (((uint32_t)(v)) >> 8) & 0xFF;\
		*((uint8_t *)(p) + 2) = (((uint32_t)(v)) >> 16) & 0xFF;\
		*((uint8_t *)(p) + 3) = (((uint32_t)(v)) >> 24) & 0xFF;\
	} while (0)

#define GET_LE_U16(p)				GET_U16_LSBFIRST(p)
#define GET_LE_U32(p)				GET_U32_LSBFIRST(p)
#define GET_BE_U16(p)				GET_U16_MSBFIRST(p)
#define GET_BE_U32(p)				GET_U32_MSBFIRST(p)
#define SET_LE_U16(p, v)			SET_U16_LSBFIRST(p, v)
#define SET_LE_U32(p, v)			SET_U32_LSBFIRST(p, v)
#define SET_BE_U16(p, v)			SET_U16_MSBFIRST(p, v)
#define SET_BE_U32(p, v)			SET_U32_MSBFIRST(p, v)

#define SWAP_U16(v)					((((uint16_t)(v) & 0xFF00) >> 8) | \
										(((uint16_t)(v) & 0x00FF) << 8))
#define SWAP_U32(v)					((((uint32_t)(v) & 0xFF000000) >> 24) | \
										(((uint32_t)(v) & 0x00FF0000) >> 8) | \
										(((uint32_t)(v) & 0x0000FF00) << 8) | \
										(((uint32_t)(v) & 0x000000FF) << 24))

#if defined(__BIG_ENDIAN__) && (__BIG_ENDIAN__ == 1)
#	define LE_TO_SYS_U16(v)			SWAP_U16(v)
#	define LE_TO_SYS_U32(v)			SWAP_U32(v)
#	define BE_TO_SYS_U16(v)			((uint16_t)(v))
#	define BE_TO_SYS_U32(v)			((uint32_t)(v))

#	define SYS_TO_LE_U16(v)			SWAP_U16(v)
#	define SYS_TO_LE_U32(v)			SWAP_U32(v)
#	define SYS_TO_BE_U16(v)			((uint16_t)(v))
#	define SYS_TO_BE_U32(v)			((uint32_t)(v))

#	define GET_SYS_U16(p)			GET_BE_U16(p)
#	define GET_SYS_U32(p)			GET_BE_U32(p)
#else
#	define LE_TO_SYS_U16(v)			((uint16_t)(v))
#	define LE_TO_SYS_U32(v)			((uint32_t)(v))
#	define BE_TO_SYS_U16(v)			SWAP_U16(v)
#	define BE_TO_SYS_U32(v)			SWAP_U32(v)

#	define SYS_TO_LE_U16(v)			((uint16_t)(v))
#	define SYS_TO_LE_U32(v)			((uint32_t)(v))
#	define SYS_TO_BE_U16(v)			SWAP_U16(v)
#	define SYS_TO_BE_U32(v)			SWAP_U32(v)

#	define GET_SYS_U16(p)			GET_LE_U16(p)
#	define GET_SYS_U32(p)			GET_LE_U32(p)
#endif

#endif /* __APP_TYPE_H_INCLUDED__ */

