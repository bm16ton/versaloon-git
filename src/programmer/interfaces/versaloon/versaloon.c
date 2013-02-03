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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <time.h>
#include "compiler.h"

#include "versaloon_include.h"
#include "versaloon.h"
#include "versaloon_internal.h"
#include "usbtoxxx/usbtoxxx.h"
#include "usbtoxxx/usbtoxxx_internal.h"

#define VERSALOON_STRING					"versaloon"

VSS_HANDLER(versaloon_support);
VSS_HANDLER(versaloon_help);
const struct vss_cmd_t versaloon_notifier[] =
{
	VSS_CMD(	"support",
				"print support information, format: support/S",
				versaloon_support,
				NULL),
	VSS_CMD(	"S",
				"print support information, format: support/S",
				versaloon_support,
				NULL),
	VSS_CMD(	"help",
				"print help information, format: help/h",
				versaloon_help,
				NULL),
	VSS_CMD(	"h",
				"print help information, format: help/h",
				versaloon_help,
				NULL),
	VSS_CMD_END
};

VSS_HANDLER(versaloon_help)
{
	VSS_CHECK_ARGC(1);
	PRINTF("\
Usage of %s:\n\
  -U,  --usb <PID_VID_EPIN_EPOUT>           set usb VID, PID, EPIN, EPOUT\n\n",
		   VERSALOON_STRING);
	return VSFERR_NONE;
}

VSS_HANDLER(versaloon_support)
{
	VSS_CHECK_ARGC(1);
	PRINTF("\
%s: see http://www.SimonQian.com/en/Versaloon\n", VERSALOON_STRING);
	return VSFERR_NONE;
}

static usb_dev_handle *versaloon_device_handle = NULL;
static uint32_t versaloon_to = VERSALOON_TIMEOUT;

static vsf_err_t versaloon_transact(uint16_t out_len, uint16_t *inlen)
{
	int ret;
	
#if PARAM_CHECK
	if (NULL == usbtoxxx_info->buff)
	{
		LOG_BUG(ERRMSG_INVALID_BUFFER, TO_STR(usbtoxxx_info->buff));
		return ERRCODE_INVALID_BUFFER;
	}
	if ((0 == out_len) || (out_len > usbtoxxx_info->buff_len))
	{
		LOG_BUG(ERRMSG_INVALID_PARAMETER, __FUNCTION__);
		return VSFERR_INVALID_PARAMETER;
	}
#endif
	
	ret = usb_bulk_write(versaloon_device_handle, usb_param_epout(),
						 (char *)usbtoxxx_info->buff, out_len, versaloon_to);
	if (ret != out_len)
	{
		LOG_ERROR(ERRMSG_FAILURE_OPERATION_ERRSTRING, "send usb data",
					usb_strerror());
		return ERRCODE_FAILURE_OPERATION;
	}
	
	if (inlen != NULL)
	{
		ret = usb_bulk_read(versaloon_device_handle, usb_param_epin(),
			(char *)usbtoxxx_info->buff, usbtoxxx_info->buff_len, versaloon_to);
		if (ret > 0)
		{
			*inlen = (uint16_t)ret;
			return VSFERR_NONE;
		}
		else
		{
			LOG_ERROR(ERRMSG_FAILURE_OPERATION_ERRSTRING, "receive usb data",
						usb_strerror());
			return VSFERR_FAIL;
		}
	}
	else
	{
		return VSFERR_NONE;
	}
}

// usbtoxxx transact structure
static struct usbtoxxx_info_t versaloon_usbtoxxx_info =
{
	NULL, NULL, 0,
	versaloon_transact
};






// Interfaces:
// Core
static vsf_err_t versaloon_fini(void)
{
	if (versaloon_device_handle != NULL)
	{
		usbtoxxx_fini();
		
		usb_release_interface(versaloon_device_handle, usb_param_interface());
		usb_close(versaloon_device_handle);
		versaloon_device_handle = NULL;
	}
	
	return VSFERR_NONE;
}

