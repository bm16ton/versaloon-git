/**************************************************************************
 *  Copyright (C) 2008 - 2010 by Simon Qian                               *
 *  SimonQian@SimonQian.com                                               *
 *                                                                        *
 *  Project:    Versaloon                                                 *
 *  File:       BDM.c                                                     *
 *  Author:     SimonQian                                                 *
 *  Versaion:   See changelog                                             *
 *  Purpose:    BDM interface implementation file                         *
 *  License:    See license                                               *
 *------------------------------------------------------------------------*
 *  Change Log:                                                           *
 *      YYYY-MM-DD:     What(by Who)                                      *
 *      2008-11-07:     created(by SimonQian)                             *
 **************************************************************************/

#include "app_cfg.h"
#if INTERFACE_BDM_EN

#include "BDM.h"

uint8 BDM_Inited = 0;
static uint16 BDM_DMA_OUT_Buffer[16];
static uint16 BDM_DMA_IN_Buffer[8 * 0xF + 1];
static uint16 BDM_clock_div = 0;

static uint16 BDM_PULSE_0;
static uint16 BDM_PULSE_1;
static uint16 BDM_PULSE_Threshold;

#define BDM_SIG0_CYCLES					13
#define BDM_SIG1_CYCLES					4
#define BDM_1BIT_CYCLES					18
#define BDM_THRES_CYCLES				9

#define BDM_SYNC_CYCLES					128
#define BDM_MAX_DLY						0xFFFFF

void BDM_Init(void)
{
	if (!BDM_Inited)
	{
		BDM_Inited = 1;
		BDM_clock_div = 0;
		SYNCSWPWM_IN_TIMER_INIT();
		SYNCSWPWM_OUT_TIMER_INIT();
		SYNCSWPWM_PORT_OD_INIT();
	}
}

void BDM_Fini(void)
{
	SYNCSWPWM_PORT_OD_FINI();
	SYNCSWPWM_OUT_TIMER_FINI();
	SYNCSWPWM_IN_TIMER_FINI();
	BDM_Inited = 0;
}

uint8 BDM_Sync(uint16 *khz)
{
	uint32 dly;

	// reset MUST be released to perform sync
	dly = 0;
	while (!SW_GET())
	{
		DelayUS(1);
		if (++dly > 0xFFFF)
		{
			return 1;
		}
	}
	// more 10ms for stablity
	DelayMS(10);

	SYNCSWPWM_IN_TIMER_DMA_INIT(2, BDM_DMA_IN_Buffer);

	SYNCSWPWM_OUT_TIMER_SetCycle(0xFFFF);
	BDM_DMA_OUT_Buffer[0] = 0xFF00;
	BDM_DMA_OUT_Buffer[1] = 0;
	SYNCSWPWM_OUT_TIMER_DMA_INIT(2, BDM_DMA_OUT_Buffer);
	SYNCSWPWM_OUT_TIMER_DMA_WAIT();

	dly = BDM_MAX_DLY;
	SYNCSWPWM_IN_TIMER_DMA_WAIT(dly);

	if (!dly)
	{
		return 1;
	}
	else
	{
		BDM_clock_div = BDM_DMA_IN_Buffer[1];
		*khz = _SYS_FREQUENCY * 1000 * 128 / BDM_clock_div;
		BDM_PULSE_1 = BDM_clock_div * BDM_SIG1_CYCLES / BDM_SYNC_CYCLES;
		if ((BDM_clock_div * BDM_SIG1_CYCLES % BDM_SYNC_CYCLES) >= (BDM_SYNC_CYCLES / 2))
		{
			BDM_PULSE_1++;
		}
		BDM_PULSE_0 = BDM_clock_div * BDM_SIG0_CYCLES / BDM_SYNC_CYCLES;
		if ((BDM_clock_div * BDM_SIG0_CYCLES % BDM_SYNC_CYCLES) >= (BDM_SYNC_CYCLES / 2))
		{
			BDM_PULSE_0++;
		}
		BDM_PULSE_Threshold = BDM_clock_div * BDM_THRES_CYCLES / BDM_SYNC_CYCLES;
		SYNCSWPWM_OUT_TIMER_SetCycle(BDM_clock_div * BDM_1BIT_CYCLES / BDM_SYNC_CYCLES);
		return 0;
	}
}

