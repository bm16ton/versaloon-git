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
 *      2011-05-09:     created(by SimonQian)                             *
 **************************************************************************/

#include "app_type.h"
#include "compiler.h"
#include "interfaces.h"

// TODO: remove MACROs below to nuc400_reg.h
#define NUC400_CLOCK_AHBCLK_UDC20_EN		((uint32_t)1 << 10)
#define NUC400_USBD_PHYCTL_DP_PULLUP		((uint32_t)1 << 8)
#define NUC400_USBD_PHYCTL_SUSPEND			((uint32_t)1 << 9)

// CEPCTL
#define USB_CEPCTL_NAKCLR					((uint32_t)0x00000000)
#define USB_CEPCTL_STALL					((uint32_t)0x00000002)
#define USB_CEPCTL_ZEROLEN					((uint32_t)0x00000004)
#define USB_CEPCTL_FLUSH					((uint32_t)0x00000008)

// EPxCFG
#define USB_EP_CFG_VALID					((uint32_t)0x00000001)
#define USB_EP_CFG_TYPE_BULK				((uint32_t)0x00000002)
#define USB_EP_CFG_TYPE_INT					((uint32_t)0x00000004)
#define USB_EP_CFG_TYPE_ISO					((uint32_t)0x00000006)
#define USB_EP_CFG_TYPE_MASK				((uint32_t)0x00000006)
#define USB_EP_CFG_DIR_OUT					((uint32_t)0x00000000)
#define USB_EP_CFG_DIR_IN					((uint32_t)0x00000008)

// EPxRSPCTL
#define USB_EP_RSPCTL_FLUSH					((uint32_t)0x00000001)
#define USB_EP_RSPCTL_MODE_AUTO				((uint32_t)0x00000000)
#define USB_EP_RSPCTL_MODE_MANUAL			((uint32_t)0x00000002)
#define USB_EP_RSPCTL_MODE_FLY				((uint32_t)0x00000004)
#define USB_EP_RSPCTL_MODE_MASK				((uint32_t)0x00000006)
#define USB_EP_RSPCTL_TOGGLE				((uint32_t)0x00000008)
#define USB_EP_RSPCTL_HALT					((uint32_t)0x00000010)
#define USB_EP_RSPCTL_ZEROLEN				((uint32_t)0x00000020)
#define USB_EP_RSPCTL_SHORTTXEN				((uint32_t)0x00000040)
#define USB_EP_RSPCTL_DISBUF				((uint32_t)0x00000080)

#define NUC400_USBD_EP_REG(ep, reg)			\
	*((__IO uint32_t *)((uint32_t)&USBD->reg + (uint32_t)((uint8_t)(ep) * 0x28)))
#define NUC400_USBD_EP_REG8(ep, reg)		\
	*((__IO uint8_t *)((uint32_t)&USBD->reg + (uint32_t)((uint8_t)(ep) * 0x28)))

#if IFS_USBD_EN

#include "NUC400_USBD.h"
#include "NUC472_442.h"

#define NUC400_USBD_EP_NUM					(6 + 2)
const uint8_t nuc400_usbd_ep_num = NUC400_USBD_EP_NUM;
struct interface_usbd_callback_t nuc400_usbd_callback;
static uint16_t EP_Cfg_Ptr = 0x1000;
static uint16_t max_ctl_ep_size = 64;

// true if data direction in setup packet is device to host
static bool nuc400_setup_status_IN;

#define NUC400_USBD_EPIN					0x10
#define NUC400_USBD_EPOUT					0x00
static int8_t nuc400_usbd_epaddr[NUC400_USBD_EP_NUM];

extern void nuc400_unlock_reg(void);
extern void nuc400_lock_reg(void);

