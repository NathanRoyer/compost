/*
 * LibPTA types manipulations features, C source
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

#include "types/field.h"

typedef struct obj_info {
	size_t offset;
	type_t * page_type;
	uint8_t page_flags;
} obj_info_t;

obj_info_t get_info(void * obj){
	page_header_t * page = PG_START(obj);
	size_t offset;
	if (PG_FLAG(page, PAGE_ARRAY)){
		array_part_t * array_part = PG_REFC(page), * next_ap;
		while (true){
			next_ap = array_next(array_part, page->type);
			if (obj < (void *)next_ap) break;
			else array_part = next_ap;
		}

		if (obj < (void *)array_part + sizeof(array_part_t)){
			return (obj_info_t){
				(size_t)obj - (size_t)array_part,
				&(get_root_page(page->type)->art), 
				page->flags
			};
		} else {
			offset = obj - ((void *)array_part + sizeof(array_part_t));
			offset %= page->type->object_size + page->type->offsets;
		}
	} else offset = (PG_REL(obj - sizeof(page_header_t))) % page->type->paged_size;
	return (obj_info_t){ offset, page->type, page->flags };
}

/* type_of (pointer obj)
 *
 * This function tells the type of a paged object.
 * How it works:
 * It first finds the base address of the object, then it searches the
 * offset from this base in the field_infos_* arrays of the type.
 * Return value: a type pointer (as a C object, not as a paged object)
 */
type_t * pta_type_of(void * obj){
	type_t * field_type;
	obj_info_t info = get_info(obj);

	size_t offsets_zone;
	if ((info.page_flags & PAGE_ARRAY) && !(info.page_type->flags & TYPE_ARRAY)) offsets_zone = info.page_type->offsets;
	else offsets_zone = GET_OFFSET_ZONE(info.page_type);

	if (info.offset == 0) field_type = info.page_type;
	else if (info.offset < offsets_zone){
		do field_type = GET_FIA(info.page_type, info.offset--)->field_type;
		while (field_type == GO_BACK);
	} else {
		info.offset -= offsets_zone;
		field_info_b_t * fib;
		do {
			fib = GET_FIB(info.page_type, info.offset--);
			field_type = fib->field_type;
		} while (field_type == GO_BACK);
		if (fib->flags & FIBF_ARRAY){
			field_type = &(get_root_page(obj)->art);
		}
	}
	return field_type;
}

/* get_c_object (pointer obj)
 *
 * This function shifts an object to the beginning of the
 * C-compatible structure.
 * Return value: a pointer to obj's C structure.
 */
void * pta_get_c_object(void * obj){
	if (obj != NULL){
		obj_info_t info = get_info(obj);
		size_t offsets_zone;
		if ((info.page_flags & PAGE_ARRAY) && !(info.page_type->flags & TYPE_ARRAY)) offsets_zone = info.page_type->offsets;	
		else offsets_zone = GET_OFFSET_ZONE(info.page_type);
		
		if (info.offset < offsets_zone) obj += offsets_zone - info.offset;
	}
	return obj;
}


/* prepare (context ctx, pointer obj, type_t pointer type)
 *
 * This function acts as a generic constructor for objects.
 * It runs through all the fields of the instance's type
 * and fills them with appropriate data, instanciating distant
 * fields if required.
 * Return value: The construct object
 */
void zero(void * addr, size_t sz, char value){
	for (size_t i = 0; i < sz; i++) *(char *)(addr + i) = value;
}
void * pta_prepare(void * obj, type_t * type){
	if (type == NULL) type = pta_type_of(obj);
	void * obj_c = pta_get_c_object(obj);
	if (obj_c == obj) pta_increment_refc(obj);

	if (type->flags & TYPE_PRIMITIVE){
		zero(obj_c, type->object_size, '\x00');
	} else {
		array str = { -1, NULL };
		do {
			pta_dict_get_next_index(type->dynamic_fields, &str);
			if (str.length > 0){
				void * field = pta_get_field(obj, str);
				if (field){
					type_t * field_type = pta_type_of(field);
					if (field_type != NULL){
						size_t should_zero = 0;

						uint8_t field_flags = pta_get_flags(field);
						if ((field_flags & FIBF_AUTO_INST)){
							if (field_flags & FIBF_POINTER){
								if (field_flags & FIBF_DEPENDENT){
									void * new_field = pta_spot_dependent(field, field_type);
									pta_prepare(new_field, (field_flags & FIBF_ARRAY) ? NULL : field_type);
								} else should_zero = sizeof(void *); // independent pointer
							} else pta_prepare(field, field_type); // nested
						} else should_zero = field_flags & FIBF_POINTER ? sizeof(void *) : field_type->object_size; // do not auto instantiate

						zero(field, should_zero, '\x00');
					}
				}
			}
		} while (str.length > 0);
	}

	if (obj_c == obj) pta_decrement_refc(obj);

	return obj;
}

