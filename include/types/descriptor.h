/*
 * LibPTA pages descriptors, C header
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

#ifndef TYPES_DESCRIPTOR_H
#define TYPES_DESCRIPTOR_H

#include "type.h"

typedef PTA_STRUCT page_desc page_desc_t;

typedef union ptr ptr_t;
typedef union ptr {
	ptr_t * p;
	size_t s;
} ptr_t;
#define SP(v) ((ptr_t){ .s = (v) })
#define PP(v) ((ptr_t){ .p = (v) })
#define PTR_BITS (sizeof(void *) * 8)

size_t reg_mask;
ptr_t first_reg;

size_t reg_i_mask;
size_t reg_i_bits;

size_t reg_md_mask;
size_t reg_md_bits;

size_t reg_part_bits;
size_t reg_last_part_bits;

typedef PTA_STRUCT page_desc {
	type_t * type;
	uint8_t flags;
} page_desc_t;

void compute_regs_config();

ptr_t new_random_page(size_t contig_len);

ptr_t get_reg_metadata(ptr_t reg);

void set_reg_metadata(ptr_t reg, ptr_t metadata);

page_desc_t * locate_page_descriptor(void * address);

void create_page_descriptor(ptr_t address, page_desc_t * desc);

#endif