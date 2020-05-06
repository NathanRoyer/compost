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

/* new_page (type_t pointer type, 8-bit flags)
 *
 * Maps a new page for the paged-types. The reference counters are initialized
 * by this function, as well as the page type and flags. The type must have
 * its paged_size and page_limit setup correctly.
 * Return value: The page address, SYS_PAGE_SIZE aligned.
 */
void * new_page(type_t * type, uint8_t flags){
	page_desc_t * page = (page_desc_t *)(new_random_page(1).p);
	create_page_descriptor(PP((void *)page), page);
	pta_pages++;

	// pta_increment_refc(type);
	page->type = type;
	page->flags = flags;

	if (flags & PAGE_ARRAY){
		array_part_t * array_part = PG_REFC(page);
		*array_part = (array_part_t){ NULL, 0, 0, NULL };
		array_part->size = 0;
		array_part->following_free_space = page_size - sizeof(page_desc_t) - sizeof(array_part_t)/* - 0 */;
	}

	return page;
}

/* pta_spot (type_t pointer type, 8-bit flags)
 * note: flags is a combination of PAGE_BASIC, PAGE_DEPENDENT, PAGE_ARRAY
 * if type is NULL, page_list_type from the root page is used instead
 *
 * Runs through all spots of this type and returns the first one
 * this isn't referenced.
 * Return value: pointer to an instance of the specified type
 */
void * spot_internal(type_t * type, uint8_t flags){
	bool pglt_page = flags & PAGE_FAKE_PGLT;
	if (pglt_page) flags = PAGE_DEPENDENT;
	void ** pgl_obj_p = &type->page_list;
	page_list_t * pgl;
	while (true){
		if (*pgl_obj_p == NULL){
			if (pglt_page){
				*pgl_obj_p = PG_REFC(new_page(type, flags));
				pgl = pta_get_c_object(*pgl_obj_p);
				*pgl = (page_list_t){ NULL, *pgl_obj_p };
			} else {
				*pgl_obj_p = spot_internal(&(get_root_page(type)->pglt), PAGE_FAKE_PGLT);
				pgl = pta_get_c_object(*pgl_obj_p);
				*pgl = (page_list_t){ NULL, PG_REFC(new_page(type, flags)) };
			}
			*(void **)*pgl_obj_p = find_raw_refc(pgl_obj_p);
		} else {
			pgl = pta_get_c_object(*pgl_obj_p);
			void * pg_refc = pgl->first_instance;
			if (flags == locate_page_descriptor(pg_refc)->flags){
				for (void * refc = pg_refc; PG_REL(refc) < type->page_limit; ){
					if (!is_obj_referenced(refc)){
						reset_fields(refc, type);
						return refc;
					}
					if (flags & PAGE_ARRAY){
						if (!array_has_next(refc, type)) break;
						refc = array_next(refc, type);
					} else refc += type->paged_size;
				}
			}
			pgl_obj_p = &pgl->next;
		}
	}
}

void * pta_spot(type_t * type){
	return spot_internal(type, PAGE_BASIC);
}

void * pta_spot_dependent(void * destination, type_t * type){
	void ** new_spot;
	uint8_t flags = pta_get_flags(destination);
	if ((flags & FIBF_DEPENDENT) == FIBF_DEPENDENT){
		new_spot = spot_internal(type, PAGE_DEPENDENT);
		attach_field(pta_get_obj(destination), destination, new_spot);
	} else new_spot = NULL;
	return new_spot;
}

/* ARRAY FUNCTIONS
 * terminology note: size is for array parts and length
 * is for arrays ; length is a sum of sizes.
 */

/* array_has_next (private function)
 *
 */
bool array_has_next(array_part_t * array_part, type_t * type){
	return (void *)array_next(array_part, type) + sizeof(array_part_t) < PG_NEXT(PG_START(array_part));
}

/* grow_array (private function)
 * note: this function is not meant to be used externally
 * note: array_part_t comprises the reference counter
 *
 * Enlarges an array's size by occupying the following empty space;
 * If an unreferenced array is found, it will be incroporated.
 * Return value: none
 */