uint8 BDM_OutByte(uint8 data)
{
	int8 i;
	uint16 *ptr = &BDM_DMA_OUT_Buffer[0];

	for (i = 7; i >= 0; i--)
	{
		if (data & (1 << i))
		{
			*ptr++ = BDM_PULSE_1;
		}
		else
		{
			*ptr++ = BDM_PULSE_0;
		}
	}
	*ptr++ = 0;
	SYNCSWPWM_OUT_TIMER_DMA_INIT(9, BDM_DMA_OUT_Buffer);
	SYNCSWPWM_OUT_TIMER_DMA_WAIT();

	return 0;
}

uint8 BDM_OutDly(void)
{
	uint8 i;
	uint16 *ptr = &BDM_DMA_OUT_Buffer[0];

	for (i = 0; i < 16; i++)
	{
		*ptr++ = 0;
	}
	SYNCSWPWM_OUT_TIMER_DMA_INIT(16, BDM_DMA_OUT_Buffer);
	SYNCSWPWM_OUT_TIMER_DMA_WAIT();

	return 0;
}

uint8 BDM_InByte(uint8 *data)
{
	uint32 dly;
	int8 i;
	uint16 *ptr = &BDM_DMA_OUT_Buffer[0];

	SYNCSWPWM_IN_TIMER_DMA_INIT(8, BDM_DMA_IN_Buffer);

	for (i = 0; i < 8; i++)
	{
		*ptr++ = BDM_PULSE_1;
	}
	*ptr++ = 0;
	SYNCSWPWM_OUT_TIMER_DMA_INIT(9, BDM_DMA_OUT_Buffer);
	SYNCSWPWM_OUT_TIMER_DMA_WAIT();

	dly = BDM_MAX_DLY;
	SYNCSWPWM_IN_TIMER_DMA_WAIT(dly);
	if (!dly)
	{
		return 1;
	}

	for (i = 0; i < 8; i++)
	{
		*data <<= 1;
		if (BDM_DMA_IN_Buffer[i] >= BDM_PULSE_Threshold)
		{
			*data &= ~1;
		}
		else
		{
			*data |= 1;
		}
	}

	return 0;
}

uint8 BDM_Transact(uint16 token, uint8 *out, uint8 *in)
{
	uint32 dly;
	uint16 outlen, inlen, i;

	outlen = BDM_OUT_LEN(token);
	if (!outlen)
	{
		return 1;
	}
	outlen *= 8;
	if (BDM_ACK(token))
	{
		outlen++;
	}
	SYNCSWPWM_IN_TIMER_DMA_INIT(outlen, BDM_DMA_IN_Buffer);

	// out data
	outlen = BDM_OUT_LEN(token);
	for (i = 0; i < outlen; i++)
	{
		if (BDM_OutByte(out[i]))
		{
			return 1;
		}
	}

	// out delay
	outlen = BDM_OUT_DLY_CNT(token);
	while (outlen--)
	{
		if (BDM_OutDly())
		{
			return 1;
		}
	}

	dly = BDM_MAX_DLY;
	SYNCSWPWM_IN_TIMER_DMA_WAIT(dly);
	if (!dly)
	{
		return 1;
	}

	// in data
	inlen = BDM_IN_LEN(token);
	if (inlen)
	{
		for (i = 0; i < inlen; i++)
		{
			if (BDM_InByte(&in[i]))
			{
				return 1;
			}
		}
	}

	return 0;
}

#endif