void misbound_error(){
	printf("\nLibPTA anomaly: misbound instance\n");
}

void * detach_field(void * host, void * field){
	void * dependent = *(void **)field;
	if (dependent != NULL){
		void ** distant_refc = find_raw_refc(dependent);
		if (*distant_refc == host) *distant_refc = NULL;
		else misbound_error();
		*(void **)field = NULL;
	}
	return dependent;
}

void * attach_field(void * host, void * field, void * dependent){
	void * bck = detach_field(host, field);
	void ** distant_refc = find_raw_refc(dependent);
	if ((*distant_refc != NULL) && (*distant_refc != FAKE_DEPENDENT(distant_refc))) misbound_error();
	*distant_refc = host;
	*(void **)field = dependent;
	return bck;
}

void * pta_detach_dependent(void * field){
	void * host = pta_get_obj(field);
	uint8_t flags = pta_get_flags(field);
	return ((flags & FIBF_POINTER) && (flags & FIBF_DEPENDENT)) ? detach_field(host, field) : NULL;
}

void * pta_attach_dependent(void * field, void * dependent_obj){
	void * host = pta_get_obj(field);
	uint8_t flags = pta_get_flags(field);
	return ((flags & FIBF_POINTER) && (flags & FIBF_DEPENDENT)) ? attach_field(host, field, dependent_obj) : NULL;
}

void reset_dependent_fields(void * refc, type_t * type){
	size_t offset = type->paged_size - type->object_size;
	for (size_t i = 0; i < type->object_size; i++){
		field_info_b_t * fib = GET_FIB(type, i);
		void * field = refc + (offset + i);
		if ((fib->flags & FIBF_POINTER) && (fib->flags & FIBF_DEPENDENT)) detach_field(refc, field);
		else if (fib->flags & (FIBF_MALLOC) && (*(void **)field != NULL)){
			free(*(void **)field);
			*(void **)field = NULL;
		}
	}
}

/* pta_create_type (object pointer any_paged_obj, 64bit nested_objects, 64bit object_size, 8bit flags)
 * note: the nested_object parameter must perfectly precise
 * note: object_size is the sum of the fields sizes
 * note: any_paged_obj is used to retrieve the context
 *
 * This function is the main way of creating a type. It spots the field_infos arrays,
 * and the fields dictionnaries.
 * Return value: The created type object
 */
void * pta_create_type(void * any_paged_obj, size_t nested_objects, size_t object_size, uint8_t flags){
	size_t offsets = 1 + nested_objects; // add self offset
	root_page_t * rp = get_root_page(any_paged_obj);

	void * new_type_refc = pta_spot(&rp->rt);
	pta_increment_refc(new_type_refc);
	type_t * new_type = pta_get_c_object(new_type_refc);

	new_type->object_size = object_size;
	new_type->offsets = offsets;
	new_type->page_list = NULL;
	new_type->client_data = NULL;
	new_type->flags = flags;

	void * dyn_f = pta_spot_dependent(&new_type->dynamic_fields, &rp->dht);
	void * stat_f = pta_spot_dependent(&new_type->static_fields, &rp->dht);
	pta_spot_array_dependent(&new_type->dfia, &rp->fiat, offsets);
	pta_spot_array_dependent(&new_type->dfib, &rp->fibt, object_size);

	*(dict_t *)pta_get_c_object(dyn_f) = (dict_t){ NULL, NULL };
	*(dict_t *)pta_get_c_object(stat_f) = (dict_t){ NULL, NULL };

	new_type->paged_size = compute_paged_size(new_type);
	new_type->page_limit = compute_page_limit(new_type);

	for (size_t i = 0; i < offsets; i++){
		*GET_FIA(new_type, i) = (field_info_a_t){ UNDEFINED, 0 };
	}

	for (size_t i = 0; i < object_size; i++){
		*GET_FIB(new_type, i) = (field_info_b_t){ UNDEFINED, TYPE_BASIC };
	}

	pta_decrement_refc(new_type_refc);
	return new_type_refc;
}

