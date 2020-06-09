/*
 * Compost types manipulations features, C header
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

typedef COMPOST_STRUCT field_info_a field_info_a_t;
typedef COMPOST_STRUCT field_info_b field_info_b_t;

#include "page.h"
#include "dict.h"
#include "refc.h"

#define FIBF_BASIC      0b00000000
#define FIBF_POINTER    0b00000001
#define FIBF_AUTO_INST  0b00000010
#define FIBF_DEPENDENT  0b00000101 // includes FIBF_POINTER
#define FIBF_NESTED     0b00001000
#define FIBF_ARRAY      -DONT- // 0b00010000
#define FIBF_MALLOC     0b00100000
#define FIBF_REFERENCES 0b01000001 // includes FIBF_POINTER
#define FIBF_PREV_OWNER 0b10000000

typedef COMPOST_STRUCT field_info_a {
	type_t * field_type;
	size_t data_offset;
} field_info_a_t;

typedef COMPOST_STRUCT field_info_b {
	type_t * field_type;
	uint8_t flags;
} field_info_b_t;

typedef struct constraint {
	size_t field_offset;
	size_t value;
} constraint_t;

typedef struct obj_info {
	size_t offsets_zone;
	size_t offset;
	type_t * page_type;
} obj_info_t;

#define CHSZ (sizeof(uint8_t))
#define GET_FIA(type, i) ((field_info_a_t *)(ARRAY_GET((type)->dfia, CHSZ + sizeof(field_info_a_t), i) + CHSZ))
#define GET_FIB(type, i) ((field_info_b_t *)(ARRAY_GET((type)->dfib, CHSZ + sizeof(field_info_b_t), i) + CHSZ))

void * compost_get_obj(void * obj);

void * compost_create_type_variant(type_t * base_type, constraint_t * constraints, size_t len);

bool compost_type_mismatch(type_t * variant, void * obj);

type_t * compost_type_of(void * obj, bool base_type);

void * compost_get_c_object(void * obj);

void * compost_prepare(void * obj, type_t * type);

void * detach_field(void * raw_refc, void * field);

void * attach_field(void * raw_refc, void * field, void * dependent);

void * compost_detach_dependent(void * field);

void * compost_attach_dependent(void * destination, void * dependent);

void ** get_previous_owner(void * ref_field);

void compost_set_reference(void * field, void * obj);

void compost_clear_reference(void * field);

void check_references(void ** refc, recursive_call_t * rec);

void reset_fields(void * c_object, type_t * type);

void * compost_create_type(void * any_paged_obj, size_t nested_objects, size_t referencers, size_t object_size, uint8_t flags);

size_t compost_set_dynamic_field(type_t * type, type_t * field_type, array field_name, size_t offset, uint8_t flags);

uint8_t compost_get_flags(void * obj);

#define compost_is_pointer(obj) (compost_get_flags(obj) & FIBF_POINTER)

void * advance_obj_ptr(void * obj, obj_info_t info, size_t field_offset, bool is_fib);

void * compost_get_field(void * obj, size_t length, char * name, bool dynamic);

#define compute_paged_size(type) ((type)->object_size + GET_OFFSET_ZONE(type))
#define compute_page_limit(type) (page_size - (type)->paged_size + 1) // located just after the last instance

#endif