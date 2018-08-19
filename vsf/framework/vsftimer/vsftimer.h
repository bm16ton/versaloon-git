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

#ifndef __VSFTIMER_H_INCLUDED__
#define __VSFTIMER_H_INCLUDED__

#include "tool/list/list.h"
#include "framework/vsfsm/vsfsm.h"

struct vsftimer_timer_t
{
	uint32_t interval;
	struct vsfsm_t *sm;
	vsfsm_evt_t evt;
	
	// private
	struct sllist list;
	uint32_t start_tickcnt;
};

vsf_err_t vsftimer_init(void);
// call vsftimer_callback_int in hw timer interrupt
void vsftimer_callback_int(void);
vsf_err_t vsftimer_register(struct vsftimer_timer_t *timer);
vsf_err_t vsftimer_unregister(struct vsftimer_timer_t *timer);

#endif	// #ifndef __VSFTIMER_H_INCLUDED__