void grow_array(array_part_t * array_part, type_t * type, size_t content_size){
	array_part_t * next_part = array_part;
	while (array_has_next(next_part, type)){
		next_part = array_next(next_part, type);
		if (is_obj_referenced(next_part)) break;
		array_part->following_free_space += sizeof(array_part_t) + ARRAY_CONTENT_SIZE(next_part, type) + next_part->following_free_space;
	}
	size_t available_slots = array_part->following_free_space / content_size;
	array_part->following_free_space -= available_slots * content_size;
	array_part->size += available_slots;
}

/* shrink_array (private function)
 * note: this function is not meant to be used externally
 * note: array_part_t comprises the reference counter
 *
 * Reduces an array's size, creating a new array next to it
 * if there is enough space.
 * Return value: none
 */
void shrink_array(array_part_t * array_part, type_t * type, size_t size, size_t content_size){
	if (array_part->size <= size) return;

	size_t delta_slots = array_part->size - size;
	array_part->size -= delta_slots;

	size_t overspace = delta_slots * content_size + array_part->following_free_space;
	if (overspace > (sizeof(array_part_t) + content_size)){
		array_part->following_free_space = 0;

		size_t slots = (overspace - sizeof(array_part_t)) / content_size; // should be at least one
		size_t na_ffs = overspace - sizeof(array_part_t) - (slots * content_size);
		array_part_t * next_array = array_next(array_part, type);
		// no ref, size = slots, following free space, no next part.
		*next_array = (array_part_t){ NULL, slots, na_ffs, NULL };
	} else array_part->following_free_space += delta_slots * content_size;
}

/* pta_array_length (array pointer)
 * note: array_part_t comprises the reference counter
 *
 * Return value: none
 */
size_t pta_array_length(array_part_t * array_part){
	size_t length = 0;
	do {
		length += array_part->size;
		array_part = array_part->next_part;
	} while (array_part);
	return length;
}

/* pta_resize_array (array pointer, size)
 * note: array_part_t comprises the reference counter
 *
 * Modify an array's total size by appending or removing
 * array parts from the end of its chain.
 * Data in the array is not touched and the indexing
 * remains untouched.
 * Return value: none
 */
void pta_resize_array(array_part_t * array_part, size_t length){
	page_desc_t * page = locate_page_descriptor(array_part);
	type_t * type = page->type;
	size_t content_size = type->object_size + type->offsets;
	size_t current_length = pta_array_length(array_part);
	if (current_length > length){
		while (length > array_part->size){
			length -= array_part->size;
			array_part = array_part->next_part;
		}
		shrink_array(array_part, type, length, content_size);
		if (array_part->next_part != NULL){
			array_part->next_part->refc = NULL;
			array_part->next_part = NULL;
		}
	} else if (current_length < length){
		// locate the last part
		while (array_part->next_part){
			array_part = array_part->next_part;
		}
		grow_array(array_part, type, content_size);
		size_t expected_size = length - (current_length - array_part->size);
		if (expected_size > array_part->size){
			expected_size -= array_part->size;
			pta_spot_array_dependent(&array_part->next_part, type, expected_size);
		}
	}
}

/* pta_spot_array (type_t pointer type, 8-bit flags, 64bit size)
 * note: flags is a combination of PAGE_BASIC, PAGE_DEPENDENT
 * note: array_part_t comprises the reference counter
 *
 * Finds an array spot and adjust its size to match the size parameter.
 * Return value: pointer to an array of instances of the specified type
 */
void * spot_array_internal(type_t * type, size_t size, uint8_t flags){
	size_t content_size = type->object_size + type->offsets;
	array_part_t * tmp, * array_part = NULL, * first_part = NULL;
	flags |= PAGE_ARRAY;
	do {
		tmp = spot_internal(type, flags);
		if (first_part == NULL){
			first_part = tmp;
			first_part->refc = type->static_fields; // this is a hack plz refer to the government
			flags |= PAGE_DEPENDENT;
		} else {
			array_part->next_part = tmp;
			tmp->refc = &array_part->refc;
		}
		array_part = tmp;
		array_part->next_part = NULL;
		// hey array pls grow
		grow_array(array_part, type, content_size);
		shrink_array(array_part, type, size, content_size);
		size -= array_part->size;
	} while (size > 0);
	first_part->refc = NULL;
	return first_part;
}

void * pta_spot_array(type_t * type, size_t size){
	return spot_array_internal(type, size, PAGE_BASIC);
}

