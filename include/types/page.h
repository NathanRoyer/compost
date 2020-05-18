/*
 * LibPTA instances storage features, C header
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

#ifndef TYPES_PAGE_H
#define TYPES_PAGE_H

#include <sys/mman.h>
#include <unistd.h>
#include "type.h"

typedef PTA_STRUCT array_obj array_obj_t;
typedef PTA_STRUCT root_page root_page_t;

#include "refc.h"
#include "field.h"
#include "descriptor.h"

typedef PTA_STRUCT array_obj {
	void * refc;
	array_obj_t * next;
	void * content_type;
	size_t capacity;
} array_obj_t;

size_t page_size;
size_t page_rel_mask;
size_t page_mask;
size_t pta_pages;

#define PG_REFC2(desc) ((void *)(PP(desc).s + sizeof(page_desc_t)))
#define PG_TYPE2(desc) ((type_t *)((desc)->type.p))
#define PG_NEXT(desc)  ((page_desc_t *)((desc)->next.p))
#define PG_FLAGS(desc) ((desc)->flags_and_limit.s & page_rel_mask)
#define PG_RAW_LIMIT(desc) ((desc)->flags_and_limit.s & page_mask)
#define PG_LIMIT(desc, type) (PG_RAW_LIMIT(desc) - (type)->paged_size + 1)

void * pta_spot(type_t * type);

void * pta_spot_dependent(void * destination, type_t * type);

void * pta_spot_array(type_t * type, size_t size);

void * pta_spot_array_dependent(void * destination, type_t * type, size_t size);

void shrink_array(page_desc_t * desc, array_obj_t * array_obj, size_t array_bytes);

void grow_array(page_desc_t * desc, array_obj_t * array_obj);

size_t pta_array_capacity(array_obj_t * array_obj);

void * pta_array_get(array_obj_t * array_obj, size_t index);

size_t pta_array_find(array_obj_t * array_obj, void * item);

size_t page_occupied_slots(size_t pg_limit, uint8_t flags, void * first_instance, type_t * type);

page_desc_t * update_page_list(page_desc_t * desc, type_t * type, bool should_delete);

root_page_t * get_root_page(void * obj);

typedef PTA_STRUCT root_page {
	page_desc_t header;

	refc_t rt_refc; // root type
	type_t rt;

	refc_t szt_refc; // size type
	type_t szt;

	refc_t chrt_refc; // char type
	type_t chrt;

	refc_t fiat_refc; // field info a type
	type_t fiat;

	refc_t fibt_refc; // field info b type
	type_t fibt;

	refc_t dht_refc; // dict header type
	type_t dht;

	refc_t dbt_refc; // dict block type
	type_t dbt;

	refc_t art_refc; // array_type
	type_t art;
} root_page_t;

#endif