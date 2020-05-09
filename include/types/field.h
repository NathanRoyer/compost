/*
 * LibPTA types manipulations features, C header
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

#ifndef TYPES_FIELD_H
#define TYPES_FIELD_H

#include "type.h"

typedef PTA_STRUCT field_info_a field_info_a_t;
typedef PTA_STRUCT field_info_b field_info_b_t;

#include "page.h"
#include "dict.h"
#include "refc.h"

#define FIBF_BASIC      0b00000000
#define FIBF_POINTER    0b00000001
#define FIBF_AUTO_INST  0b00000010
#define FIBF_DEPENDENT  0b00000101 // includes FIBF_POINTER
#define FIBF_NESTED     0b00001000
#define FIBF_ARRAY      0b00010000
#define FIBF_MALLOC     0b00100000
#define FIBF_REFERENCES 0b01000001 // includes FIBF_POINTER
#define FIBF_PREV_OWNER 0b10000000

typedef PTA_STRUCT field_info_a {
	type_t * field_type;
	size_t data_offset;
} field_info_a_t;

typedef PTA_STRUCT field_info_b {
	type_t * field_type;
	uint8_t flags;
} field_info_b_t;

void * pta_get_obj(void * obj);

type_t * pta_type_of(void * obj);

void * pta_get_c_object(void * obj);

void * pta_prepare(void * obj, type_t * type);

void * attach_field(void * host, void * field, void * dependent);

void * pta_detach_dependent(void * field);

void * pta_attach_dependent(void * destination, void * dependent);

void ** get_previous_owner(void * ref_field);

void pta_set_reference(void * field, void * obj);

void pta_clear_reference(void * field);

void check_references(void ** refc, recursive_call_t * rec);

void reset_fields(void * c_object, type_t * type);

void * pta_create_type(void * any_paged_obj, size_t nested_objects, size_t referencers, size_t object_size, uint8_t flags);

size_t pta_set_dynamic_field(type_t * type, type_t * field_type, array field_name, size_t offset, uint8_t flags);

uint8_t pta_get_flags(void * obj);

#define pta_is_pointer(obj) (pta_get_flags(obj) & FIBF_POINTER)

void * pta_get_field(void * obj, array field_name);

#define compute_paged_size(type) ((type)->object_size + GET_OFFSET_ZONE(type))
#define compute_page_limit(type) (page_size - (type)->paged_size + 1) // located just after the last instance

#endif