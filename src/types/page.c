/*
 * LibPTA instances storage features, C source
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

#include "types/page.h"

size_t page_size;
size_t page_rel_mask;
size_t page_mask;
size_t pta_pages = 0;

// called automatically before main, to get page_size from the system cfg :
__attribute__((constructor))
void get_system_paging_config(){
	page_size = sysconf(_SC_PAGE_SIZE);
	page_rel_mask = page_size - 1; // typically 0x...00000fff
	page_mask = ~page_rel_mask;        // typically 0x...fffff000
	compute_regs_config();
}

void * spot_internal(type_t * type, uint8_t flags, size_t array_capacity){
	page_desc_t * desc = type->page_list;
	bool array_f = flags & PAGE_ARRAY;
	while (true){
		if (desc == NULL){
			size_t contig_pages;
			if (array_f){
				size_t bytes = (type->object_size + type->offsets) * array_capacity;
				bytes += sizeof(array_part_t) + sizeof(page_desc_t);
				contig_pages = CEILDIV(bytes, page_size);
			} else contig_pages = 1;

			ptr_t page = new_random_page(contig_pages);
			desc = (page_desc_t *)page.p;
			prepare_page_desc(desc, type, type->page_list, contig_pages, flags);
			size_t pg_limit = PG_LIMIT(desc, type);
			for (ptr_t i = page; i.s < pg_limit; i.s += page_size){
				set_page_descriptor(i, desc);
			}

			type->page_list = desc;
			pta_pages += contig_pages;
		} else {
			size_t pg_limit = PG_LIMIT(desc, type);
			if (flags == PG_FLAGS(desc)){
				for (array_part_t * refc = PG_REFC2(desc); refc && PP(refc).s < pg_limit; ){
					if (!is_obj_referenced(refc)){
						if (array_f) grow_array(desc, refc, type);
						if ((!array_f) || refc->capacity >= array_capacity){
							if (array_f){
								shrink_array(desc, refc, type, array_capacity);
								for (size_t i = 0; i < array_capacity; i++){
									void * c_object = pta_array_get(refc, i);
									reset_fields(c_object + type->offsets, type);
								}
							} else reset_fields(pta_get_c_object(refc), type);
							return refc;
						}
					}
					if (array_f) refc = refc->next;
					else refc = (array_part_t *)(PP(refc).s + type->paged_size);
				}
			}
			desc = PG_NEXT(desc);
		}
	}
}

void * pta_spot(type_t * type){
	return spot_internal(type, PAGE_BASIC, 0);
}

void * pta_spot_dependent(void * destination, type_t * type){
	void ** new_spot;
	uint8_t flags = pta_get_flags(destination);
	if ((flags & FIBF_DEPENDENT) == FIBF_DEPENDENT){
		new_spot = spot_internal(type, PAGE_DEPENDENT, 0);
		attach_field(pta_get_obj(destination), destination, new_spot);
	} else new_spot = NULL;
	return new_spot;
}

/* grow_array (private function)
 * note: this function is not meant to be used externally
 * note: array_part_t comprises the reference counter
 *
 * Enlarges an array's size by occupying the following empty space;
 * If an unreferenced array is found, it will be incroporated.
 * Return value: none
 */
void grow_array(page_desc_t * desc, array_part_t * array_part, type_t * type){
	while (array_part->next != NULL && !is_obj_referenced(array_part->next)){
		array_part->next = array_part->next->next;
	}
	size_t high_boundary = (array_part->next == NULL) ? PG_RAW_LIMIT(desc) : PP(array_part->next).s;
	// plus one means advance to content :
	array_part->capacity = (high_boundary - PP(array_part + 1).s) / (type->object_size + type->offsets);
}

void shrink_array(page_desc_t * desc, array_part_t * array_part, type_t * type, size_t array_capacity){
	size_t high_boundary = (array_part->next == NULL) ? PG_RAW_LIMIT(desc) : PP(array_part->next).s;
	size_t content_size = type->object_size + type->offsets;
	size_t next = (PP(array_part + 1).s + array_capacity * content_size);
	size_t excess = high_boundary - next;
	if (excess > sizeof(array_part_t)){
		array_part_t * new_array = (array_part_t *)next;
		new_array->refc = NULL;
		new_array->capacity = 0;
		new_array->next = array_part->next;
		array_part->next = new_array;
	}
	array_part->capacity = array_capacity;
}

