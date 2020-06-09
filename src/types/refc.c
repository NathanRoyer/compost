/*
 * Compost garbage collection features, C source
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

#include "types/refc.h"

void ** find_raw_refc(void * address){
	page_desc_t * desc = get_page_descriptor(address);
	if (PG_TYPE2(desc)->flags & TYPE_ARRAY){
		array_obj_t * array_obj = PG_REFC2(desc), * next_ap;
		while ((next_ap = array_obj->next) != NULL){
			if (address < (void *)next_ap) break;
			else array_obj = next_ap;
		}
		return &array_obj->refc;
	} else return address - ((PP(address).s - PP(desc + 1).s) % PG_TYPE2(desc)->paged_size);
}

void * compost_get_final_obj(void * address){
	// void * bckdbg = address;
	address = find_raw_refc(address);
	while ((PG_FLAGS(get_page_descriptor(address)) & PAGE_DEPENDENT) && *(void **)address != NULL && *(void **)address != address){
		address = *(void **)address;
	}
	return address;
}

void ** find_refc(void * address, recursive_call_t * rec){
	void ** indep_refc = compost_get_final_obj(address);
	if (*indep_refc != indep_refc) check_references(indep_refc, rec);
	return indep_refc;
}

bool compost_protect(void * obj){
	void ** refc = find_refc(obj, NULL);
	bool result = (*refc == NULL);
	if (result) *refc = FAKE_DEPENDENT(refc);
	return result;
}

bool is_obj_protected(void * obj){
	void ** refc = find_refc(obj, NULL);
	return *refc == FAKE_DEPENDENT(refc);
}

void compost_unprotect(void * obj){
	void ** refc = find_refc(obj, NULL);
	if (*refc == FAKE_DEPENDENT(refc)) *refc = NULL;
}

bool is_obj_referenced(void * obj){
	return *find_refc(obj, NULL) != NULL;
}

/* type_instances (type_t pointer type)
 * note: this function is never used internally, but page_occupied_slots is.
 *
 * Counts all instances of a type accross all its pages.
 * Return value: the total number of instances of the specified type.
 */
int compost_type_instances(type_t * type){
	int n = 0;
	for (page_desc_t * desc = type->page_list; desc; desc = PG_NEXT(desc)){
		n += page_occupied_slots(PG_LIMIT(desc, type), PG_FLAGS(desc), PG_REFC2(desc), type);
	}
	return n;
}

/* remove_superfluous_pages (type_t pointer type)
 * note: this function is indirectly responsible for page unmaps.
 *
 * Updates the page-list of a type, effectively removing
 * the empty pages (i.e. containing no referenced instances).
 * Return value: none
 */
void compost_remove_superfluous_pages(type_t * type, bool should_delete){
	type->page_list = update_page_list(type->page_list, type, should_delete);
}

/* garbage_collect (root type pointer root_type)
 * note: this function is indirectly responsible for page unmaps.
 *
 * Runs through all types registered in the root types page,
 * and calls remove_superfluous_pages on them if they have the TYPE_HAS_UNREF flag.
 * Return value: none
 */
void compost_garbage_collect(type_t * root_type){
	/*
	 * TODO : pre-run to subtract independent-references that
	 * are unreferenced themselves
	 */
	page_desc_t * desc, * desc_bck = root_type->page_list;
	for (size_t i = 0; i < 2; i++){
		desc = desc_bck;
		while (desc){
			size_t pg_limit = PG_LIMIT(desc, root_type);
			for (void * refc = PG_REFC2(desc); PP(refc).s < pg_limit; refc += root_type->paged_size){
				if (*(void **)refc == NULL) continue;
				compost_remove_superfluous_pages(compost_get_c_object(refc), i > 0);
			}
			desc = PG_NEXT(desc);
		}
	}
}