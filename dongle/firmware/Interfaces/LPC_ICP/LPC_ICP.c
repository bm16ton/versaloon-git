/**************************************************************************
 *  Copyright (C) 2008 - 2010 by Simon Qian                               *
 *  SimonQian@SimonQian.com                                               *
 *                                                                        *
 *  Project:    Versaloon                                                 *
 *  File:       LPC_ICP.c                                                 *
 *  Author:     SimonQian                                                 *
 *  Versaion:   See changelog                                             *
 *  Purpose:    LPC_ICP interface implementation file                     *
 *  License:    See license                                               *
 *------------------------------------------------------------------------*
 *  Change Log:                                                           *
 *      YYYY-MM-DD:     What(by Who)                                      *
 *      2008-11-07:     created(by SimonQian)                             *
 **************************************************************************/

#include "app_cfg.h"
#if INTERFACE_LPC_ICP_EN

#include "interfaces.h"
#include "LPC_ICP.h"
#if POWER_OUT_EN
#	include "PowerExt.h"
#endif

#define LPCICP_POWERON_DELAY			10
#define LPCICP_RST_TOGGLE_DELAY			1
#define LPCICP_POST_ENTERPROGMODE_DELAY	100
#define LPCICP_SHIFT_DELAY_SHORT		0
#define LPCICP_SHIFT_DELAY_LONG			0

static void LPCICP_Init(void)
{
	LPCICP_RST_CLR();
	LPCICP_RST_SETOUTPUT();

	LPCICP_PCL_SET();
	LPCICP_PCL_SETOUTPUT();

	LPCICP_PDA_SET();
	LPCICP_PDA_SETOUTPUT();

	GLOBAL_OUTPUT_Acquire();
}

static void LPCICP_Fini(void)
{
	GLOBAL_OUTPUT_Release();

	LPCICP_RST_SETINPUT();
	LPCICP_PDA_SETINPUT();
	LPCICP_PCL_SETINPUT();
}

static void LPCICP_LeavrProgMode(void)
{
	PWREXT_Release();
}

static void LPCICP_EnterProgMode(void)
{
	uint8 toggle_count;

	PWREXT_Acquire();
	DelayMS(LPCICP_POWERON_DELAY);

	for (toggle_count = 0; toggle_count < 7; toggle_count++)
	{
		LPCICP_RST_SET();
		DelayUS(LPCICP_RST_TOGGLE_DELAY);		// Trh
		LPCICP_RST_CLR();
		DelayUS(LPCICP_RST_TOGGLE_DELAY);		// Trl
	}
	LPCICP_RST_SET();

	DelayMS(LPCICP_POST_ENTERPROGMODE_DELAY);	// Trp
	LPCICP_PDA_SETINPUT();
}

static void LPCICP_In(uint8 *buff, uint16 len)
{
	uint32 i;

	for (i = 0; i < len * 8; i++)
	{
		LPCICP_PCL_CLR();
		DelayUS(LPCICP_SHIFT_DELAY_SHORT);
		LPCICP_PCL_SET();

		if (LPCICP_PDA_GET())
		{
			buff[i / 8] |= 1 << (i % 8);
		}
		else
		{
			buff[i / 8] &= ~(1 << (i % 8));
		}
		DelayUS(LPCICP_SHIFT_DELAY_LONG);
	}
}

static void LPCICP_Out(uint8 *buff, uint16 len)
{
	uint32 i;

	LPCICP_PDA_SETOUTPUT();

	for (i = 0; i < len * 8; i++)
	{
		LPCICP_PCL_CLR();

		if (buff[i / 8] & (1 << (i % 8)))
		{
			LPCICP_PDA_SET();
		}
		else
		{
			LPCICP_PDA_CLR();
		}

		DelayUS(LPCICP_SHIFT_DELAY_LONG);
		LPCICP_PCL_SET();
		DelayUS(LPCICP_SHIFT_DELAY_SHORT);
	}

	LPCICP_PDA_SETINPUT();
}

static uint8 LPCICP_Poll(uint8 out, uint8 setbit, uint8 clearbit, uint16 pollcnt)
{
	uint8 tmp;

	while (pollcnt-- > 0)
	{
		LPCICP_Out(&out, 1);
		LPCICP_In(&tmp, 1);

		if (setbit && ((tmp & setbit) == setbit))
		{
			return LPCICP_POLL_ON_SET;
		}
		if (clearbit && ((tmp & clearbit) == 0))
		{
			return LPCICP_POLL_ON_CLEAR;
		}
	}

	return LPCICP_POLL_TIME_OUT;
}

RESULT lpcicp_init(uint8_t index)
{
	switch (index)
	{
	case 0:
		LPCICP_Init();
		return ERROR_OK;
	default:
		return ERROR_FAIL;
	}
}

RESULT lpcicp_fini(uint8_t index)
{
	switch (index)
	{
	case 0:
		LPCICP_LeavrProgMode();
		LPCICP_Fini();
		return ERROR_OK;
	default:
		return ERROR_FAIL;
	}
}

RESULT lpcicp_enter_program_mode(uint8_t index)
{
	uint16_t voltage;
	
	switch (index)
	{
	case 0:
		if ((ERROR_OK != interfaces->target_voltage.get(0, &voltage)) || 
			(voltage > TVCC_SAMPLE_MIN_POWER))
		{
			// No power should be applied on the target
			return ERROR_FAIL;
		}
		else
		{
			LPCICP_EnterProgMode();
			return ERROR_OK;
		}
	default:
		return ERROR_FAIL;
	}
}

RESULT lpcicp_in(uint8_t index, uint8_t *buff, uint16_t len)
{
	switch (index)
	{
	case 0:
		LPCICP_In(buff, len);
		return ERROR_OK;
	default:
		return ERROR_FAIL;
	}
}

RESULT lpcicp_out(uint8_t index, uint8_t *buff, uint16_t len)
{
	switch (index)
	{
	case 0:
		LPCICP_Out(buff, len);
		return ERROR_OK;
	default:
		return ERROR_FAIL;
	}
}

RESULT lpcicp_poll_ready(uint8_t index, uint8_t data, uint8_t *ret, 
							uint8_t setmask, uint8_t clearmask, uint16_t pollcnt)
{
	uint8_t ret_tmp;
	
	switch (index)
	{
	case 0:
		ret_tmp = LPCICP_Poll(data, setmask, clearmask, pollcnt);
		if (ret != NULL)
		{
			*ret = ret_tmp;
		}
		return ERROR_OK;
	default:
		return ERROR_FAIL;
	}
}

#endif	// INTERFACE_LPC_ICP_EN