void * pta_spot_array_dependent(void * destination, type_t * type, size_t size){
	void ** new_spot;
	uint8_t flags = pta_get_flags(destination);
	if ((flags & FIBF_DEPENDENT) == FIBF_DEPENDENT){
		new_spot = spot_array_internal(type, size, PAGE_DEPENDENT);
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
	while (array_part->size <= index){
		if (array_part->next_part == NULL) return NULL;
		index -= array_part->size;
		array_part = array_part->next_part;
	}
	type_t * type = locate_page_descriptor(array_part)->type;
	return (void *)array_part + sizeof(array_part_t) + (index * (type->object_size + type->offsets));
}

size_t pta_array_find(array_part_t * array_part, void * item){
	size_t index = 0;
	type_t * type = locate_page_descriptor(array_part)->type;
	void * low_boundary;
	while (true){
		void * high_boundary = array_next(array_part, type);
		low_boundary = array_part + 1; // 1 = sizeof(array_part_t) here
		if (high_boundary > item && low_boundary <= item) break;
		if (array_part->next_part == NULL) return (size_t)-1;
		index += array_part->size;
		array_part = array_part->next_part;
	}
	return index + ((size_t)((void *)item - low_boundary) / (type->object_size + type->offsets));
}

/* page_occupied_slots (obj pointer first_instance, type_t pointer type)
 * note: this function is not meant to be used externally.
 *
 * Counts referenced instances in a page, starting at the specified address.
 * The first_instance parameter must be the result of a PG_REFC macro function.
 * Return value: the number of referenced instances in the page.
 */
size_t page_occupied_slots(void * first_instance, type_t * type){
	size_t n = 0;
	uint8_t flags = locate_page_descriptor(first_instance)->flags;
	for (void * refc = first_instance; PG_REL(refc) < type->page_limit;){
		n += is_obj_referenced(refc);
		if (flags & PAGE_ARRAY){
			if (!array_has_next(refc, type)) break;
			refc = array_next(refc, type);
		} else refc += type->paged_size;
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
void * update_page_list(void * pl_obj, type_t * type, bool should_delete){
	if (pl_obj != NULL){
		page_list_t * pl = pta_get_c_object(pl_obj);
		void * next_obj = update_page_list(pl->next, type, should_delete);
		uint8_t flags = locate_page_descriptor(pl->first_instance)->flags;

		if (should_delete && page_occupied_slots(pl->first_instance, type) == 0){
			if (type->flags & TYPE_MALLOC_F){
				size_t offset = type->paged_size - type->object_size;
				for (size_t i = 0; i < type->object_size; i++){
					field_info_b_t * fib = GET_FIB(type, i);
					if (fib->flags & (FIBF_MALLOC)){
						for (void * refc = pl->first_instance; PG_REL(refc) < type->page_limit;){
							void * mallocd_addr = *(void **)(refc + offset + i);
							if (mallocd_addr != NULL) free(mallocd_addr);
							if (flags & PAGE_ARRAY){
								if (!array_has_next(refc, type)) break;
								refc = array_next(refc, type);
							} else refc += type->paged_size;
						}
					}
				}
			}
			// second gc iteration
			// pta_decrement_refc(type);
			pta_pages--;
			munmap((void *)PG_START(pl->first_instance), page_size);
			// *find_raw_refc(pl_obj) = NULL; // reset refc
			// TODO : when pages are deleted, the child page has a bad refc !
			pl_obj = next_obj;
		} else if (flags & PAGE_DEPENDENT){
			// first gc iteration
			// goal: detach all dependent objects
			for (void * refc = pl->first_instance; PG_REL(refc) < type->page_limit;){
				if (*(void **)refc != NULL && (!is_obj_referenced(refc))) *(void **)refc = NULL;
				if (flags & PAGE_ARRAY){
					if (!array_has_next(refc, type)) break;
					refc = array_next(refc, type);
				} else refc += type->paged_size;
			}
		}
	}
	return pl_obj;
}

root_page_t * get_root_page(void * obj){
	page_desc_t * page = locate_page_descriptor(obj); // getting any type
	page = locate_page_descriptor(page->type); // getting the root type
	return (void *)((size_t)page->type & page_mask); // getting the root page
}