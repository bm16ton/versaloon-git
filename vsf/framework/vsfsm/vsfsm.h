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

#ifndef __VSFSM_H_INCLUDED__
#define __VSFSM_H_INCLUDED__

#include "app_type.h"
#include "vsfsm_cfg.h"

enum
{
	VSFSM_EVT_INVALID = -1,
	VSFSM_EVT_NONE = 0,
	VSFSM_EVT_SYSTEM = 1,
	VSFSM_EVT_DUMMY = VSFSM_EVT_SYSTEM + 0,
	VSFSM_EVT_INIT = VSFSM_EVT_SYSTEM + 1,
	VSFSM_EVT_FINI = VSFSM_EVT_SYSTEM + 2,
	VSFSM_EVT_USER = 0x10,
	// instant message CANNOT be but in the event queue and
	// can not be sent in interrupt
	VSFSM_EVT_INSTANT = 0x2000,
	VSFSM_EVT_USER_INSTANT = VSFSM_EVT_INSTANT,
	VSFSM_EVT_INSTANT_END = VSFSM_EVT_INSTANT + 0x2000 - 1,
	// local event can not transmit or be passed to superstate
	VSFSM_EVT_LOCAL = VSFSM_EVT_INSTANT_END + 1,
	VSFSM_EVT_USER_LOCAL = VSFSM_EVT_LOCAL,
	// local instant message CANNOT be but in the event queue and
	// can not be sent in interrupt
	VSFSM_EVT_LOCAL_INSTANT = VSFSM_EVT_LOCAL + 0x2000,
	// VSFSM_EVT_ENTER and VSFSM_EVT_EXIT are local instant events
	VSFSM_EVT_ENTER = VSFSM_EVT_LOCAL_INSTANT + 0,
	VSFSM_EVT_EXIT = VSFSM_EVT_LOCAL_INSTANT + 1,
	VSFSM_EVT_USER_LOCAL_INSTANT = VSFSM_EVT_LOCAL_INSTANT + 2,
	VSFSM_EVT_LOCAL_INSTANT_END = VSFSM_EVT_LOCAL_INSTANT + 0x2000 - 1,
};

typedef int vsfsm_evt_t;

struct vsfsm_evtqueue_t
{
	vsfsm_evt_t *evt_buffer;
	uint16_t evt_buffer_num;
	
	// private
	uint16_t head;
	uint16_t tail;
	uint16_t count;
};

struct vsfsm_t;
struct vsfsm_state_t
{
	// return NULL means the event is handled, and no transition
	// return a vsfsm_state_t pointer means transition to the state
	// return -1 means the event is not handled, should redirect to superstate
	struct vsfsm_state_t * (*evt_handler)(struct vsfsm_t *sm, vsfsm_evt_t evt);
	
	// sub state machine list
	// for subsm, user need to call vsfsm_init on VSFSM_EVT_ENTER
	// if the subsm is historical, vsfsm_init should only be called once on
	// 		first VSFSM_EVT_ENTER
	// for initialized historical subsm, vsfsm_set_active should be called to
	// 		set the subsm active(means ready to accept events) on
	// 		VSFSM_EVT_ENTER, and set the subsm inactive on VSFSM_EVT_EXIT
	struct vsfsm_t *subsm;
	
#if VSFSM_CFG_SM_EN && VSFSM_CFG_HSM_EN
	// for top state, super is NULL; other super points to the superstate
	struct vsfsm_state_t *super;
#endif
};

struct vsfsm_t
{
	struct vsfsm_evtqueue_t evtq;
	// initial state
	// for protothread, evt_handler MUST point to vsfsm_pt_evt_handler
	// 		which will be initialized in vsfsm_pt_init
	struct vsfsm_state_t init_state;
	// user_data point to the user specified data for the sm
	// for protothread, user_data should point to vsfsm_pt_t structure
	// 		which will be initialized in vsfsm_pt_init
	void *user_data;
#if VSFSM_CFG_SM_EN
	// sm_extra is used for specific sm type
	// for MSM, sm_extra point to the transition table
	void *sm_extra;
#endif
	
	// private
	struct vsfsm_state_t *cur_state;
#if VSFSM_CFG_SYNC_EN
	// pending_next is used for vsfsm_sem_t
	struct vsfsm_t *pending_next;
#endif
	volatile bool active;
	struct vsfsm_t *next;
};

