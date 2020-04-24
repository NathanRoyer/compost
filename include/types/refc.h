/*
 * LibPTA garbage collection features, C header
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

#ifndef TYPES_REFC_H
#define TYPES_REFC_H

#include "type.h"

typedef void * refc_t;

typedef struct recursive_call recursive_call_t;
typedef struct recursive_call {
	void * arg;
	recursive_call_t * next;
} recursive_call_t;

#include "page.h"

// a direct self-reference is used as fake dependence
#define FAKE_DEPENDENT(any_paged_obj) (any_paged_obj)

void ** find_raw_refc(void * address);

void * pta_get_final_obj(void * address);

void ** find_refc(void * address, recursive_call_t * rec);

bool is_obj_referenced(void * obj);

bool pta_protect(void * obj);

void pta_unprotect(void * obj);

int pta_type_instances(type_t * type);

void pta_remove_superfluous_pages(type_t * type, bool should_delete);

void pta_garbage_collect(type_t * root_type);

#endif