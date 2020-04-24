/*
 * LibPTA garbage collection features, C source
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
	page_header_t * page = PG_START(address);
	if (PG_FLAG(page, PAGE_ARRAY)){
		array_part_t * array_part = PG_REFC(page), * next_ap;
		while (true){
			next_ap = array_next(array_part, page->type);
			if (address < (void *)next_ap) break;
			else array_part = next_ap;
		}
		return (void **)&array_part->refc;
	} else return address - ((PG_REL(address) - sizeof(page_header_t)) % page->type->paged_size);
}

void * pta_get_final_obj(void * address){
	// void * bckdbg = address;
	address = find_raw_refc(address);
	while (PG_FLAG(PG_START(address), PAGE_DEPENDENT) && *(void **)address != NULL && *(void **)address != address) address = *(void **)address;
	return address;
}

void ** find_refc(void * address, recursive_call_t * rec){
	void ** indep_refc = pta_get_final_obj(address);
	if (*indep_refc != indep_refc) check_references(indep_refc, rec);
	return indep_refc;
}

bool pta_protect(void * obj){
	void ** refc = find_refc(obj, NULL);
	bool result = (*refc == NULL);
	if (result) *refc = FAKE_DEPENDENT(refc);
	return result;
}

void pta_unprotect(void * obj){
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
int pta_type_instances(type_t * type){
	int n = 0;
	for (page_list_t * pl = pta_get_c_object(type->page_list); pl; pl = pta_get_c_object(pl->next)){
		n += page_occupied_slots(pl->first_instance, type);
	}
	return n;
}

/* remove_superfluous_pages (type_t pointer type)
 * note: this function is indirectly responsible for page unmaps.
 *
 * Updates the page-list of a type, effectively removing
 * the empty pages (i.e. containing no referenced instances).
 * It then unsets the TYPE_HAS_UNREF flag.
 * Return value: none
 */
void pta_remove_superfluous_pages(type_t * type, bool should_delete){
	type->page_list = update_page_list(type->page_list, type, should_delete);
	// if (should_delete) type->flags &= ~TYPE_HAS_UNREF;
}

/* garbage_collect (root type pointer root_type)
 * note: this function is indirectly responsible for page unmaps.
 *
 * Runs through all types registered in the root types page,
 * and calls remove_superfluous_pages on them if they have the TYPE_HAS_UNREF flag.
 * Return value: none
 */
void pta_garbage_collect(type_t * root_type){
	/*
	 * TODO : pre-run to subtract independent-references that
	 * are unreferenced themselves
	 */
	page_list_t * pgl, * pgl_bck = pta_get_c_object(root_type->page_list);
	for (size_t i = 0; i < 2; i++){
		pgl = pgl_bck;
		while (pgl){
			for (void * refc = pgl->first_instance; PG_REL(refc) < root_type->page_limit; refc += root_type->paged_size){
				if (*(void **)refc == NULL) continue;
				pta_remove_superfluous_pages(pta_get_c_object(refc), i > 0);
			}
			pgl = pta_get_c_object(pgl->next);
		}
	}
}