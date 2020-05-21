/*
 * LibPTA setup features, C header
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

#ifndef TYPES_TYPE_H
#define TYPES_TYPE_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define PTA_STRUCT struct __attribute__ ((__packed__))

typedef PTA_STRUCT {
	size_t length;
	char * data;
} array;

#define const_array(s) ((array){ sizeof(s) - 1, s })
#define char_at(key, i) (((char *)key.data)[i])

// this helps a lot for pointer arithmetics:
typedef union ptr ptr_t;
typedef union ptr {
	ptr_t * p;
	size_t s;
} ptr_t;
#define SP(v) ((ptr_t){ .s = (size_t)(v) })
#define PP(v) ((ptr_t){ .p = (ptr_t *)(v) })
#define PTR_BITS (sizeof(void *) * 8)

#define CEILDIV(a, b) ((a / b) + ((a % b) != 0))

#define TYPE_BASIC     0b00000000
#define TYPE_PRIMITIVE 0b00000001
#define TYPE_INTERNAL  0b00000010
#define TYPE_MALLOC_F  0b00000100
#define TYPE_ARRAY     0b00001000
#define TYPE_CHAR      0b00010000
#define TYPE_ROOT      0b00100000
#define TYPE_FIB       0b01000000
#define TYPE_CSTM_FLAG 0b10000000

#define GET_OFFSET_ZONE(type) ((type)->offsets > sizeof(void *) ? (type)->offsets : sizeof(void *))

typedef PTA_STRUCT type {
	void * dfia;     // field offsets
	void * dfib;     // real fields
	size_t object_size;     // 
	size_t offsets;         // 
	size_t paged_size;      // 
	void * variants; // how many pages to allocate at once - not used yet
	void * dynamic_fields;  // name -> field_info_*
	void * static_fields;   // name -> *
	void * page_list;       // 
	void * client_data;     // 
	uint8_t flags;          // 
} type_t;

uint8_t fake_type_go_back;
#define GO_BACK   ((type_t *)&fake_type_go_back)

typedef struct {
	type_t * rt;
	type_t * szt;
	type_t * chrt;
	type_t * dht;
	type_t * art;
} context_t;

context_t pta_setup();

typedef void (*pta_for_each_type_callback)(void * type_obj, void * arg);

void pta_for_each_type(type_t * root_type, pta_for_each_type_callback cb, void * arg);

#endif