#define VERSALOON_RETRY_CNT				10
static vsf_err_t versaloon_init(void *p)
{
	struct interfaces_info_t *t = (struct interfaces_info_t *)p;
	uint16_t ret = 0;
	uint8_t retry;
	uint32_t timeout_tmp;
	vsf_err_t err = VSFERR_NONE;
	
	usbtoxxx_info = &versaloon_usbtoxxx_info;
	
	if (!usb_param_valid())
	{
		usb_set_param(VERSALOON_VID, VERSALOON_PID, VERSALOON_INP,
						VERSALOON_OUTP, VERSALOON_IFACE);
	}
	versaloon_device_handle = find_usb_device(usb_param_vid(),
		usb_param_pid(), usb_param_interface(), VERSALOON_SERIALSTRING_INDEX,
		usb_param_serial(), VERSALOON_PRODUCTSTRING_INDEX,
						VERSALOON_PRODUCTSTRING);
	if (NULL == versaloon_device_handle)
	{
		if (usb_param_serial() != NULL)
		{
			LOG_ERROR("Not found vid=0x%04x,pid = 0x%04x,serial = %s.",
						usb_param_vid(), usb_param_pid(), usb_param_serial());
		}
		else
		{
			LOG_ERROR("Not found vid=0x%04x,pid = 0x%04x.", usb_param_vid(),
						usb_param_pid());
		}
		return VSFERR_FAIL;
	}
	
	// malloc temporary buffer
	if (!usbtoxxx_info->buff_len)
	{
		usbtoxxx_info->buff_len = 256;
	}
	usbtoxxx_info->buff = (uint8_t *)malloc(usbtoxxx_info->buff_len);
	if (NULL == usbtoxxx_info->buff)
	{
		LOG_ERROR(ERRMSG_NOT_ENOUGH_MEMORY);
		return VSFERR_NOT_ENOUGH_RESOURCES;
	}
	
	sleep_ms(100);
	
	// connect to versaloon
	LOG_PUSH();
	LOG_MUTE();
	timeout_tmp = versaloon_to;
	// not output error message when connectting
	// 500ms delay when connect
	versaloon_to = 100;
	for (retry = 0; retry < VERSALOON_RETRY_CNT; retry++)
	{
		usbtoxxx_info->buff[0] = VERSALOON_GET_INFO;
		if (!versaloon_transact(1, &ret) && (ret >= 3))
		{
			break;
		}
	}
	LOG_POP();
	versaloon_to = timeout_tmp;
	if (VERSALOON_RETRY_CNT == retry)
	{
		LOG_ERROR(ERRMSG_FAILURE_OPERATION, "communicate with versaloon");
		err = ERRCODE_FAILURE_OPERATION;
		goto versaloon_init_fail;
	}
	
	usbtoxxx_info->buff[ret] = 0;
	usbtoxxx_info->buff_len = GET_LE_U16(&usbtoxxx_info->buff[0]);
	LOG_INFO("%s", usbtoxxx_info->buff + 2);
	
	// free temporary buffer
	free(usbtoxxx_info->buff);
	usbtoxxx_info->buff = NULL;
	
	if (usbtoxxx_init())
	{
		err = VSFERR_FAIL;
		goto versaloon_init_fail;
	}
	// fixes programmer abilities
	if (t != NULL)
	{
		if ((t->support_mask & IFS_POLL) &&
			!usbtoxxx_interface_supported(USB_TO_POLL))
		{
			t->support_mask &= ~IFS_POLL;
		}
		if ((t->support_mask & IFS_USART) &&
			!usbtoxxx_interface_supported(USB_TO_USART))
		{
			t->support_mask &= ~IFS_USART;
		}
		if ((t->support_mask & IFS_SPI) &&
			!usbtoxxx_interface_supported(USB_TO_SPI))
		{
			t->support_mask &= ~IFS_SPI;
		}
		if ((t->support_mask & IFS_EBI) &&
			!usbtoxxx_interface_supported(USB_TO_EBI))
		{
			t->support_mask &= ~IFS_EBI;
		}
		if ((t->support_mask & IFS_I2C) &&
			!usbtoxxx_interface_supported(USB_TO_I2C))
		{
			t->support_mask &= ~IFS_I2C;
		}
		if ((t->support_mask & IFS_GPIO) &&
			!usbtoxxx_interface_supported(USB_TO_GPIO))
		{
			t->support_mask &= ~IFS_GPIO;
		}
		if ((t->support_mask & IFS_CAN) &&
			!usbtoxxx_interface_supported(USB_TO_CAN))
		{
			t->support_mask &= ~IFS_CAN;
		}
		if ((t->support_mask & IFS_ADC) &&
			!usbtoxxx_interface_supported(USB_TO_ADC))
		{
			t->support_mask &= ~IFS_ADC;
		}
		if ((t->support_mask & IFS_DAC) &&
			!usbtoxxx_interface_supported(USB_TO_DAC))
		{
			t->support_mask &= ~IFS_DAC;
		}
		if ((t->support_mask & IFS_POWER) &&
			!usbtoxxx_interface_supported(USB_TO_POWER))
		{
			t->support_mask &= ~IFS_POWER;
		}
		if ((t->support_mask & IFS_ISSP) &&
			!usbtoxxx_interface_supported(USB_TO_ISSP))
		{
			t->support_mask &= ~IFS_ISSP;
		}
		if ((t->support_mask & IFS_JTAG_HL) &&
			!usbtoxxx_interface_supported(USB_TO_JTAG_HL))
		{
			t->support_mask &= ~IFS_JTAG_HL;
		}
		if ((t->support_mask & IFS_JTAG_LL) &&
			!usbtoxxx_interface_supported(USB_TO_JTAG_LL))
		{
			t->support_mask &= ~IFS_JTAG_LL;
		}
		if ((t->support_mask & IFS_MSP430_JTAG) &&
			!usbtoxxx_interface_supported(USB_TO_MSP430_JTAG))
		{
			t->support_mask &= ~IFS_MSP430_JTAG;
		}
		if ((t->support_mask & IFS_C2) &&
			!usbtoxxx_interface_supported(USB_TO_C2))
		{
			t->support_mask &= ~IFS_C2;
		}
		if ((t->support_mask & IFS_USART) &&
			!usbtoxxx_interface_supported(USB_TO_USART))
		{
			t->support_mask &= ~IFS_USART;
		}
		if ((t->support_mask & IFS_LPC_ICP) &&
			!usbtoxxx_interface_supported(USB_TO_LPCICP))
		{
			t->support_mask &= ~IFS_LPC_ICP;
		}
		if ((t->support_mask & IFS_SWD) &&
			!usbtoxxx_interface_supported(USB_TO_SWD))
		{
			t->support_mask &= ~IFS_SWD;
		}
		if ((t->support_mask & IFS_SWIM) &&
			!usbtoxxx_interface_supported(USB_TO_SWIM))
		{
			t->support_mask &= ~IFS_SWIM;
		}
		if ((t->support_mask & IFS_JTAG_RAW) &&
			!usbtoxxx_interface_supported(USB_TO_JTAG_RAW))
		{
			t->support_mask &= ~IFS_JTAG_RAW;
		}
		if ((t->support_mask & IFS_BDM) &&
			!usbtoxxx_interface_supported(USB_TO_BDM))
		{
			t->support_mask &= ~IFS_BDM;
		}
		if ((t->support_mask & IFS_DUSI) &&
			!usbtoxxx_interface_supported(USB_TO_DUSI))
		{
			t->support_mask &= ~IFS_DUSI;
		}
		if ((t->support_mask & IFS_MICROWIRE) &&
			!usbtoxxx_interface_supported(USB_TO_MICROWIRE))
		{
			t->support_mask &= ~IFS_MICROWIRE;
		}
		if ((t->support_mask & IFS_PWM) &&
			!usbtoxxx_interface_supported(USB_TO_PWM))
		{
			t->support_mask &= ~IFS_PWM;
		}
	}
	
	return VSFERR_NONE;
versaloon_init_fail:
	versaloon_fini();
	return err;
}