vsf_err_t nuc400_usbd_init(uint32_t int_priority)
{
	memset(nuc400_usbd_epaddr, -1, sizeof(nuc400_usbd_epaddr));
	nuc400_usbd_epaddr[0] = 0 | NUC400_USBD_EPIN;
	nuc400_usbd_epaddr[1] = 0 | NUC400_USBD_EPOUT;
	
	nuc400_unlock_reg();
	// Enable IP clock
	CLK->AHBCLK |= CLK_AHBCLK_USBDCKEN_Msk;
	// Enable USB PHY: USB device
	SYS->USBPHY = 0x100;
	nuc400_lock_reg();
	
	// Enable PHY
	USBD->PHYCTL |= USBD_PHYCTL_PHYEN_Msk;
	// wait PHY clock ready
	while (1)
	{
		USBD->EPAMPS = 0x20;
		if (USBD->EPAMPS == 0x20)
			break;
	}
	
	// Enable USB FULL SPEED
	USBD->OPER = 0;
	while ((USBD->OPER & 0x4) != 0);
	// Enable USB interrupt
	USBD->GINTEN = USBD_GINTEN_USBIE_Msk | USBD_GINTEN_CEPIE_Msk;
	// Enable BUS interrupt
	USBD->BUSINTEN = USBD_BUSINTEN_RSTIEN_Msk;
	USBD->CEPINTEN = USBD_CEPINTEN_SETUPPKIEN_Msk |
					USBD_CEPINTEN_TXPKIEN_Msk | USBD_CEPINTEN_STSDONEIEN_Msk;
	
	NVIC->IP[USBD_IRQn] = int_priority;
	NVIC->ISER[USBD_IRQn >> 0x05] = 1UL << (USBD_IRQn & (uint8_t)0x1F);
	return VSFERR_NONE;
}

vsf_err_t nuc400_usbd_fini(void)
{
	USBD->PHYCTL &= ~USBD_PHYCTL_PHYEN_Msk;
	CLK->AHBCLK &= ~CLK_AHBCLK_USBDCKEN_Msk;
	NVIC->ICER[USBD_IRQn >> 0x05] = 1UL << (USBD_IRQn & (uint8_t)0x1F);
	return VSFERR_NONE;
}

vsf_err_t nuc400_usbd_reset(void)
{
	return VSFERR_NONE;
}

void USB_Istr(void);
vsf_err_t nuc400_usbd_poll(void)
{
	USB_Istr();
	return VSFERR_NONE;
}

vsf_err_t nuc400_usbd_connect(void)
{
	USBD->PHYCTL |= USBD_PHYCTL_DPPUEN_Msk;
	return VSFERR_NONE;
}

vsf_err_t nuc400_usbd_disconnect(void)
{
	USBD->PHYCTL &= ~USBD_PHYCTL_DPPUEN_Msk;
	return VSFERR_NONE;
}

vsf_err_t nuc400_usbd_set_address(uint8_t address)
{
	USBD->FADDR = address;
	return VSFERR_NONE;
}

uint8_t nuc400_usbd_get_address(void)
{
	return USBD->FADDR;
}

vsf_err_t nuc400_usbd_suspend(void)
{
	return VSFERR_NONE;
}

vsf_err_t nuc400_usbd_resume(void)
{
	return VSFERR_NONE;
}

vsf_err_t nuc400_usbd_lowpower(uint8_t level)
{
	return VSFERR_NONE;
}

uint32_t nuc400_usbd_get_frame_number(void)
{
	return USBD->FRAMECNT >> 3;
}

vsf_err_t nuc400_usbd_get_setup(uint8_t *buffer)
{
	buffer[0] = USBD->SETUP1_0 & 0xFF;
	nuc400_setup_status_IN = (buffer[0] & 0x80) > 0;
	buffer[1] = (USBD->SETUP1_0 >> 8) & 0xFF;
	buffer[2] = USBD->SETUP3_2 & 0xFF;
	buffer[3] = (USBD->SETUP3_2 >> 8) & 0xFF;
	buffer[4] = USBD->SETUP5_4 & 0xFF;
	buffer[5] = (USBD->SETUP5_4 >> 8) & 0xFF;
	buffer[6] = USBD->SETUP7_6 & 0xFF;
	buffer[7] = (USBD->SETUP7_6 >> 8) & 0xFF;
	return VSFERR_NONE;
}