size_t pta_array_capacity(array_part_t * array_part){
	return array_part->capacity;
}

void * pta_spot_array(type_t * type, size_t capacity){
	return spot_internal(type, PAGE_ARRAY, capacity);
}

void * pta_spot_array_dependent(void * destination, type_t * type, size_t capacity){
	void ** new_spot;
	uint8_t flags = pta_get_flags(destination);
	if ((flags & FIBF_DEPENDENT) == FIBF_DEPENDENT){
		new_spot = spot_internal(type, PAGE_ARRAY | PAGE_DEPENDENT, capacity);
		attach_field(pta_get_obj(destination), destination, new_spot);
	} else new_spot = NULL;
	return new_spot;
}

/* array_get (array_part_t pointer array_part, 64bit index)
 * note: array_part_t comprises the reference counter
 *
 * Finds the instance at the specified index in the specified array.
 * Return value: pointer to an instance of the array' contained type
 */
void * pta_array_get(array_part_t * array_part, size_t index){
	type_t * type = PG_TYPE2(get_page_descriptor(array_part));
	return (void *)array_part + sizeof(array_part_t) + (index * (type->object_size + type->offsets));
}

size_t pta_array_find(array_part_t * array_part, void * item){
	type_t * type = PG_TYPE2(get_page_descriptor(array_part));
	array_part += 1; // advance to content
	return (PP(item).s - PP(array_part).s) / (type->object_size + type->offsets);
}

/* page_occupied_slots (obj pointer first_instance, type_t pointer type)
 * note: this function is not meant to be used externally.
 *
 * Counts referenced instances in a page, starting at the specified address.
 * The first_instance parameter must be the result of a PG_REFC macro function.
 * Return value: the number of referenced instances in the page.
 */
size_t page_occupied_slots(size_t pg_limit, uint8_t flags, void * first_instance, type_t * type){
	size_t n = 0;
	for (void * refc = first_instance; refc && PP(refc).s < pg_limit;){
		n += is_obj_referenced(refc);
		if (flags & PAGE_ARRAY) refc = ((array_part_t *)refc)->next;
		else refc += type->paged_size;
	}
	return n;
}

/* update_page_list (page_list object pointer pl_obj, type_t pointer type, boolean should_delete)
 * note: this function is not meant to be used externally.
 * note: is called two times by the gc, only the second time pages are deleted
 *
 * This function unmaps unused pages. If a page is to be unmapped, it will
 * remove references to values in the page first.
 * Return value: this page-list block if it was not empty, or the next block
 * if it was empty; this way the list is updated from the last block to the
 * first.
 */
page_desc_t * update_page_list(page_desc_t * desc, type_t * type, bool should_delete){
	if (desc != NULL){
		page_desc_t * next_desc = update_page_list(PG_NEXT(desc), type, should_delete);
		desc->next.p = (ptr_t *)next_desc;
		void * first_instance = PG_REFC2(desc);
		size_t pg_limit = PG_LIMIT(desc, type);
		uint8_t flags = PG_FLAGS(desc);

		if (should_delete && page_occupied_slots(pg_limit, flags, first_instance, type) == 0){
			size_t bytes = PG_RAW_LIMIT(desc) - PP(desc).s;
			pta_pages -= bytes / page_size;
			munmap(desc, bytes);
			desc = next_desc;
		} else {
			// first gc iteration
			// goal: detach all dependent objects

			for (array_part_t * refc = first_instance; refc && PP(refc).s < pg_limit; ){
				bool unreferenced = !is_obj_referenced(refc);
				if (flags & PAGE_ARRAY){
					if (unreferenced){
						for (size_t i = 0; i < refc->capacity; i++){
							void * c_object = pta_array_get(refc, i);
							reset_fields(c_object + type->offsets, type);
						}
					}
					refc = refc->next;
				} else {
					if (unreferenced) reset_fields(pta_get_c_object(refc), type);
					refc = (array_part_t *)(PP(refc).s + type->paged_size);
				}
			}
		}
	}
	return desc;
}

root_page_t * get_root_page(void * obj){
	page_desc_t * desc = get_page_descriptor(obj); // getting any type
	desc = get_page_descriptor(PG_TYPE2(desc)); // getting the root type
	return (root_page_t *)((size_t)PG_TYPE2(desc) & page_mask); // getting the root page
}