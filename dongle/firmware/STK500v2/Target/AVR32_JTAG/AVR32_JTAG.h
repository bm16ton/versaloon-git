/**************************************************************************
 *  Copyright (C) 2008 - 2010 by Simon Qian                               *
 *  SimonQian@SimonQian.com                                               *
 *                                                                        *
 *  Project:    STK500V2                                                  *
 *  File:       AVR32_JTAG.h                                              *
 *  Author:     SimonQian                                                 *
 *  Versaion:   See changelog                                             *
 *  Purpose:    header for AVR32_JTAG support                             *
 *  License:    See license                                               *
 *------------------------------------------------------------------------*
 *  Change Log:                                                           *
 *      YYYY-MM-DD:     What(by Who)                                      *
 *      2010-08-30:     created(by SimonQian)                             *
 **************************************************************************/

// Instructions:
#define AVR32_JTAG_INS_Len						5
#define AVR32_JTAG_INS_IDCODE					0x01
#define AVR32_JTAG_INS_SAMPLE_PRELOAD			0x02
#define AVR32_JTAG_INS_EXTEST					0x03
#define AVR32_JTAG_INS_INTEST					0x04
#define AVR32_JTAG_INS_CLAMP					0x06
#define AVR32_JTAG_INS_RESET					0x0C
#define AVR32_JTAG_INS_NEXUS_ACCESS				0x10
#define AVR32_JTAG_INS_MEMORY_WORD_ACCESS		0x11
#define AVR32_JTAG_INS_MEMORY_BLOCK_ACCESS		0x12
#define AVR32_JTAG_INS_CANCEL_ACCESS			0x13
#define AVR32_JTAG_INS_MEMORY_SERVICE			0x14
#define AVR32_JTAG_INS_MEMORY_SIZED_ACCESS		0x15
#define AVR32_JTAG_INS_SYNC						0x17
#define AVR32_JTAG_INS_HALT						0x1C
#define AVR32_JTAG_INS_BYPASS					0x1F

#define AVR32_JTAG_READ							1
#define AVR32_JTAG_WRITE						0

#define AVR32_JTAG_RTI_CYCLE					1

#define AVR32_JTAG_Instr(instr)					JTAG_TAP_Instr(instr, AVR32_JTAG_INS_Len, AVR32_JTAG_RTI_CYCLE)
#define AVR32_JTAG_Data(tdi, len)				JTAG_TAP_Data(tdi, len, AVR32_JTAG_RTI_CYCLE)
#define AVR32_JTAG_DataInPtr(ptdo, len)			JTAG_TAP_DataInPtr(ptdo, len, AVR32_JTAG_RTI_CYCLE)
#define AVR32_JTAG_DataOutPtr(ptdi, len)		JTAG_TAP_DataOutPtr(ptdi, len, AVR32_JTAG_RTI_CYCLE)
#define AVR32_JTAG_DataPtr(ptdi, ptdo, len)		JTAG_TAP_DataPtr(ptdi, ptdo, len, AVR32_JTAG_RTI_CYCLE)

uint8 AVR32_JTAG_SAB_WordAccess(uint8 *addr, uint8 *data, uint8 r);
uint8 AVR32_JTAG_SAB_WordBlockAccess(uint8 *addr, uint8 *data, uint8 r, uint8 len);
uint8 AVR32_JTAG_NexusAccess(uint8 a, uint8 *data, uint8 r);
