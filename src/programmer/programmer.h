/***************************************************************************
 *   Copyright (C) 2009 by Simon Qian <SimonQian@SimonQian.com>            *
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
#ifndef __PROGRAMMER_H_INCLUDED__
#define __PROGRAMMER_H_INCLUDED__

enum jtag_irdr_t
{
	JTAG_SCANTYPE_IR, 
	JTAG_SCANTYPE_DR
};
typedef RESULT (*jtag_callback_t)(enum jtag_irdr_t cmd, uint32_t ir, 
								  uint8_t *dest_buffer, uint8_t *src_buffer, 
								  uint16_t buffer_len, uint16_t *processed);

// msic_cmd
struct misc_cmd_t
{
	const char *cmd_name;
	const char *help_str;
	RESULT (*processor)(uint8_t argc, const char *argv[]);
};
struct misc_param_t
{
	const char *param_name;
	const char *help_str;
	uint32_t value;
};

struct interface_usart_t
{
	RESULT (*usart_init)(void);
	RESULT (*usart_fini)(void);
	RESULT (*usart_config)(uint32_t baudrate, uint8_t datalength, 
								char paritybit, char stopbit, char handshake);
	RESULT (*usart_send)(uint8_t *buf, uint16_t len);
	RESULT (*usart_receive)(uint8_t *buf, uint16_t len);
	RESULT (*usart_status)(uint32_t buffer_size[2]);
};

struct interface_spi_t
{
	RESULT (*spi_init)(void);
	RESULT (*spi_fini)(void);
	RESULT (*spi_config)(uint16_t kHz, uint8_t cpol, uint8_t cpha, 
							uint8_t first_bit);
	RESULT (*spi_io)(uint8_t *out, uint8_t *in, uint16_t len, 
						uint16_t inpos, uint16_t inlen);
};

struct interface_gpio_t
{
	RESULT (*gpio_init)(void);
	RESULT (*gpio_fini)(void);
	RESULT (*gpio_config)(uint16_t pin_mask, uint16_t io, 
							uint16_t input_pull_mask);
	RESULT (*gpio_out)(uint16_t pin_mask, uint16_t value);
	RESULT (*gpio_in)(uint16_t pin_mask, uint16_t *value);
};

struct interface_delay_t
{
	RESULT (*delayms)(uint16_t ms);
	RESULT (*delayus)(uint16_t us);
};

struct interface_issp_t
{
	RESULT (*issp_init)(void);
	RESULT (*issp_fini)(void);
	RESULT (*issp_enter_program_mode)(uint8_t mode);
	RESULT (*issp_leave_program_mode)(uint8_t mode);
	RESULT (*issp_wait_and_poll)(void);
	RESULT (*issp_vector)(uint8_t operate, uint8_t addr, uint8_t data, 
							uint8_t *buf);
};

struct interface_swd_t
{
	RESULT (*swd_init)(void);
	RESULT (*swd_fini)(void);
	RESULT (*swd_seqout)(uint8_t *data, uint16_t bitlen);
	RESULT (*swd_seqin)(uint8_t *data, uint16_t bitlen);
	RESULT (*swd_transact)(uint8_t request, uint32_t *data);
	RESULT (*swd_setpara)(uint8_t trn, uint16_t retry, uint16_t dly);
	RESULT (*swd_get_last_ack)(uint8_t *result);
};

struct interface_jtag_hl_t
{
	RESULT (*jtag_hl_init)(void);
	RESULT (*jtag_hl_fini)(void);
	RESULT (*jtag_hl_config)(uint16_t kHz, uint8_t ub, uint8_t ua, 
								uint16_t bb, uint16_t ba);
	RESULT (*jtag_hl_tms)(uint8_t* tms, uint16_t bitlen);
	RESULT (*jtag_hl_runtest)(uint32_t cycles);
	RESULT (*jtag_hl_ir)(uint8_t *ir, uint8_t len, uint8_t idle, 
							uint8_t want_ret);
	RESULT (*jtag_hl_dr)(uint8_t *dr, uint16_t len, uint8_t idle, 
							uint8_t want_ret);
	RESULT (*jtag_hl_register_callback)(jtag_callback_t send_callback, 
										jtag_callback_t receive_callback);
};

struct interface_jtag_ll_t
{
	RESULT (*jtag_ll_init)(void);
	RESULT (*jtag_ll_fini)(void);
	RESULT (*jtag_ll_set_frequency)(uint16_t kHz);
	RESULT (*jtag_ll_tms)(uint8_t *tms, uint8_t bytelen);
	RESULT (*jtag_ll_tms_clocks)(uint32_t bytelen, uint8_t tms);
	RESULT (*jtag_ll_scan)(uint8_t* r, uint16_t bitlen, 
							uint8_t tms_before_valid, uint8_t tms_before, 
							uint8_t tms_after0, uint8_t tms_after1);
};

struct interface_msp430jtag_t
{
	RESULT (*msp430jtag_init)(void);
	RESULT (*msp430jtag_fini)(void);
	RESULT (*msp430jtag_config)(uint8_t has_test);
	RESULT (*msp430jtag_ir)(uint8_t *ir, uint8_t want_ret);
	RESULT (*msp430jtag_dr)(uint32_t *dr, uint8_t len, uint8_t want_ret);
	RESULT (*msp430jtag_tclk)(uint8_t value);
	RESULT (*msp430jtag_tclk_strobe)(uint16_t cnt);
	RESULT (*msp430jtag_reset)(void);
	RESULT (*msp430jtag_poll)(uint32_t dr, uint32_t mask, uint32_t value, 
						uint8_t len, uint16_t poll_cnt, uint8_t toggle_tclk);
};

struct interface_msp430sbw_t
{
	RESULT (*msp430sbw_init)(void);
	RESULT (*msp430sbw_fini)(void);
	RESULT (*msp430sbw_config)(uint8_t has_test);
	RESULT (*msp430sbw_ir)(uint8_t *ir, uint8_t want_ret);
	RESULT (*msp430sbw_dr)(uint32_t *dr, uint8_t len, uint8_t want_ret);
	RESULT (*msp430sbw_tclk)(uint8_t value);
	RESULT (*msp430sbw_tclk_strobe)(uint16_t cnt);
	RESULT (*msp430sbw_reset)(void);
	RESULT (*msp430sbw_poll)(uint32_t dr, uint32_t mask, uint32_t value, 
						uint8_t len, uint16_t poll_cnt, uint8_t toggle_tclk);
};

struct interface_c2_t
{
	RESULT (*c2_init)(void);
	RESULT (*c2_fini)(void);
	RESULT (*c2_addr_write)(uint8_t addr);
	RESULT (*c2_addr_read)(uint8_t *data);
	RESULT (*c2_data_read)(uint8_t *data, uint8_t len);
	RESULT (*c2_data_write)(uint8_t *data, uint8_t len);
};

struct interface_i2c_t
{
	RESULT (*i2c_init)(void);
	RESULT (*i2c_fini)(void);
	RESULT (*i2c_set_speed)(uint16_t kHz, uint16_t dead_cnt, 
							uint16_t byte_interval);
	RESULT (*i2c_read)(uint16_t chip_addr, uint8_t *data, uint16_t data_len, 
						uint8_t stop);
	RESULT (*i2c_write)(uint16_t chip_addr, uint8_t *data, uint16_t data_len, 
						uint8_t stop);
};

struct interface_lpcicp_t
{
	RESULT (*lpcicp_init)(void);
	RESULT (*lpcicp_fini)(void);
	RESULT (*lpcicp_enter_program_mode)(void);
	RESULT (*lpcicp_in)(uint8_t *buff, uint16_t len);
	RESULT (*lpcicp_out)(uint8_t *buff, uint16_t len);
	RESULT (*lpcicp_poll_ready)(uint8_t data, uint8_t *ret, uint8_t setmask, 
								uint8_t clearmask, uint16_t pollcnt);
};

struct interface_swim_t
{
	RESULT (*swim_init)(void);
	RESULT (*swim_fini)(void);
	RESULT (*swim_set_param)(uint8_t mHz, uint8_t cnt0, uint8_t cnt1);
	RESULT (*swim_srst)(void);
	RESULT (*swim_wotf)(uint8_t *data, uint16_t bytelen, uint32_t addr);
	RESULT (*swim_rotf)(uint8_t *data, uint16_t bytelen, uint32_t addr);
	RESULT (*swim_sync)(uint8_t mHz);
	RESULT (*swim_enable)(void);
};

struct interface_target_voltage_t
{
	RESULT (*get_target_voltage)(uint16_t *voltage);
	RESULT (*set_target_voltage)(uint16_t voltage);
};

struct interface_poll_t
{
	RESULT (*poll_start)(uint16_t retry, uint16_t interval_us);
	RESULT (*poll_end)(void);
	RESULT (*poll_checkbyte)(uint8_t offset, uint8_t mask, uint8_t value);
	RESULT (*poll_checkfail)(uint8_t offset, uint8_t mask, uint8_t value);
};

struct interfaces_info_t
{
	struct interface_target_voltage_t target_voltage;
	struct interface_usart_t usart;
	struct interface_spi_t spi;
	struct interface_gpio_t gpio;
	struct interface_delay_t delay;
	struct interface_issp_t issp;
	struct interface_swd_t swd;
	struct interface_jtag_hl_t jtag_hl;
	struct interface_jtag_ll_t jtag_ll;
	struct interface_msp430jtag_t msp430jtag;
	struct interface_msp430sbw_t msp430sbw;
	struct interface_c2_t c2;
	struct interface_i2c_t i2c;
	struct interface_lpcicp_t lpcicp;
	struct interface_swim_t swim;
	struct interface_poll_t poll;
	RESULT (*peripheral_commit)(void);
};

struct programmer_info_t
{
	// the 3 element listed below MUST be define valid
	// other functions can be initialized in .init_ability()
	char *name;
	RESULT (*parse_argument)(char cmd, const char *argu);
	RESULT (*init_capability)(void *p);
	uint32_t (*display_programmer)(void);
	
	// init and fini
	RESULT (*init)(void);
	RESULT (*fini)(void);
	
	// interfaces supported
	uint32_t interfaces_mask;
	
	// peripheral
	struct interfaces_info_t interfaces;
	
	// mass-product support
	RESULT (*query_mass_product_data_size)(uint32_t *size);
	RESULT (*download_mass_product_data)(const char *name, uint8_t *buffer, 
										 uint32_t len);
	
	// firmware update support
	RESULT (*enter_firmware_update_mode)(void);
};

#define PROGRAMMER_DEFINE(name, parse_argument, init_capability, 	\
						  display_programmer)						\
	{\
		name, parse_argument, init_capability, display_programmer, 	\
		0, 0, 0, \
		{\
			{0, 0},\
			{0, 0, 0, 0, 0, 0},\
			{0, 0, 0, 0},\
			{0, 0, 0, 0, 0},\
			{0, 0},\
			{0, 0, 0, 0, 0, 0},\
			{0, 0, 0, 0, 0, 0, 0},\
			{0, 0, 0, 0, 0, 0, 0, 0},\
			{0, 0, 0, 0, 0, 0},\
			{0, 0, 0, 0, 0, 0, 0, 0, 0},\
			{0, 0, 0, 0, 0, 0, 0, 0, 0},\
			{0, 0, 0, 0, 0, 0},\
			{0, 0, 0, 0, 0},\
			{0, 0, 0, 0, 0, 0},\
			{0, 0, 0, 0, 0, 0, 0, 0},\
			{0, 0, 0, 0},\
			0\
		},\
		0, 0, 0\
	}

extern struct programmer_info_t *cur_programmer;
extern struct programmer_info_t programmers_info[];

void programmer_print_list(void);
void programmer_print_help(void);
RESULT programmer_init(const char *programmer);
RESULT programmer_run_script(char *cmd);

#endif /* __PROGRAMMER_H_INCLUDED__ */