static vsf_err_t versaloon_reset(void)
{
	return VSFERR_NONE;
}

// Commit
static vsf_err_t versaloon_peripheral_commit(void)
{
	vsf_err_t err = usbtoxxx_execute_command();
	versaloon_to = VERSALOON_TIMEOUT;
	return err;
}

// Target Voltage
vsf_err_t versaloon_set_target_voltage(uint8_t index, uint16_t voltage)
{
	REFERENCE_PARAMETER(index);
	
	usbtopwr_init(index);
	usbtopwr_config(index);
	usbtopwr_set(index, voltage);
	usbtopwr_fini(index);
	
	return usbtoxxx_execute_command();
}

vsf_err_t versaloon_get_target_voltage(uint8_t index, uint16_t *voltage)
{
	REFERENCE_PARAMETER(index);
	
	usbtopwr_init(index);
	usbtopwr_config(index);
	usbtopwr_get(index, voltage);
	usbtopwr_fini(index);
	
	return usbtoxxx_execute_command();
}

// POLL
static vsf_err_t versaloon_poll_start(uint16_t retry_cnt, uint16_t interval_us)
{
	versaloon_to = VERSALOON_TIMEOUT_LONG;
	return usbtopoll_start(retry_cnt, interval_us);
}
static vsf_err_t versaloon_poll_end(void)
{
	return usbtopoll_end();
}
static vsf_err_t versaloon_poll_checkok(enum poll_check_type_t type, uint16_t offset,
								uint8_t size, uint32_t mask, uint32_t value)
{
	uint8_t equ = 0;
	
	if (POLL_CHECK_EQU == type)
	{
		equ = 1;
	}
	return usbtopoll_checkok(equ, offset, size, mask, value);
}
static vsf_err_t versaloon_poll_checkfail(enum poll_check_type_t type, uint16_t offset,
								uint8_t size, uint32_t mask, uint32_t value)
{
	uint8_t equ = 0;
	
	if (POLL_CHECK_EQU == type)
	{
		equ = 1;
	}
	return usbtopoll_checkfail(equ, offset, size, mask, value);
}
static vsf_err_t versaloon_poll_verifybuff(uint16_t offset, uint16_t size, uint8_t *buff)
{
	return usbtopoll_verifybuff(offset, size, buff);
}

