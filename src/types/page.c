/*
 * Compost instances storage features, C source
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
size_t compost_pages = 0;

// called automatically before main, to get page_size from the system cfg :
__attribute__((constructor))
void get_system_paging_config(){
	page_size = sysconf(_SC_PAGE_SIZE);
	page_rel_mask = page_size - 1; // typically 0x...00000fff
	page_mask = ~page_rel_mask;        // typically 0x...fffff000
	compute_regs_config();
}

void * spot_internal(vartype_t vartype, uint8_t flags, size_t array_bytes){
	type_t * type = strip_variant(vartype);
	page_desc_t * desc = type->page_list;
	bool array_f = type->flags & TYPE_ARRAY;
	while (true){
		if (desc == NULL){
			size_t contig_pages;
			if (array_f){
				size_t bytes = array_bytes + sizeof(array_obj_t) + sizeof(page_desc_t);
				contig_pages = CEILDIV(bytes, page_size);
			} else contig_pages = 1;

			ptr_t page = new_random_page(contig_pages);
			desc = (page_desc_t *)page.p;
			prepare_page_desc(desc, vartype, type->page_list, contig_pages, flags);
			size_t pg_limit = PG_LIMIT(desc, type);
			for (ptr_t i = page; i.s < pg_limit; i.s += page_size){
				set_page_descriptor(i, desc);
			}

			type->page_list = desc;
			compost_pages += contig_pages;
		} else {
			size_t pg_limit = PG_LIMIT(desc, type);
			if (flags == PG_FLAGS(desc)){
				for (array_obj_t * refc = PG_REFC2(desc); refc && PP(refc).s < pg_limit; ){
					if (!is_obj_referenced(refc)){
						if (refc->refc){
							// reset_fields(compost_get_c_object(refc->refc), compost_type_of(refc->refc, true));
							printf("\nCompost anomaly: is_obj_referenced() has not cleared the referenced count\n");
							raise(SIGABRT);
						}
						if (array_f) grow_array(desc, refc);
						if ((!array_f) || refc->capacity >= array_bytes){
							if (array_f) shrink_array(desc, refc, array_bytes);
							else reset_fields(compost_get_c_object(refc), type);
							return refc;
						} else if (array_f) refc->capacity = 0;
					}
					if (array_f) refc = refc->next;
					else refc = (array_obj_t *)(PP(refc).s + type->paged_size);
				}
			}
			desc = PG_NEXT(desc);
		}
	}
}

void * compost_spot(vartype_t vartype){
	return spot_internal(vartype, PAGE_BASIC, 0);
}

void * compost_spot_dependent(void * destination, vartype_t vartype){
	void ** new_spot;
	uint8_t flags = compost_get_flags(destination);
	if ((flags & FIBF_DEPENDENT) == FIBF_DEPENDENT){
		new_spot = spot_internal(vartype, PAGE_DEPENDENT, 0);
		attach_field(compost_get_obj(destination), destination, new_spot);
	} else new_spot = NULL;
	return new_spot;
}

void reset_array(array_obj_t * array_obj){
	type_t * type = compost_get_c_object(array_obj->content_type);
	for (size_t i = 0; i < array_obj->capacity; i++){
		void * c_object = compost_array_get(array_obj, i);
		reset_fields(c_object + type->offsets, type);
	}
}

void grow_array(page_desc_t * desc, array_obj_t * array_obj){
	reset_array(array_obj);
	while (array_obj->next != NULL && !is_obj_referenced(array_obj->next)){
		reset_array(array_obj->next);
		array_obj->next = array_obj->next->next;
	}
	size_t high_boundary = (array_obj->next == NULL) ? PG_RAW_LIMIT(desc) : PP(array_obj->next).s;
	// plus one means advance to content :
	array_obj->capacity = (high_boundary - PP(array_obj + 1).s);
	// here, array_obj->capacity = capacity in bytes
}

void shrink_array(page_desc_t * desc, array_obj_t * array_obj, size_t array_bytes){
	size_t high_boundary = (array_obj->next == NULL) ? PG_RAW_LIMIT(desc) : PP(array_obj->next).s;
	size_t next = PP(array_obj + 1).s + array_bytes;
	size_t excess = high_boundary - next;
	if (excess > sizeof(array_obj_t)){
		array_obj_t * new_array = (array_obj_t *)next;
		new_array->refc = NULL;
		new_array->capacity = 0;
		new_array->next = array_obj->next;
		array_obj->next = new_array;
	}
}

void * compost_spot_array_internal(type_t * type, size_t capacity, uint8_t flags){
	size_t array_bytes = capacity * (type->object_size + type->offsets);
	array_obj_t * array_obj = spot_internal((vartype_t){ .type = &get_root_page(type)->art }, flags, array_bytes);
	array_obj->content_type = compost_get_obj(type);
	array_obj->capacity = capacity;
	return (void *)array_obj;
}

void * compost_spot_array(type_t * type, size_t capacity){
	return compost_spot_array_internal(type, capacity, PAGE_BASIC);
}

void * compost_spot_array_dependent(void * destination, type_t * type, size_t capacity){
	void * new_spot;
	uint8_t flags = compost_get_flags(destination);
	if ((flags & FIBF_DEPENDENT) == FIBF_DEPENDENT){
		new_spot = compost_spot_array_internal(type, capacity, PAGE_DEPENDENT);
		attach_field(find_raw_refc(destination), destination, new_spot);
	} else new_spot = NULL;
	return new_spot;
}

/* array_get (array_obj_t pointer array_obj, 64bit index)
 * note: array_obj_t comprises the reference counter
 *
 * Finds the instance at the specified index in the specified array.
 * Return value: pointer to an instance of the array' contained type
 */
void * compost_array_get(array_obj_t * array_obj, size_t index){
	type_t * type = compost_get_c_object(array_obj->content_type);
	return ARRAY_GET(array_obj, type->object_size + type->offsets, index);
}

size_t compost_array_find(array_obj_t * array_obj, void * item){
	type_t * type = compost_get_c_object(array_obj->content_type);
	array_obj += 1; // advance to content
	return (PP(item).s - PP(array_obj).s) / (type->object_size + type->offsets);
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
		if (type->flags & TYPE_ARRAY) refc = ((array_obj_t *)refc)->next;
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
			compost_pages -= bytes / page_size;
			munmap(desc, bytes);
			desc = next_desc;
		} else {
			// first gc iteration
			// goal: detach all dependent objects

			for (array_obj_t * refc = first_instance; refc && PP(refc).s < pg_limit; ){
				bool unreferenced = !is_obj_referenced(refc);
				if (type->flags & TYPE_ARRAY){
					if (unreferenced){
						for (size_t i = 0; i < refc->capacity; i++){
							void * c_object = compost_array_get(refc, i);
							type_t * content_type = compost_get_c_object(refc->content_type);
							reset_fields(c_object + content_type->offsets, content_type);
						}
					}
					refc = refc->next;
				} else {
					if (unreferenced) reset_fields(compost_get_c_object(refc), type);
					refc = (array_obj_t *)(PP(refc).s + type->paged_size);
				}
			}
		}
	}
	return desc;
}

root_page_t * get_root_page(void * obj){
	page_desc_t * desc = get_page_descriptor(obj); // getting any type
	desc = get_page_descriptor(PG_TYPE2(desc).obj); // getting the root type
	return (root_page_t *)((size_t)(PG_TYPE2(desc).obj) & page_mask); // getting the root page
}