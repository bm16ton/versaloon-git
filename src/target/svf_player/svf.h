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
#ifndef __SVF_H_INCLUDED__
#define __SVF_H_INCLUDED__

// SVF command
typedef enum
{
	ENDDR,
	ENDIR,
	FREQUENCY,
	HDR,
	HIR,
	PIO,
	PIOMAP,
	RUNTEST,
	SDR,
	SIR,
	STATE,
	TDR,
	TIR,
	TRST,
}svf_command_t;

extern const char *svf_command_name[14];

#define XXR_TDI				(1 << 0)
#define XXR_TDO				(1 << 1)
#define XXR_MASK			(1 << 2)
#define XXR_SMASK			(1 << 3)
typedef struct
{
	uint32 len;
	uint32 data_mask;
	uint8 *tdi;
	uint8 *tdo;
	uint8 *mask;
	uint8 *smask;
}svf_xxr_para_t;

typedef enum
{
	TRST_ON,
	TRST_OFF,
	TRST_Z,
	TRST_ABSENT
}trst_mode_t;

extern const char *svf_trst_mode_name[4];




typedef struct
{
	svf_xxr_para_t hir_para;
	svf_xxr_para_t hdr_para;
	svf_xxr_para_t tir_para;
	svf_xxr_para_t tdr_para;

	svf_xxr_para_t sir_para;
	svf_xxr_para_t sdr_para;

	tap_state_t ir_end_state;
	tap_state_t dr_end_state;

	tap_state_t runtest_run_state;
	tap_state_t runtest_end_state;

	trst_mode_t trst_mode;

	float frequency;
}svf_para_t;

#endif /* __SVF_H_INCLUDED__ */

