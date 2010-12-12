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


#ifndef __BYTE_TAP_H_INCLUDED__
#define __BYTE_TAP_H_INCLUDED__

extern struct interfaces_info_t *interfaces;

#define jtag_init()					interfaces->jtag_ll.init(0)
#define jtag_fini()					interfaces->jtag_ll.fini(0)
#define jtag_config(f)				interfaces->jtag_ll.config(0, f)
#define jtag_tms(m, len)			interfaces->jtag_ll.tms(0, (m), (len))
#define jtag_tms_clocks(len, m)		\
	interfaces->jtag_ll.tms_clocks(0, (len), (m))
#define jtag_xr(d, l, v, b, a0, a1)	\
	interfaces->jtag_ll.scan(0, (d), (l), (v), (v), (a0), (a1))
#define jtag_commit()				interfaces->peripheral_commit()

#define jtag_trst_init()			interfaces->gpio.init(0)
#define jtag_trst_fini()			interfaces->gpio.fini(0)
#define jtag_trst_output(value)		\
	interfaces->gpio.config(0, JTAG_TRST, JTAG_TRST, (value) ? JTAG_TRST : 0)
#define jtag_trst_input()			\
	interfaces->gpio.config(0, JTAG_TRST, 0, JTAG_TRST)
#define jtag_trst_1()			interfaces->gpio.out(0, JTAG_TRST, JTAG_TRST)
#define jtag_trst_0()			interfaces->gpio.out(0, JTAG_TRST, 0)


#define TAP_NUM_OF_STATE		16

// TAP state
enum tap_state_t
{
	RESET,
	IDLE,
	DRSHIFT,
	DRPAUSE,
	IRSHIFT,
	IRPAUSE,
	DRSELECT,
	DRCAPTURE,
	DREXIT1,
	DREXIT2,
	DRUPDATE,
	IRSELECT,
	IRCAPTURE,
	IREXIT1,
	IREXIT2,
	IRUPDATE,
};

extern const char *tap_state_name[TAP_NUM_OF_STATE];

RESULT tap_init(void);
RESULT tap_fini(void);
uint8_t tap_state_is_stable(enum tap_state_t state);
uint8_t tap_state_is_valid(enum tap_state_t state);
RESULT tap_state_move(void);
RESULT tap_end_state(enum tap_state_t state);
RESULT tap_path_move(uint32_t num_states, enum tap_state_t *path);
RESULT tap_runtest(enum tap_state_t run_state, enum tap_state_t end_state, 
				   uint32_t num_cycles);
RESULT tap_scan_ir(uint8_t *buffer, uint32_t bit_size);
RESULT tap_scan_dr(uint8_t *buffer, uint32_t bit_size);
RESULT tap_commit(void);

#endif /* __BYTE_TAP_H_INCLUDED__ */

