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
#ifndef __AT89S5X_H_INCLUDED__
#define __AT89S5X_H_INCLUDED__

#define S5X_STRING					"at89s5x"

extern const struct program_area_map_t s5x_program_area_map[];
extern const struct program_mode_t s5x_program_mode[];

RESULT s5x_parse_argument(char cmd, const char *argu);
RESULT s5x_program(struct operation_t operations, struct program_info_t *pi, 
				   struct programmer_info_t *prog);

#endif /* __AT89S5X_H_INCLUDED__ */