static uint32_t versaloon_display_programmer(void)
{
	// usb parameter
	if (!usb_param_valid())
	{
		usb_set_param(VERSALOON_VID, VERSALOON_PID, VERSALOON_INP,
						VERSALOON_OUTP, 1);
	}
	
	PRINTF(_GETTEXT("Supported Programmer by Versaloon driver:\n"));
	return print_usb_devices(usb_param_vid(), usb_param_pid(),
					VERSALOON_SERIALSTRING_INDEX, usb_param_serial(),
					VERSALOON_PRODUCTSTRING_INDEX, VERSALOON_PRODUCTSTRING);
}

// tick clock
vsf_err_t versaloon_tickclk_init(void)
{
	return VSFERR_NONE;
}

vsf_err_t versaloon_tickclk_fini(void)
{
	return VSFERR_NONE;
}

vsf_err_t versaloon_tickclk_start(void)
{
	return VSFERR_NONE;
}

vsf_err_t versaloon_tickclk_stop(void)
{
	// not supported
	return VSFERR_FAIL;
}

uint32_t versaloon_tickclk_get_count(void)
{
	return (uint32_t)(clock() / (CLOCKS_PER_SEC / 1000));
}

struct interfaces_info_t versaloon_interfaces =
{
	VERSALOON_STRING,
	versaloon_notifier,
	versaloon_display_programmer,
	
	false,
	NULL,
	
	IFS_USART | IFS_SPI | IFS_EBI | IFS_I2C | IFS_GPIO | IFS_POWER | IFS_ISSP |
	IFS_JTAG_LL | IFS_POLL | IFS_JTAG_HL | IFS_SWIM | IFS_JTAG_RAW | IFS_C2 |
	IFS_MSP430_JTAG | IFS_LPC_ICP | IFS_SWD | IFS_BDM | IFS_DUSI |
	IFS_MICROWIRE | IFS_PWM | IFS_ADC,
	