vsf_err_t nuc400_usbd_prepare_buffer(void)
{
	EP_Cfg_Ptr = 0x1000;
	return VSFERR_NONE;
}

vsf_err_t nuc400_usbd_ep_reset(uint8_t idx)
{
	return VSFERR_NONE;
}

static int8_t nuc400_usbd_ep(uint8_t idx)
{
	uint8_t i;
	
	for (i = 0; i < sizeof(nuc400_usbd_epaddr); i++)
	{
		if ((int8_t)idx == nuc400_usbd_epaddr[i])
		{
			return (int8_t)i;
		}
	}
	return -1;
}

static int8_t nuc400_usbd_get_free_ep(uint8_t idx)
{
	uint8_t i;
	
	for (i = 1; i < sizeof(nuc400_usbd_epaddr); i++)
	{
		if (-1 == nuc400_usbd_epaddr[i])
		{
			nuc400_usbd_epaddr[i] = (int8_t)idx;
			return i;
		}
	}
	return -1;
}

static void nuc400_usbd_set_eptype(uint8_t ep, uint32_t type)
{
	NUC400_USBD_EP_REG(ep, EPARSPCTL) =
				USB_EP_RSPCTL_FLUSH | USB_EP_RSPCTL_MODE_MANUAL;
	NUC400_USBD_EP_REG(ep, EPACFG) &= ~USB_EP_CFG_TYPE_MASK;
	NUC400_USBD_EP_REG(ep, EPACFG) |= type | USB_EP_CFG_VALID;
}

vsf_err_t nuc400_usbd_ep_set_type(uint8_t idx, enum interface_usbd_eptype_t type)
{
	int8_t index_in, index_out;
	uint32_t eptype;
	
	index_in = nuc400_usbd_ep(idx | NUC400_USBD_EPIN);
	index_out = nuc400_usbd_ep(idx | NUC400_USBD_EPOUT);
	if ((0 == idx) || ((index_in < 0) && (index_out < 0)))
	{
		return VSFERR_FAIL;
	}
	
	switch (type)
	{
	case USB_EP_TYPE_CONTROL:
		return VSFERR_NONE;
	case USB_EP_TYPE_INTERRUPT:
		eptype = USB_EP_CFG_TYPE_INT;
		break;
	case USB_EP_TYPE_BULK:
		eptype = USB_EP_CFG_TYPE_BULK;
		break;
	case USB_EP_TYPE_ISO:
		eptype = USB_EP_CFG_TYPE_ISO;
		break;
	default:
		return VSFERR_INVALID_PARAMETER;
	}
	if (index_in > 1)
	{
		index_in -= 2;
		nuc400_usbd_set_eptype(index_in, eptype);
		USBD->GINTEN |= USBD_GINTEN_EPAIE_Msk << index_in;
		NUC400_USBD_EP_REG(index_in, EPAINTEN) =
					USBD_EPINTEN_TXPKIEN_Msk | USBD_EPINTEN_RXPKIEN_Msk;
	}
	if (index_out > 1)
	{
		index_out -= 2;
		nuc400_usbd_set_eptype(index_out, eptype);
		USBD->GINTEN |= USBD_GINTEN_EPAIE_Msk << index_out;
		NUC400_USBD_EP_REG(index_out, EPAINTEN) =
					USBD_EPINTEN_TXPKIEN_Msk | USBD_EPINTEN_RXPKIEN_Msk;
	}
	return VSFERR_NONE;
}

vsf_err_t nuc400_usbd_ep_set_IN_dbuffer(uint8_t idx)
{
	return VSFERR_NONE;
}

bool nuc400_usbd_ep_is_IN_dbuffer(uint8_t idx)
{
	return false;
}

vsf_err_t nuc400_usbd_ep_switch_IN_buffer(uint8_t idx)
{
	return VSFERR_NONE;
}

