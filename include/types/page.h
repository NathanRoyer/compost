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

typedef PTA_STRUCT array_part array_part_t;
typedef PTA_STRUCT page_list page_list_t;
typedef PTA_STRUCT root_page root_page_t;

#include "refc.h"
#include "field.h"
#include "descriptor.h"

#define PAGE_BASIC     0b000
#define PAGE_DEPENDENT 0b001
#define PAGE_ARRAY     0b010
#define PAGE_FAKE_PGLT 0b100

typedef PTA_STRUCT array_part {
	void * refc;
	size_t size;
	size_t following_free_space;
	array_part_t * next_part;
} array_part_t;

#define PG_START(addr)      (void *)((size_t)addr & page_mask)
#define PG_TYPE(addr)       --- dont use ---
#define PG_REL(addr)        (((size_t)addr) & page_rel_mask)
#define PG_NEXT(page)       ((void *)page + page_size)
#define PG_REFC(page)       ((void *)page + sizeof(page_desc_t))
#define PG_FLAGS(page)      --- dont use ---
#define PG_FLAG(page, flag) --- dont use ---
#define ARRAY_CONTENT_SIZE(array, type) (((array_part_t *)array)->size * (type->object_size + type->offsets))

size_t page_size;
size_t page_rel_mask;
size_t page_mask;
size_t pta_pages;

typedef PTA_STRUCT page_list {
	void * next;
	void * first_instance;
} page_list_t;

/* MACRO array_next (array part pointer)
 *
 * Return value: the next array in page, no matter if it is referenced.
 */
#define array_next(array_part, type) (array_part_t *)((void *)(array_part) + sizeof(array_part_t) + ARRAY_CONTENT_SIZE((array_part), (type)) + ((array_part_t *)(array_part))->following_free_space)

void * new_page(type_t * type, uint8_t flags);

void * pta_spot(type_t * type);

void * pta_spot_dependent(void * destination, type_t * type);

void * pta_spot_array(type_t * type, size_t size);

void * pta_spot_array_dependent(void * destination, type_t * type, size_t size);

bool array_has_next(array_part_t * array_part, type_t * type);

size_t pta_array_length(array_part_t * array_part);

void pta_resize_array(array_part_t * array_part, size_t length);

void * pta_array_get(array_part_t * array_part, size_t index);

size_t pta_array_find(array_part_t * array_part, void * item);

size_t page_occupied_slots(void * pg_refc, type_t * type);

void * update_page_list(void * pl_obj, type_t * type, bool should_delete);

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

	refc_t pglt_refc; // page list type
	type_t pglt;

	refc_t dht_refc; // dict header type
	type_t dht;

	refc_t dbt_refc; // dict block type
	type_t dbt;

	refc_t art_refc; // array_type
	type_t art;
	
	refc_t free;
} root_page_t;

#endif