#if VSFSM_CFG_PT_EN
struct vsfsm_pt_t
{
	vsf_err_t (*thread)(struct vsfsm_pt_t *pt, vsfsm_evt_t evt);
	void *user_data;
	
#if VSFSM_CFG_PT_STACK_EN
	// stack for the current pt, can be NULL to indicate using the main stack
	// note that the size of the stack MUST be large enough for the interrupts
	void *stack;
#endif
	
	// protected
	int state;
	struct vsfsm_t *sm;
};

vsf_err_t vsfsm_pt_init(struct vsfsm_t *sm, struct vsfsm_pt_t *pt,
						bool add_to_top);
#define vsfsm_pt_begin(pt)				switch ((pt)->state) { case 0:
#define vsfsm_pt_entry(pt)				(pt)->state = __LINE__; case __LINE__:
// wait for event
#define vsfsm_pt_wfe(pt, e)				do {\
											vsfsm_pt_entry(pt);\
											if (evt != (e)) return VSFERR_NOT_READY;\
										} while (0)
// wait for pt, slave pt uses the same stack as the master pt
#define vsfsm_pt_wfpt(pt, ptslave)		do {\
											(ptslave)->state = 0;\
											(ptslave)->sm = (pt)->sm;\
											vsfsm_pt_entry(pt);\
											{\
												vsf_err_t __err = (ptslave)->thread(ptslave);\
												if (__err != VSFERR_NONE)\
												{\
													return __err;\
												}\
											}\
										} while (0)
#define vsfsm_pt_end(pt)				}
#endif

extern struct vsfsm_state_t vsfsm_top;
// vsfsm_get_event_pending should be called with interrupt disabled
uint32_t vsfsm_get_event_pending(void);

// sub-statemachine add/remove
vsf_err_t vsfsm_add_subsm(struct vsfsm_state_t *state, struct vsfsm_t *sm);
vsf_err_t vsfsm_remove_subsm(struct vsfsm_state_t *state, struct vsfsm_t *sm);

// vsfsm_init will set the sm to be active(means ready to accept events)
vsf_err_t vsfsm_init(struct vsfsm_t *sm, bool add_to_top);
vsf_err_t vsfsm_poll(void);
// sm is avtive after init, if sm will not accept further events
// user MUST set the sm to be inactive
vsf_err_t vsfsm_set_active(struct vsfsm_t *sm, bool active);
vsf_err_t vsfsm_post_evt(struct vsfsm_t *sm, vsfsm_evt_t evt);
vsf_err_t vsfsm_post_evt_pending(struct vsfsm_t *sm, vsfsm_evt_t evt);

#if VSFSM_CFG_SYNC_EN
// vsfsm_sem_t is used as access lock for resources
struct vsfsm_sync_t
{
	uint32_t cur_value;
	vsfsm_evt_t evt;
	bool valid_on_increase;
	
	// private
	uint32_t max_value;
	struct vsfsm_t *sm_pending;
};
vsf_err_t vsfsm_sync_init(struct vsfsm_sync_t *sem, uint32_t cur_value,
				uint32_t max_value, vsfsm_evt_t evt, bool valid_on_increase);
vsf_err_t vsfsm_sync_cancel(struct vsfsm_t *sm, struct vsfsm_sync_t *sync);
vsf_err_t vsfsm_sync_increase(struct vsfsm_t *sm, struct vsfsm_sync_t *sync);
vsf_err_t vsfsm_sync_decrease(struct vsfsm_t *sm, struct vsfsm_sync_t *sync);

// SEMAPHORE
#define vsfsm_sem_t					vsfsm_sync_t
#define vsfsm_sem_init(sem, evt)	vsfsm_sync_init((sem), 0, 1, (evt), true)
#define vsfsm_sem_send(sm, sem)		vsfsm_sync_increase((sm), (sem))
#define vsfsm_sem_wait(sm, sem)		vsfsm_sync_decrease((sm), (sem))

// CRITICAL
#define vsfsm_crit_t				vsfsm_sync_t
#define vsfsm_crit_init(crit, evt)	vsfsm_sync_init((cirt), 1, 1, (evt), false)
#define vsfsm_cirt_enter(sm, crit)	vsfsm_sync_decrease((sm), (crit))
#define vsfsm_cirt_leave(sm, crit)	vsfsm_sync_increase((sm), (crit))

#endif	// VSFSM_CFG_SYNC_EN

#endif	// #ifndef __VSFSM_H_INCLUDED__
