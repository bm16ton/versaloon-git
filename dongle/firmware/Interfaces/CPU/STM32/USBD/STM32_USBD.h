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

#define stm32_usbd_ep_num					8

RESULT stm32_usbd_init(void *device);
RESULT stm32_usbd_fini(void);
RESULT stm32_usbd_reset(void);
RESULT stm32_usbd_poll(void);
RESULT stm32_usbd_connect(void);
RESULT stm32_usbd_disconnect(void);
RESULT stm32_usbd_set_address(uint8_t address);
uint8_t stm32_usbd_get_address(void);
RESULT stm32_usbd_suspend(void);
RESULT stm32_usbd_resume(void);
RESULT stm32_usbd_lowpower(uint8_t level);
uint32_t stm32_usbd_get_frame_number(void);

RESULT stm32_usbd_ep_reset(uint8_t idx);
RESULT stm32_usbd_ep_set_type(uint8_t idx, enum usb_ep_type_t type);
enum usb_ep_type_t stm32_usbd_ep_get_type(uint8_t idx);

RESULT stm32_usbd_ep_set_IN_handler(uint8_t idx, vsfusbd_IN_hanlder_t handler);
RESULT stm32_usbd_ep_set_IN_dbuffer(uint8_t idx);
RESULT stm32_usbd_ep_set_IN_epsize(uint8_t idx, uint16_t epsize);
uint16_t stm32_usbd_ep_get_IN_epsize(uint8_t idx);
RESULT stm32_usbd_ep_set_IN_state(uint8_t idx, enum usb_ep_state_t state);
enum usb_ep_state_t stm32_usbd_ep_get_IN_state(uint8_t idx);
RESULT stm32_usbd_ep_set_IN_count(uint8_t idx, uint16_t size);
RESULT stm32_usbd_ep_write_IN_buffer(uint8_t idx, uint8_t *buffer, uint16_t size);

RESULT stm32_usbd_ep_set_OUT_handler(uint8_t idx, vsfusbd_OUT_hanlder_t handler);
RESULT stm32_usbd_ep_set_OUT_dbuffer(uint8_t idx);
RESULT stm32_usbd_ep_set_OUT_epsize(uint8_t idx, uint16_t epsize);
uint16_t stm32_usbd_ep_get_OUT_epsize(uint8_t idx);
RESULT stm32_usbd_ep_set_OUT_state(uint8_t idx, enum usb_ep_state_t state);
enum usb_ep_state_t stm32_usbd_ep_get_OUT_state(uint8_t idx);
uint16_t stm32_usbd_ep_get_OUT_count(uint8_t idx);
RESULT stm32_usbd_ep_read_OUT_buffer(uint8_t idx, uint8_t *buffer, uint16_t size);

