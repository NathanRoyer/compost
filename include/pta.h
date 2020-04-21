/*
 * LibPTA, C header
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

#ifndef PAGED_TYPES_ALLOCATOR_H
#define PAGED_TYPES_ALLOCATOR_H

// type.h
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define PTA_STRUCT struct __attribute__ ((__packed__))

typedef PTA_STRUCT {
	int length;
	char * data;
} pta_array;

#define pta_const_array(s) ((pta_array){ sizeof(s) - 1, s })
#define pta_char_at(key, i) (((char *)key.data)[i])

typedef void * pta_obj;

#define PTA_TYPE_BASIC     0b00000000 // you can use this one
#define PTA_TYPE_PRIMITIVE 0b00000001 // you can use this one
#define PTA_TYPE_RSV1      0b00000010
#define PTA_TYPE_MALLOC_F  0b00000100 // you can use this one
#define PTA_TYPE_RSV2      0b00001000
#define PTA_TYPE_RSV3      0b00010000
#define PTA_TYPE_RSV4      0b00100000
#define PTA_TYPE_FLAG_2    0b01000000 // you can use this one
#define PTA_TYPE_FLAG_3    0b10000000 // you can use this one

typedef PTA_STRUCT type {
	pta_obj dfia;
	pta_obj dfib;
	size_t object_size;
	size_t offsets;
	size_t paged_size;
	size_t page_limit;
	pta_obj dynamic_fields;
	pta_obj static_fields;
	pta_obj page_list;
	pta_obj client_data; // this is customizable
	uint8_t flags;
} pta_type_t;

extern uint8_t pta_fake_type_undefined;
#define PTA_UNDEFINED ((pta_type_t *)&pta_fake_type_undefined)

typedef struct {
	pta_type_t * rt;
	pta_type_t * szt;
	pta_type_t * chrt;
	pta_type_t * dht;
} pta_context_t;

extern pta_context_t pta_setup();

typedef void (*pta_for_each_type_callback)(pta_obj type, void * arg);

extern void pta_for_each_type(pta_type_t * root_type, pta_for_each_type_callback cb, void * arg);

// page.h
size_t pta_pages;

extern pta_obj pta_spot(pta_type_t * type);

extern void * pta_spot_dependent(void * destination, pta_type_t * type);

extern pta_obj pta_spot_array(pta_type_t * type, size_t size);

extern void * pta_spot_array_dependent(void * destination, pta_type_t * type, size_t size);

size_t pta_array_length(pta_obj array);

void pta_resize_array(pta_obj array, size_t length);

void * pta_array_get(pta_obj array, size_t index);

// field.h
#define PTA_FIELD_BASIC     0b000000
#define PTA_FIELD_DEPENDENT 0b000001
#define PTA_FIELD_AUTO_INST 0b000010
#define PTA_FIELD_POINTER   0b000100
#define PTA_FIELD_ARRAY     0b010101
// free will be called on these fields upon garbage collection :
#define PTA_FIELD_MALLOC    0b100000
// please declare them with a primitive type which can hold a pointer.
// types with such fields must have the PTA_TYPE_MALLOC_F flag.

extern pta_type_t * pta_type_of(pta_obj obj);

extern void * pta_get_c_object(pta_obj obj);

extern pta_obj pta_prepare(pta_obj obj, pta_type_t * type);

extern void * pta_detach_dependent(void * field);

extern void * pta_attach_dependent(void * destination, void * dependent);

extern void * pta_create_type(pta_obj any_pta_obj, size_t nested_objects, size_t object_size, uint8_t flags);

extern size_t pta_set_dynamic_field(pta_type_t * type, pta_type_t * field_type, pta_array field_name, size_t offset, uint8_t flags);

uint8_t pta_get_flags(void * obj);

#define pta_is_pointer(obj) (pta_get_flags(obj) & PTA_FIBF_POINTER)

void * pta_get_field(void * obj, pta_array field_name);


// refc.h
extern pta_obj pta_get_obj(pta_obj address);

extern pta_obj pta_get_final_obj(pta_obj address);

extern size_t pta_get_refc(pta_obj address);

extern void pta_increment_refc(pta_obj obj);

extern void pta_decrement_refc(pta_obj obj);

extern int pta_type_instances(pta_type_t * type);

extern void pta_remove_superfluous_pages(pta_type_t * type);

extern void pta_garbage_collect(pta_type_t * root_type);

// dict.h
extern void * pta_dict_get(pta_obj dictionnary, pta_array key);

extern bool pta_dict_has(pta_obj dictionnary, pta_array key);

extern void * pta_dict_set(pta_obj dictionnary, pta_array key, void * value);

extern size_t pta_dict_count(pta_obj d_refc);

extern void pta_get_next_index(pta_obj dictionnary, pta_array * index);

// debug.h
extern size_t pta_print_cstr(pta_array string);

extern void pta_show(pta_obj obj);

#endif