vsf_err_t nuc400_usbd_ep_set_IN_epsize(uint8_t idx, uint16_t epsize)
{
	int8_t index, index_out;
	
	index_out = nuc400_usbd_ep(idx | NUC400_USBD_EPOUT);
	index = nuc400_usbd_get_free_ep(idx | NUC400_USBD_EPIN);
	if (index < 0)
	{
		return VSFERR_FAIL;
	}
	
	if (0 == idx)
	{
		if ((EP_Cfg_Ptr - epsize) > 0x1000)
		{
			return VSFERR_NOT_ENOUGH_RESOURCES;
		}
		
		EP_Cfg_Ptr -= epsize & 1 ? epsize + 1 : epsize;
		USBD->CEPBUFSTART = EP_Cfg_Ptr;
		USBD->CEPBUFEND = EP_Cfg_Ptr + epsize - 1;
		max_ctl_ep_size = epsize;
		return VSFERR_NONE;
	}
	else if (index > 1)
	{ 
		index -= 2;
		if (index_out < 0)
		{
			if ((EP_Cfg_Ptr - epsize) > 0x1000)
			{
				return VSFERR_NOT_ENOUGH_RESOURCES;
			}
			
			EP_Cfg_Ptr -= epsize & 1 ? epsize + 1 : epsize;
			NUC400_USBD_EP_REG(index, EPABUFSTART) = EP_Cfg_Ptr;
			NUC400_USBD_EP_REG(index, EPABUFEND) = EP_Cfg_Ptr + epsize - 1;
		}
		else if (index_out > 1)
		{
			index_out -= 2;
			NUC400_USBD_EP_REG(index, EPABUFSTART) =\
									NUC400_USBD_EP_REG(index_out, EPABUFSTART);
			NUC400_USBD_EP_REG(index, EPABUFEND) =\
									NUC400_USBD_EP_REG(index_out, EPABUFEND);
		}
		else
		{
			return VSFERR_BUG;
		}
		NUC400_USBD_EP_REG(index, EPAMPS) = epsize;
		NUC400_USBD_EP_REG(index, EPACFG) |= USB_EP_CFG_DIR_IN;
		NUC400_USBD_EP_REG(index, EPACFG) &= ~(0xF << 4);
		NUC400_USBD_EP_REG(index, EPACFG) |= (idx & 0x0F) << 4;
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

uint16_t nuc400_usbd_ep_get_IN_epsize(uint8_t idx)
{
	int8_t index;
	
	index = nuc400_usbd_ep(idx | NUC400_USBD_EPIN);
	if ((index < 0) || (index >= NUC400_USBD_EP_NUM))
	{
		return 0;
	}
	
	if (0 == idx)
	{
		return max_ctl_ep_size;
	}
	else if (index > 1)
	{
		index -= 2;
		return NUC400_USBD_EP_REG(index, EPAMPS);
	}
	return 0;
}

vsf_err_t nuc400_usbd_ep_set_IN_stall(uint8_t idx)
{
	int8_t index;
	
	index = nuc400_usbd_ep(idx | NUC400_USBD_EPIN);
	if ((index < 0) || (index >= NUC400_USBD_EP_NUM))
	{
		return VSFERR_FAIL;
	}
	
	if (0 == idx)
	{
		USBD->CEPCTL = 2;
		return VSFERR_NONE;
	}
	else if (index > 1)
	{
		index -= 2;
		NUC400_USBD_EP_REG(index, EPARSPCTL) =
			(NUC400_USBD_EP_REG(index, EPARSPCTL) & 0xF7) | USB_EP_RSPCTL_HALT;
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

vsf_err_t nuc400_usbd_ep_clear_IN_stall(uint8_t idx)
{
	int8_t index;
	
	index = nuc400_usbd_ep(idx | NUC400_USBD_EPIN);
	if ((index < 0) || (index >= NUC400_USBD_EP_NUM))
	{
		return VSFERR_FAIL;
	}
	
	if (0 == idx)
	{
		USBD->CEPCTL &= ~3;
		return VSFERR_NONE;
	}
	else if (index > 1)
	{
		index -= 2;
		NUC400_USBD_EP_REG(index, EPARSPCTL) &= ~USB_EP_RSPCTL_HALT;
		NUC400_USBD_EP_REG(index, EPARSPCTL) |= USB_EP_RSPCTL_TOGGLE;
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

bool nuc400_usbd_ep_is_IN_stall(uint8_t idx)
{
	int8_t index;
	
	index = nuc400_usbd_ep(idx | NUC400_USBD_EPIN);
	if ((index < 0) || (index >= NUC400_USBD_EP_NUM))
	{
		return true;
	}
	
	if (0 == idx)
	{
		return (USBD->CEPCTL & USB_CEPCTL_STALL) > 0;
	}
	else if (index > 1)
	{
		index -= 2;
		return (NUC400_USBD_EP_REG(index, EPARSPCTL) & USB_EP_RSPCTL_HALT) > 0;
	}
	return true;
}

vsf_err_t nuc400_usbd_ep_reset_IN_toggle(uint8_t idx)
{
	int8_t index;
	
	index = nuc400_usbd_ep(idx | NUC400_USBD_EPIN);
	if ((index < 0) || (index >= NUC400_USBD_EP_NUM))
	{
		return VSFERR_FAIL;
	}
	
	if (0 == idx)
	{
		return VSFERR_NOT_SUPPORT;
	}
	else if (index > 1)
	{
		index -= 2;
		NUC400_USBD_EP_REG(index, EPARSPCTL) |= USB_EP_RSPCTL_TOGGLE;
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

vsf_err_t nuc400_usbd_ep_toggle_IN_toggle(uint8_t idx)
{
	return VSFERR_NOT_SUPPORT;
}

vsf_err_t nuc400_usbd_ep_set_IN_count(uint8_t idx, uint16_t size)
{
	int8_t index;
	
	index = nuc400_usbd_ep(idx | NUC400_USBD_EPIN);
	if ((index < 0) || (index >= NUC400_USBD_EP_NUM))
	{
		return VSFERR_FAIL;
	}
	
	if (0 == idx)
	{
		if (nuc400_setup_status_IN && (0 == size))
		{
			USBD->CEPCTL = USB_CEPCTL_NAKCLR;
		}
		else
		{
			USBD->CEPTXCNT = size;
		}
		return VSFERR_NONE;
	}
	else if (index > 1)
	{
		index -= 2;
		NUC400_USBD_EP_REG(index, EPATXCNT) = size;
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

vsf_err_t nuc400_usbd_ep_write_IN_buffer(uint8_t idx, uint8_t *buffer,
											uint16_t size)
{
	int8_t index;
	uint32_t i;
	
	index = nuc400_usbd_ep(idx | NUC400_USBD_EPIN);
	if ((index < 0) || (index >= NUC400_USBD_EP_NUM))
	{
		return VSFERR_FAIL;
	}
	
	if (0 == idx)
	{
		for (i = 0; i < size; i++)
		{
			USBD->CEPDAT_BYTE = buffer[i];
		}
		return VSFERR_NONE;
	}
	else if (index > 1)
	{
		index -= 2;
		for (i = 0; i < size; i++)
		{
			NUC400_USBD_EP_REG8(index, EPADAT_BYTE) = buffer[i];
		}
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

vsf_err_t nuc400_usbd_ep_set_OUT_dbuffer(uint8_t idx)
{
	return VSFERR_NONE;
}

bool nuc400_usbd_ep_is_OUT_dbuffer(uint8_t idx)
{
	return false;
}

vsf_err_t nuc400_usbd_ep_switch_OUT_buffer(uint8_t idx)
{
	return VSFERR_NONE;
}

vsf_err_t nuc400_usbd_ep_set_OUT_epsize(uint8_t idx, uint16_t epsize)
{
	int8_t index, index_in;
	
	index_in = nuc400_usbd_ep(idx | NUC400_USBD_EPIN);
	index = nuc400_usbd_get_free_ep(idx | NUC400_USBD_EPOUT);
	if (index < 0)
	{
		return VSFERR_FAIL;
	}
	
	if (0 == idx)
	{
		// has already been(will be) allocated in set_IN_epsize
		return VSFERR_NONE;
	}
	else if (index > 1)
	{
		index -= 2;
		if (index_in < 0)
		{
			if ((EP_Cfg_Ptr - epsize) > 0x1000)
			{
				return VSFERR_NOT_ENOUGH_RESOURCES;
			}
			
			EP_Cfg_Ptr -= epsize & 1 ? epsize + 1 : epsize;
			NUC400_USBD_EP_REG(index, EPABUFSTART) = EP_Cfg_Ptr;
			NUC400_USBD_EP_REG(index, EPABUFEND) = EP_Cfg_Ptr + epsize - 1;
		}
		else if (index_in > 1)
		{
			index_in -= 2;
			NUC400_USBD_EP_REG(index, EPABUFSTART) =\
									NUC400_USBD_EP_REG(index_in, EPABUFSTART);
			NUC400_USBD_EP_REG(index, EPABUFEND) =\
									NUC400_USBD_EP_REG(index_in, EPABUFEND);
		}
		else
		{
			return VSFERR_BUG;
		}
		NUC400_USBD_EP_REG(index, EPAMPS) = epsize;
		NUC400_USBD_EP_REG(index, EPACFG) &= ~USB_EP_CFG_DIR_IN;
		NUC400_USBD_EP_REG(index, EPACFG) &= ~(0xF << 4);
		NUC400_USBD_EP_REG(index, EPACFG) |= (idx & 0x0F) << 4;
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

uint16_t nuc400_usbd_ep_get_OUT_epsize(uint8_t idx)
{
	int8_t index;
	
	index = nuc400_usbd_ep(idx | NUC400_USBD_EPOUT);
	if ((index < 0) || (index >= NUC400_USBD_EP_NUM))
	{
		return 0;
	}
	
	if (0 == idx)
	{
		return max_ctl_ep_size;
	}
	else if (index > 1)
	{
		index -= 2;
		return NUC400_USBD_EP_REG(index, EPAMPS);
	}
	return 0;
}

vsf_err_t nuc400_usbd_ep_set_OUT_stall(uint8_t idx)
{
	int8_t index;
	
	index = nuc400_usbd_ep(idx | NUC400_USBD_EPOUT);
	if ((index < 0) || (index >= NUC400_USBD_EP_NUM))
	{
		return VSFERR_FAIL;
	}
	
	if (0 == idx)
	{
		USBD->CEPCTL = 2;
		return VSFERR_NONE;
	}
	else if (index > 1)
	{
		index -= 2;
		NUC400_USBD_EP_REG(index, EPARSPCTL) =
			(NUC400_USBD_EP_REG(index, EPARSPCTL) & 0xF7) | USB_EP_RSPCTL_HALT;
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

vsf_err_t nuc400_usbd_ep_clear_OUT_stall(uint8_t idx)
{
	int8_t index;
	
	index = nuc400_usbd_ep(idx | NUC400_USBD_EPOUT);
	if ((index < 0) || (index >= NUC400_USBD_EP_NUM))
	{
		return VSFERR_FAIL;
	}
	
	if (0 == idx)
	{
		USBD->CEPCTL = USBD->CEPCTL & ~3;
		return VSFERR_NONE;
	}
	else if (index > 1)
	{
		index -= 2;
		NUC400_USBD_EP_REG(index, EPARSPCTL) &= ~USB_EP_RSPCTL_HALT;
		NUC400_USBD_EP_REG(index, EPARSPCTL) |= USB_EP_RSPCTL_TOGGLE;
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

bool nuc400_usbd_ep_is_OUT_stall(uint8_t idx)
{
	int8_t index;
	
	index = nuc400_usbd_ep(idx | NUC400_USBD_EPOUT);
	if ((index < 0) || (index >= NUC400_USBD_EP_NUM))
	{
		return true;
	}
	
	if (0 == idx)
	{
		return (USBD->CEPCTL & USB_CEPCTL_STALL) > 0;
	}
	else if (index > 1)
	{
		index -= 2;
		return (NUC400_USBD_EP_REG(index, EPARSPCTL) & USB_EP_RSPCTL_HALT) > 0;
	}
	return VSFERR_BUG;
}

vsf_err_t nuc400_usbd_ep_reset_OUT_toggle(uint8_t idx)
{
	int8_t index;
	
	index = nuc400_usbd_ep(idx | NUC400_USBD_EPOUT);
	if ((index < 0) || (index >= NUC400_USBD_EP_NUM))
	{
		return VSFERR_FAIL;
	}
	
	if (0 == idx)
	{
		return VSFERR_NOT_SUPPORT;
	}
	else if (index > 1)
	{
		index -= 2;
		NUC400_USBD_EP_REG(index, EPARSPCTL) |= USB_EP_RSPCTL_TOGGLE;
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

vsf_err_t nuc400_usbd_ep_toggle_OUT_toggle(uint8_t idx)
{
	return VSFERR_NOT_SUPPORT;
}

uint16_t nuc400_usbd_ep_get_OUT_count(uint8_t idx)
{
	int8_t index;
	
	index = nuc400_usbd_ep(idx | NUC400_USBD_EPOUT);
	if ((index < 0) || (index >= NUC400_USBD_EP_NUM))
	{
		return 0;
	}
	
	if (0 == idx)
	{
		return USBD->CEPRXCNT;
	}
	else if (index > 1)
	{
		index -= 2;
		return NUC400_USBD_EP_REG(index, EPADATCNT) & 0xFF;
	}
	return 0;
}

vsf_err_t nuc400_usbd_ep_read_OUT_buffer(uint8_t idx, uint8_t *buffer,
											uint16_t size)
{
	int8_t index;
	uint32_t i;
	
	index = nuc400_usbd_ep(idx | NUC400_USBD_EPOUT);
	if ((index < 0) || (index >= NUC400_USBD_EP_NUM))
	{
		return VSFERR_FAIL;
	}
	
	if (0 == idx)
	{
		for (i = 0; i < size; i++)
		{
			buffer[i] = USBD->CEPDAT_BYTE;
		}
		return VSFERR_NONE;
	}
	else if (index > 1)
	{
		index -= 2;
		size = min(size, NUC400_USBD_EP_REG(index, EPAMPS));
		for (i = 0; i < size; i++)
		{
			buffer[i] = NUC400_USBD_EP_REG8(index, EPADAT_BYTE);
		}
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

vsf_err_t nuc400_usbd_ep_enable_OUT(uint8_t idx)
{
	int8_t index;
	
	index = nuc400_usbd_ep(idx | NUC400_USBD_EPOUT);
	if ((index < 0) || (index >= NUC400_USBD_EP_NUM))
	{
		return VSFERR_FAIL;
	}
	
	if (0 == idx)
	{	
		if (!nuc400_setup_status_IN)
		{
			USBD->CEPCTL = USB_CEPCTL_NAKCLR;
		}
		return VSFERR_NONE;
	}
	else if (index > 1)
	{
		index -= 2;
		NUC400_USBD_EP_REG(index, EPACFG) |= USB_EP_CFG_VALID;
		return VSFERR_NONE;
	}
	return VSFERR_BUG;
}

void USB_Istr(void)
{
	static uint32_t IrqStL, IrqSt;
	
	IrqStL = USBD->GINTSTS;
	IrqStL &= USBD->GINTEN;
	
	if (!IrqStL)
	{
		return;
	}
	
	// USB interrupt
	if (IrqStL & USBD_GINTSTS_USBIF_Msk) {
		IrqSt = USBD->BUSINTSTS;
		IrqSt &= USBD->BUSINTEN;
		
		if (IrqSt & USBD_BUSINTSTS_RSTIF_Msk) {
			if (nuc400_usbd_callback.on_reset != NULL)
			{
				nuc400_usbd_callback.on_reset(\
						nuc400_usbd_callback.param);
			}
			USBD->BUSINTSTS = USBD_BUSINTSTS_RSTIF_Msk;
			USBD->CEPINTSTS = 0x1ffc;
		}
		
/*		if (IrqSt & USBD_BUSINTSTS_RESUMEIF_Msk) {
			USBD->BUSINTSTS = USBD_BUSINTSTS_RESUMEIF_Msk;
		}
		
		if (IrqSt & USBD_BUSINTSTS_SUSPENDIF_Msk) {
			USBD->BUSINTSTS = USBD_BUSINTSTS_SUSPENDIF_Msk;
		}
*/
	}
	
	// CEP interrupt
	if (IrqStL & USBD_GINTSTS_CEPIF_Msk) {
		IrqSt = USBD->CEPINTSTS;
		IrqSt &= USBD->CEPINTEN;
		
		if (IrqSt & USBD_CEPINTSTS_SETUPPKIF_Msk) {
			USBD->CEPINTSTS = USBD_CEPINTSTS_SETUPPKIF_Msk;
			
			if (nuc400_usbd_callback.on_setup != NULL)
			{
				nuc400_usbd_callback.on_setup(\
						nuc400_usbd_callback.param);
			}
		}
		
		if (IrqSt & USBD_CEPINTSTS_TXPKIF_Msk) {
			USBD->CEPINTSTS = USBD_CEPINTSTS_TXPKIF_Msk;
			
			if (nuc400_usbd_callback.on_in != NULL)
			{
				nuc400_usbd_callback.on_in(nuc400_usbd_callback.param, 0);
			}
		}
		if (IrqSt & USBD_CEPINTSTS_RXPKIF_Msk) {
			USBD->CEPINTSTS = USBD_CEPINTSTS_RXPKIF_Msk;
			
			if (nuc400_usbd_callback.on_out != NULL)
			{
				nuc400_usbd_callback.on_out(nuc400_usbd_callback.param, 0);
			}
		}
		
		if (IrqSt & USBD_CEPINTSTS_STSDONEIF_Msk) {
			USBD->CEPINTSTS = USBD_CEPINTSTS_STSDONEIF_Msk;
			
			if (nuc400_setup_status_IN)
			{
				if (nuc400_usbd_callback.on_in != NULL)
				{
					nuc400_usbd_callback.on_in(nuc400_usbd_callback.param, 0);
				}
			}
			else
			{
				if (nuc400_usbd_callback.on_out != NULL)
				{
					nuc400_usbd_callback.on_out(nuc400_usbd_callback.param, 0);
				}
			}
		}
	}
	
	// EP interrupt
	if (IrqStL & (~3))
	{
		int i;
		for (i = 0; i < 12; i++)
		{
			if (IrqStL & (1 << (i + 2)))
			{
				IrqSt = NUC400_USBD_EP_REG(i, EPAINTSTS);
				
				if (IrqSt & USBD_EPINTSTS_TXPKIF_Msk)
				{
					NUC400_USBD_EP_REG(i, EPAINTSTS) = USBD_EPINTSTS_TXPKIF_Msk;
					if (nuc400_usbd_callback.on_in != NULL)
					{
						nuc400_usbd_callback.on_in(\
								nuc400_usbd_callback.param,\
								nuc400_usbd_epaddr[i + 2] & 0x0F);
					}
				}
				if (IrqSt & USBD_EPINTSTS_RXPKIF_Msk)
				{
					NUC400_USBD_EP_REG(i, EPAINTSTS) = USBD_EPINTSTS_RXPKIF_Msk;
					if (nuc400_usbd_callback.on_out != NULL)
					{
						nuc400_usbd_callback.on_out(\
								nuc400_usbd_callback.param,\
								nuc400_usbd_epaddr[i + 2] & 0x0F);
					}
				}
			}
		}
	}
}

ROOTFUNC void USBD_IRQHandler(void)
{
	USB_Istr();
}

#endif
