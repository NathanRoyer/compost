/*
 * Compost, C header
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

#define COMPOST_STRUCT struct __attribute__ ((__packed__))

typedef COMPOST_STRUCT {
	size_t length;
	char * data;
} compost_array;

#define compost_const_array(s) ((compost_array){ sizeof(s) - 1, s })
#define compost_char_at(key, i) (((char *)key.data)[i])

typedef void * compost_obj;

#define COMPOST_TYPE_BASIC     0b00000000 // you can use this one
#define COMPOST_TYPE_PRIMITIVE 0b00000001 // you can use this one
#define COMPOST_TYPE_INTERNAL  0b00000010
#define COMPOST_TYPE_MALLOC_F  0b00000100 // you can use this one
#define COMPOST_TYPE_ARRAY     0b00001000
#define COMPOST_TYPE_CHAR      0b00010000
#define COMPOST_TYPE_ROOT      0b00100000
#define COMPOST_TYPE_FIB       0b01000000
#define COMPOST_TYPE_CSTM_FLAG 0b10000000 // you can define and use this one

typedef COMPOST_STRUCT type {
	compost_obj dfia;
	compost_obj dfib;
	size_t object_size;
	size_t offsets;
	size_t paged_size;
	compost_obj variants;
	compost_obj dynamic_fields;
	compost_obj static_fields;
	compost_obj page_list;
	compost_obj client_data; // this is customizable
	uint8_t flags;
} compost_type_t;

typedef struct {
	compost_type_t * rt;
	compost_type_t * szt;
	compost_type_t * chrt;
	compost_type_t * dht;
	compost_type_t * art;
} compost_context_t;

extern compost_context_t compost_setup();

typedef void (*compost_for_each_type_callback)(compost_obj type, void * arg);

extern void compost_for_each_type(compost_type_t * root_type, compost_for_each_type_callback cb, void * arg);

// page.h
extern size_t compost_pages;

typedef COMPOST_STRUCT compost_array_obj {
	void * rsv[2];
	compost_obj content_type;
	size_t capacity;
} compost_array_obj_t;

extern compost_obj compost_spot(compost_type_t * type);

extern void * compost_spot_dependent(void * destination, compost_type_t * type);

extern compost_obj compost_spot_array(compost_type_t * type, size_t size);

extern void * compost_spot_array_dependent(void * destination, compost_type_t * type, size_t size);

void * compost_array_get(compost_obj array, size_t index);

size_t compost_array_find(compost_obj array, compost_obj item);

// field.h

typedef struct compost_constraint {
	size_t field_offset;
	size_t value;
} compost_constraint_t;

#define COMPOST_FIELD_BASIC      0b0000000
#define COMPOST_FIELD_POINTER    0b0000001
#define COMPOST_FIELD_AUTO_INST  0b0000010
#define COMPOST_FIELD_DEPENDENT  0b0000101
#define COMPOST_FIELD_MALLOC     0b0100000
#define COMPOST_FIELD_REFERENCES 0b1000001

extern compost_obj compost_get_obj(compost_obj address);

extern compost_obj compost_create_type_variant(compost_type_t * base_type, compost_constraint_t * constraints, size_t len);

extern bool compost_type_mismatch(void * type, compost_obj obj);

extern compost_type_t * compost_type_of(compost_obj obj, bool base_type);

extern void * compost_get_c_object(compost_obj obj);

extern compost_obj compost_prepare(compost_obj obj, compost_type_t * type);

extern void * compost_detach_dependent(void * field);

extern void * compost_attach_dependent(void * destination, void * dependent);

extern void compost_set_reference(compost_obj * field, compost_obj obj);

extern void compost_clear_reference(compost_obj * field);

extern compost_obj compost_create_type(void * any_compost_address, size_t nested_objects, size_t referencers, size_t object_size, uint8_t flags);

extern size_t compost_set_dynamic_field(compost_type_t * type, compost_type_t * field_type, compost_array field_name, size_t offset, uint8_t flags);

extern uint8_t compost_get_flags(compost_obj obj);

#define compost_is_pointer(obj) (compost_get_flags(obj) & COMPOST_FIBF_POINTER)

extern compost_obj compost_get_field(compost_obj obj, size_t length, char * name, bool dynamic);


// refc.h
extern compost_obj compost_get_final_obj(compost_obj address);

extern size_t compost_get_refc(compost_obj address);

extern bool compost_protect(compost_obj address);

extern void compost_unprotect(compost_obj address);

extern int compost_type_instances(compost_type_t * type);

extern void compost_remove_superfluous_pages(compost_type_t * type);

extern void compost_garbage_collect(compost_type_t * root_type);

// dict.h

typedef COMPOST_STRUCT compost_dict {
	compost_obj first_block;
	void * empty_key_v;
} compost_dict_t;

extern void * compost_dict_get_al(compost_obj dictionnary, compost_obj key);

extern void * compost_dict_set_al(compost_obj dictionnary, compost_obj key, void * value);

extern void * compost_dict_get_pa(compost_obj dictionnary, compost_array key);

extern void * compost_dict_set_pa(compost_obj dictionnary, compost_array key, void * value);

extern size_t compost_dict_count(compost_obj d_refc);

extern void compost_get_next_index(compost_obj dictionnary, compost_array * index);

// debug.h
extern void compost_print_regs();

extern size_t compost_print_cstr(compost_array string);

extern void compost_show(compost_obj obj);

extern void compost_print_fields(compost_obj obj, compost_type_t * type);

extern void compost_show_references(compost_obj obj);

extern void compost_show_pages(void * any_compost_address);

#endif