/* find_and_fill_fia (private function)
 *
 * This function fills the first undefined fia with the provided fib_offset.
 * Return value: the freshly defined fia offset
 */
size_t find_and_fill_fia(type_t * host_type, type_t * field_type, size_t fib_offset){
	for (size_t i = 1; i < host_type->offsets; i++){
		field_info_a_t * fia = GET_FIA(host_type, i);
		if (fia->field_type == UNDEFINED){
			*fia = (field_info_a_t){ field_type, fib_offset };
			return i;
		}
	}
	// the next line will segfault to prevent further execution.
	// this is thrown if there is no more room for nested objects in this type spec.
	*(char *)NULL = '\0';
	// just to make compilers happy :
	return 0;
}

/* pta_set_dynamic_field (type_t pointer type, type_t pointer field_type, 64bit offset, 8bit flags)
 * note: flags must be a combination of FIBF_BASIC, FIBF_POINTER, FIBF_DEPENDENT, FIBF_ARRAY and FIBF_AUTO_INST.
 * note: offset must be inferior to the type's object_size field.
 *
 * This function is the main way of adding a field to a type. It handles
 * nested objects by creating an appropriate entry in the field_info_a array.
 * It also adds the field into the dynamic_field dictionnary of the type.
 * It has a different method for pointers.
 * Return value: the size of the created field (sizeof(void *) for pointers)
 */
size_t pta_set_dynamic_field(type_t * host_type, type_t * field_type, array field_name, size_t fib_offset, uint8_t flags){
	bool nested = !(field_type->flags & TYPE_PRIMITIVE) && !(flags & FIBF_POINTER);

	size_t field_size, offset_from_refc;

	if (nested){
		field_size = field_type->object_size;

		offset_from_refc = find_and_fill_fia(host_type, field_type, fib_offset);
		if (field_type->offsets > 1){
			for (size_t i = 1; i < field_type->offsets; i++){
				field_info_a_t * fia = GET_FIA(field_type, i);
				find_and_fill_fia(host_type, fia->field_type, fia->data_offset + fib_offset);
			}
		}

		for (size_t i = 0; i < field_size; i++){
			*GET_FIB(host_type, fib_offset + i) = *GET_FIB(field_type, i);
		}
	} else {
		field_size = (flags & FIBF_POINTER) ? sizeof(void *) : field_type->object_size;
		offset_from_refc = host_type->offsets + fib_offset;
		for (size_t i = 0; i < field_size; i++){
			field_info_b_t * fib = GET_FIB(host_type, fib_offset + i);
			if (i == 0) *fib = (field_info_b_t){ field_type, flags };
			else *fib = (field_info_b_t){ GO_BACK, FIBF_BASIC };
		}
	}
	pta_dict_set(host_type->dynamic_fields, field_name, (void *)offset_from_refc);
	return field_size;
}

/* pta_get_flags (object pointer obj)
 *
 * This function fetches the flags of an object if it is the field
 * of another object.
 * Return value: 8bit FIBF_* combination
 */
uint8_t pta_get_flags(void * obj){
	obj_info_t info = get_info(obj);
	uint8_t result;

	if (info.offset > info.page_type->offsets){
		field_info_b_t * fib = GET_FIB(info.page_type, info.offset - GET_OFFSET_ZONE(info.page_type));
		result = fib->flags;
	} else result = FIBF_NESTED | FIBF_AUTO_INST;

	return result;
}

/* pta_get_field (object pointer obj, string field_name)
 *
 * This function gives a field's address given an object address
 * and a field name. It took *three days* to get it to work.
 * Return value: the field's address
 */
void * pta_get_field(void * obj, array field_name){
	if (pta_is_pointer(obj)){
		obj = *(void **)obj;
		if (!obj) return NULL;
	}
	obj_info_t info = get_info(obj);
	type_t * field_type = pta_type_of(obj);
	size_t new_offset = (size_t)pta_dict_get(field_type->dynamic_fields, field_name);

	if (new_offset >= field_type->offsets){
		new_offset -= field_type->offsets;
		new_offset += GET_OFFSET_ZONE(info.page_type);
		if (info.offset > 0 && info.offset < info.page_type->offsets){
			new_offset += GET_FIA(info.page_type, info.offset)->data_offset;
		}
	} else new_offset += info.offset;

	return ((void *)find_raw_refc(obj) + new_offset);
}

