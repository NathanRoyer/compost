/*
 * LibPTA dictionnary features, C header
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

#ifndef TYPES_DICT_H
#define TYPES_DICT_H

#include "type.h"

typedef PTA_STRUCT dict_block dict_block_t;
typedef PTA_STRUCT dict dict_t;

#include "page.h"

typedef PTA_STRUCT dict_block {
	void * equal; // to next key part
	void * unequal; // to alternative key part
	void * value;
	int8_t key_part;
} dict_block_t;

typedef PTA_STRUCT dict {
	void * first_block;
	void * empty_key_v;
} dict_t;

void * pta_dict_get(void * d_refc, array key);

bool pta_dict_has(void * d_refc, array key);

void * pta_dict_set(void * d_refc, array key, void * value);

size_t pta_dict_count(void * d_refc);

void pta_dict_get_next_index(void * d_refc, array * index);

#endif