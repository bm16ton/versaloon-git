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

#ifndef __SCRIPTS_H_INCLUDED__
#define __SCRIPTS_H_INCLUDED__

#include "tool/list/list.h"

struct vss_cmd_t
{
	const char *cmd_name;
	const char *help_str;
	vsf_err_t (*processor)(uint16_t argc, const char *argv[]);
};
struct vss_cmd_list_t
{
	char *list_name;
	struct vss_cmd_t *cmd;
	struct sllist list;
};

struct vss_param_t
{
	const char *param_name;
	const char *help_str;
	uint32_t value;
};
struct vss_param_list_t
{
	char *list_name;
	struct vss_param_t *param;
	struct sllist list;
};

#define VSS_CMD_LIST(str_name, cmd_array)		\
			{(str_name), (cmd_array), {NULL}}
#define VSS_PARAM_LIST(str_name, param_array)	\
			{(str_name), (param_array), {NULL}}

#define VSS_HANDLER(name)						\
	vsf_err_t (name)(uint16_t argc, const char *argv[])

#define VSS_CMD(name, helpstr, handler)			\
	{\
		(name),\
		(helpstr),\
		(handler)\
	}
#define VSS_CMD_END								VSS_CMD(NULL, NULL, NULL)
#define VSS_PARAM(name, helpstr, default)		\
	{\
		(name),\
		(helpstr),\
		(default)\
	}
#define VSS_PARAM_END							VSS_PARAM(NULL, NULL, 0)

#define VSS_CHECK_ARGC(n)						\
	if (argc != (n))\
	{\
		LOG_ERROR(ERRMSG_INVALID_CMD, argv[0]);\
		vss_print_help(argv[0]);\
		return VSFERR_FAIL;\
	}
#define VSS_CHECK_ARGC_2(n1, n2)				\
	if ((argc != (n1)) && (argc != (n2)))\
	{\
		LOG_ERROR(ERRMSG_INVALID_CMD, argv[0]);\
		vss_print_help(argv[0]);\
		return VSFERR_FAIL;\
	}
#define VSS_CHECK_ARGC_3(n1, n2, n3)			\
	if ((argc != (n1)) && (argc != (n2)) && (argc != (n3)))\
	{\
		LOG_ERROR(ERRMSG_INVALID_CMD, argv[0]);\
		vss_print_help(argv[0]);\
		return VSFERR_FAIL;\
	}
#define VSS_CHECK_ARGC_MIN(n)					\
	if (argc < (n))\
	{\
		LOG_ERROR(ERRMSG_INVALID_CMD, argv[0]);\
		vss_print_help(argv[0]);\
		return VSFERR_FAIL;\
	}
#define VSS_CHECK_ARGC_MAX(n)					\
	if (argc > (n))\
	{\
		LOG_ERROR(ERRMSG_INVALID_CMD, argv[0]);\
		vss_print_help(argv[0]);\
		return VSFERR_FAIL;\
	}
#define VSS_CHECK_ARGC_RANGE(min, max)			\
	if ((argc < (min)) || (argc > (max)))\
	{\
		LOG_ERROR(ERRMSG_INVALID_CMD, argv[0]);\
		vss_print_help(argv[0]);\
		return VSFERR_FAIL;\
	}

#define VSS_COMMENT_CHAR						'#'
#define VSS_HIDE_CHAR							'@'

vsf_err_t vss_init(void);
vsf_err_t vss_fini(void);
vsf_err_t vss_register_cmd_list(struct vss_cmd_list_t *cmdlist);
vsf_err_t vss_register_param_list(struct vss_param_list_t *paramlist);
void vss_set_fatal_error(void);
vsf_err_t vss_cmd_supported_by_notifier(const struct vss_cmd_t *notifier,
										char *notify_cmd);
vsf_err_t vss_call_notifier(const struct vss_cmd_t *notifier,
							char *notify_cmd, char *notify_param);
vsf_err_t vss_cmd_supported(char *name);
vsf_err_t vss_print_help(const char *name);
vsf_err_t vss_run_script(char *cmd);
vsf_err_t vss_run_cmd(uint16_t argc, const char *argv[]);
vsf_err_t vss_get_binary_buffer(uint16_t argc, const char *argv[],
						uint8_t data_size, uint32_t data_num, void **pbuff);

#endif		// __SCRIPTS_H_INCLUDED__
