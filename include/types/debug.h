/*
 * Compost debugging features, C header
 * Copyright (C) 2020 Nathan ROYER
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef TYPES_DEBUG_H
#define TYPES_DEBUG_H

#include "type.h"
#include "refc.h"
#include "field.h"
#include "descriptor.h"
#include "page.h"
#include "dict.h"
#include <stdio.h>

typedef COMPOST_STRUCT dbg_field_info {
	array name;
	size_t size;
	size_t offset;
	uint8_t flags;
} dbg_field_info_t;

void compost_print_regs();

size_t compost_print_cstr(array string);

void compost_print_fields(void * obj, type_t * type);

void compost_show(void * obj);

void compost_show_references(void * obj);

void compost_show_pages(void * any_paged_address);

#endif