	{	// core
		versaloon_init,
		versaloon_fini,
		versaloon_reset
	},
	{	// tickclk
		versaloon_tickclk_init,
		versaloon_tickclk_fini,
		versaloon_tickclk_start,
		versaloon_tickclk_stop,
		versaloon_tickclk_get_count,
	},
	{	// delay
		usbtodelay_init,
		usbtodelay_delayms,
		usbtodelay_delayus
	},
	{	// gpio
		usbtogpio_init,
		usbtogpio_fini,
		usbtogpio_config_pin,
		usbtogpio_config,
		usbtogpio_set,
		usbtogpio_clear,
		usbtogpio_out,
		usbtogpio_in
	},
	{	// usart
		usbtousart_init,
		usbtousart_fini,
		usbtousart_config,
		usbtousart_send,
		usbtousart_receive,
		usbtousart_status
	},
	{	// spi
		usbtospi_init,
		usbtospi_fini,
		usbtospi_get_ability,
		usbtospi_enable,
		usbtospi_disable,
		usbtospi_config,
		usbtospi_select,
		usbtospi_deselect,
		usbtospi_io
	},
	{
		// ebi
		usbtoebi_init,
		usbtoebi_fini,
		usbtoebi_config,
		usbtoebi_isready,
		usbtoebi_read,
		usbtoebi_write
	},
	{	// i2c
		usbtoi2c_init,
		usbtoi2c_fini,
		usbtoi2c_config,
		usbtoi2c_read,
		usbtoi2c_write
	},
	{	// pwm
		usbtopwm_init,
		usbtopwm_fini,
		usbtopwm_config_mode,
		usbtopwm_config_freq,
		usbtopwm_out,
		usbtopwm_in
	},
	{	// microwire
		usbtomicrowire_init,
		usbtomicrowire_fini,
		usbtomicrowire_config,
		usbtomicrowire_transport,
		usbtomicrowire_poll
	},
	{	// target_voltage
		versaloon_get_target_voltage,
		versaloon_set_target_voltage
	},
	{
		// adc
		usbtoadc_init,
		usbtoadc_fini,
		usbtoadc_config,
		usbtoadc_config_channel,
		usbtoadc_calibrate,
		NULL, NULL, NULL,
		usbtoadc_sample
	},
	{	// issp
		usbtoissp_init,
		usbtoissp_fini,
		usbtoissp_enter_program_mode,
		usbtoissp_leave_program_mode,
		usbtoissp_wait_and_poll,
		usbtoissp_vector
	},
	{	// swd
		usbtoswd_init,
		usbtoswd_fini,
		usbtoswd_config,
		usbtoswd_seqout,
		usbtoswd_seqin,
		usbtoswd_transact
	},
	{	// jtag_hl
		usbtojtaghl_init,
		usbtojtaghl_fini,
		usbtojtaghl_config,
		usbtojtaghl_tms,
		usbtojtaghl_runtest,
		usbtojtaghl_ir,
		usbtojtaghl_dr,
		usbtojtaghl_register_callback
	},
	{	// jtag_ll
		usbtojtagll_init,
		usbtojtagll_fini,
		usbtojtagll_config,
		usbtojtagll_tms,
		usbtojtagll_tms_clocks,
		usbtojtagll_scan
	},
	{	// jtag_raw
		usbtojtagraw_init,
		usbtojtagraw_fini,
		usbtojtagraw_config,
		usbtojtagraw_execute
	},
	{	// msp430_jtag
		usbtomsp430jtag_init,
		usbtomsp430jtag_fini,
		usbtomsp430jtag_config,
		usbtomsp430jtag_ir,
		usbtomsp430jtag_dr,
		usbtomsp430jtag_tclk,
		usbtomsp430jtag_tclk_strobe,
		usbtomsp430jtag_reset,
		usbtomsp430jtag_poll
	},
	{	// msp430_sbw
		usbtomsp430sbw_init,
		usbtomsp430sbw_fini,
		usbtomsp430sbw_config,
		usbtomsp430sbw_ir,
		usbtomsp430sbw_dr,
		usbtomsp430sbw_tclk,
		usbtomsp430sbw_tclk_strobe,
		usbtomsp430sbw_reset,
		usbtomsp430sbw_poll
	},
	{	// c2
		usbtoc2_init,
		usbtoc2_fini,
		usbtoc2_writeaddr,
		usbtoc2_readaddr,
		usbtoc2_readdata,
		usbtoc2_writedata
	},
	{	// lpcicp
		usbtolpcicp_init,
		usbtolpcicp_fini,
		usbtolpcicp_enter_program_mode,
		usbtolpcicp_in,
		usbtolpcicp_out,
		usbtolpcicp_poll_ready
	},
	{	// swim
		usbtoswim_init,
		usbtoswim_fini,
		usbtoswim_config,
		usbtoswim_srst,
		usbtoswim_wotf,
		usbtoswim_rotf,
		usbtoswim_sync,
		usbtoswim_enable
	},
	{	// bdm
		usbtobdm_init,
		usbtobdm_fini,
		usbtobdm_sync,
		usbtobdm_transact
	},
	{	// dusi
		usbtodusi_init,
		usbtodusi_fini,
		usbtodusi_config,
		usbtodusi_io
	},
	{	// poll
		versaloon_poll_start,
		versaloon_poll_end,
		versaloon_poll_checkok,
		versaloon_poll_checkfail,
		versaloon_poll_verifybuff
	},
	versaloon_peripheral_commit
};

