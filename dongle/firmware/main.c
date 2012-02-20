/**************************************************************************
 *  Copyright (C) 2008 - 2010 by Simon Qian                               *
 *  SimonQian@SimonQian.com                                               *
 *                                                                        *
 *  Project:    Versaloon                                                 *
 *  File:       main.c                                                    *
 *  Author:     SimonQian                                                 *
 *  Versaion:   See changelog                                             *
 *  Purpose:    main.c file                                               *
 *  License:    See license                                               *
 *------------------------------------------------------------------------*
 *  Change Log:                                                           *
 *      YYYY-MM-DD:     What(by Who)                                      *
 *      2008-11-07:     created(by SimonQian)                             *
 **************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "app_cfg.h"
#include "interfaces.h"

#include "usb_protocol.h"

#if SCRIPTS_EN
#	include "app_io.h"
#	include "app_log.h"
#	include "scripts.h"
#	include "vsprog.h"

#	include "interfaces.h"
#endif

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
extern struct usart_stream_info_t usart_stream_p0;
int main(void)
{
#if HW_HAS_BEEPER
	uint16_t start_beeper_cnt = 0x8000;
#endif
#if SCRIPTS_EN
	FILE *script_file = NULL;
#endif
	
	core_interfaces.core.init(NULL);
	usb_protocol_init();
#if SCRIPTS_EN
	APP_IO_INIT();
	vss_init();
	vss_register_cmd_list(&appio_cmd_list);
	vss_register_cmd_list(&vsprog_cmd_list);
	
	KEY_Init();
	while (!KEY_IsDown())
	{
		usb_protocol_poll();
	}
	
	script_file = FOPEN(EVSPROG_SCRIPT_FILE, "rt");
	if (script_file != NULL)
	{
		FCLOSE(script_file);
		script_file = NULL;
		if (!usb_device.configured)
		{
			vss_run_script("dummy 1");
		}
		vss_run_script("run " EVSPROG_SCRIPT_FILE);
	}
	else
	{
		vss_run_script("shell");
	}
#endif	// if SCRIPTS_EN

#if HW_HAS_BEEPER
	BEEPER_INIT();
	BEEPER_ON();
#endif
	while (1)
	{
		usb_protocol_poll();
#if HW_HAS_BEEPER
		if (start_beeper_cnt)
		{
			start_beeper_cnt--;
			if (!start_beeper_cnt)
			{
				BEEPER_OFF();
			}
		}
#endif
	}
}

/******************* (C) COPYRIGHT 2007 STMicroelectronics *****END OF FILE****/
