/*
 * Compost types manipulations features, C source
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

obj_info_t get_info(void * obj){
	page_desc_t * desc = get_page_descriptor(obj);
	vartype_t vartype = PG_TYPE2(desc);
	type_t * type = strip_variant(vartype);
	obj_info_t info;
	if (type->flags & TYPE_ARRAY){
		array_obj_t * array_obj = PG_REFC2(desc), * next_ap;
		while ((next_ap = array_obj->next) != NULL){
			if (obj < (void *)next_ap) break;
			else array_obj = next_ap;
		}

		if (obj < (void *)array_obj + sizeof(array_obj_t)){
			info.offsets_zone = GET_OFFSET_ZONE(type);
			info.offset = (size_t)obj - (size_t)array_obj;
		} else {
			type = compost_get_c_object(array_obj->content_type);
			vartype.type = type;
			info.offsets_zone = type->offsets;
			info.offset = obj - ((void *)array_obj + sizeof(array_obj_t));
			info.offset %= type->object_size + type->offsets;
		}
	} else {
		info.offset = (PP(obj).s - PP(desc + 1).s) % type->paged_size;
		info.offsets_zone = GET_OFFSET_ZONE(type);
	}
	info.page_type = type;
	info.page_vartype = vartype;
	return info;
}

void * compost_get_obj(void * address){
	return address - get_info(address).offset;
}

type_t * strip_variant(vartype_t vartype){
	// is this assumption dangerous ?
	bool is_variant = PG_TYPE2(get_page_descriptor(vartype.obj)).type->flags & TYPE_ARRAY;
	return is_variant ? *(type_t **)(vartype.variant + 1) : vartype.type;
}

void * compost_create_type_variant(type_t * base_type, constraint_t * constraints, size_t len){
	void ** destination = &base_type->variants;
	while (*destination != NULL){
		destination = ARRAY_GET(*destination, sizeof(void *), 1);
	}
	type_t * szt = &get_root_page(base_type)->szt;
	array_obj_t * new_var = compost_spot_array_dependent(destination, szt, (len + 1) * 2);
	constraint_t * slots = ARRAY_GET(new_var, sizeof(void *), 0);
	slots[0] = (constraint_t){ (size_t)base_type, (size_t)NULL };
	for (size_t i = 0; i < len; i++){
		slots[i + 1] = constraints[i];
	}
	return (void *)new_var;
}

bool compost_type_mismatch(vartype_t vartype, void * obj){
	type_t * base_type = strip_variant(vartype);
	bool match = compost_get_obj(base_type) == compost_get_obj(compost_type_of(obj));
	if (match && base_type != vartype.type){
		constraint_t * constraints = (constraint_t *)(vartype.variant + 1);
		obj_info_t info = get_info(obj);
		for (size_t i = 1, j = 2; j < vartype.variant->capacity && match; j += 2){
			size_t * location = (size_t *)advance_obj_ptr(obj, info, constraints[i].field_offset, true);
			match = constraints[i].value == *location;
		}
	}
	return !match;
}

vartype_t compost_vartype_of(void * obj){
	vartype_t vartype;
	obj_info_t info = get_info(obj);

	if (info.offset == 0) vartype = info.page_vartype;
	else if (info.offset < info.offsets_zone){
		do vartype.type = GET_FIA(info.page_type, info.offset--)->field_type;
		while (vartype.type == GO_BACK);
	} else {
		info.offset -= info.offsets_zone;
		field_info_b_t * fib;
		do {
			fib = GET_FIB(info.page_type, info.offset--);
			vartype = fib->field_vartype;
		} while (vartype.type == GO_BACK);
	}
	return vartype;
}

type_t * compost_type_of(void * obj){
	return strip_variant(compost_vartype_of(obj));
}

/* get_c_object (pointer obj)
 *
 * This function shifts an object to the beginning of the
 * C-compatible structure.
 * Return value: a pointer to obj's C structure.
 */
void * compost_get_c_object(void * obj){
	if (obj != NULL){
		obj_info_t info = get_info(obj);
		if (info.offset < info.offsets_zone) obj += info.offsets_zone - info.offset;
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
void * compost_prepare(void * obj, type_t * type){
	if (type == NULL) type = compost_type_of(obj);
	void * obj_c = compost_get_c_object(obj);
	bool unprotect = compost_protect(obj);

	if (type->flags & TYPE_PRIMITIVE){
		zero(obj_c, type->object_size, '\x00');
	} else {
		array str = { -1, NULL };
		do {
			compost_dict_get_next_index(type->dynamic_fields, &str);
			if (str.data != NULL){
				// compost_print_cstr(str);
				void * field = compost_get_field(obj, str.length, str.data, true);
				// printf(" (%p) is being prepared.\n", field);
				if (field){
					vartype_t field_vartype = compost_vartype_of(field);
					type_t * field_type = strip_variant(field_vartype);
					if (field_vartype.type != NULL){
						size_t should_zero = 0;

						uint8_t field_flags = compost_get_flags(field);
						if ((field_flags & FIBF_AUTO_INST)){
							if ((field_flags & FIBF_DEPENDENT) == FIBF_DEPENDENT){
								void * new_field = compost_spot_dependent(field, field_vartype);
								compost_prepare(new_field, field_type);
							} else if (field_flags & FIBF_POINTER){
								should_zero = sizeof(void *); // independent pointer
							} else compost_prepare(field, field_type); // nested
						} else should_zero = (field_flags & FIBF_POINTER) ? sizeof(void *) : field_type->object_size; // do not auto instantiate

						zero(field, should_zero, '\x00');
					}
				}
			}
		} while (str.data != NULL);
	}

	if (unprotect) compost_unprotect(obj);

	return obj;
}

void misbound_error(){
	printf("\nCompost anomaly: misbound instance\n");
	raise(SIGABRT);
}

void * detach_field(void * raw_refc, void * field){
	void * dependent = *(void **)field;
	if (dependent != NULL){
		void ** distant_refc = find_raw_refc(dependent);
		if (*distant_refc == raw_refc) *distant_refc = NULL;
		else misbound_error();
		*(void **)field = NULL;
	}
	return dependent;
}

void * attach_field(void * raw_refc, void * field, void * dependent){
	void * bck = detach_field(raw_refc, field);
	void ** distant_refc = find_raw_refc(dependent);
	if ((*distant_refc != NULL) && (*distant_refc != FAKE_DEPENDENT(distant_refc))) misbound_error();
	*distant_refc = raw_refc;
	*(void **)field = dependent;
	return bck;
}

void * compost_detach_dependent(void * field){
	void * raw_refc = find_raw_refc(field);
	uint8_t flags = compost_get_flags(field);
	return ((flags & FIBF_DEPENDENT) == FIBF_DEPENDENT) ? detach_field(raw_refc, field) : NULL;
}

void * compost_attach_dependent(void * field, void * dependent_obj){
	void * raw_refc = find_raw_refc(field);
	uint8_t flags = compost_get_flags(field);
	return ((flags & FIBF_DEPENDENT) == FIBF_DEPENDENT) ? attach_field(raw_refc, field, dependent_obj) : NULL;
}

void ** get_previous_owner(void * ref_field){
	obj_info_t info = get_info(ref_field);
	info.offset -= info.offsets_zone;
	size_t prev_owner_i = info.page_type->object_size;
	field_info_b_t * fib;
	do {
		prev_owner_i -= sizeof(void *);
		fib = GET_FIB(info.page_type, prev_owner_i);
		if ((fib->flags & FIBF_PREV_OWNER) == 0) return NULL;
	} while (fib->field_vartype.obj != (void *)info.offset);
	return ref_field + prev_owner_i - info.offset;
}
/*
 * WARNING
 * protected objects should never be unprotected
 * after fields are set as referencing to them !
 */
void compost_set_reference(void * field, void * obj){
	uint8_t flags = compost_get_flags(field);
	if ((flags & FIBF_REFERENCES) == FIBF_REFERENCES){
		compost_clear_reference(field);
		if (obj != NULL){
			void ** refc = compost_get_final_obj(obj);
			if (!is_obj_protected(refc)){
				if (*refc != NULL) *get_previous_owner(field) = *refc;
				*refc = field;
			}
		}
	} else misbound_error(); // maybe tmp but i wanna know if it happens
	*(void **)field = obj;
}

void compost_clear_reference(void * field){
	uint8_t flags = compost_get_flags(field);
	if ((flags & FIBF_REFERENCES) == FIBF_REFERENCES && *(void **)field != NULL){
		void ** refc = compost_get_final_obj(*(void **)field);
		if (!is_obj_protected(refc)){
			while (*refc != field && *refc != NULL){
				refc = get_previous_owner(*refc);
			}
			*refc = *get_previous_owner(field);
		}
	}
	*(void **)field = NULL;
}

void check_references(void ** refc, recursive_call_t * rec){
	// check for infinite recursive calls:
	recursive_call_t next_rec = { refc, rec };
	while (rec){
		if (rec->arg == refc) return;
		else rec = rec->next;
	}
	while (*refc != NULL){
		void ** prev_owner = get_previous_owner(*refc);
		if (*find_refc(*refc, &next_rec) == NULL){
			// detach
			*(void **)*refc = NULL;
			*refc = *prev_owner;
		} else refc = prev_owner;
	}
}

void reset_fields(void * c_object, type_t * type){
	for (size_t i = 0; i < type->object_size; i++){
		uint8_t flags = GET_FIB(type, i)->flags;
		void * field = c_object + i;
		if ((flags & FIBF_DEPENDENT) == FIBF_DEPENDENT){
			detach_field(find_raw_refc(c_object), field);
		} else if ((flags & FIBF_MALLOC) && (*(void **)field != NULL)){
			free(*(void **)field);
		} else if ((flags & FIBF_REFERENCES) && (*(void **)field != NULL)){
			compost_clear_reference(field);
		}
		*(char *)field = '\0';
	}
}

/* compost_create_type (object pointer any_paged_obj, 64bit nested_objects, 64bit object_size, 8bit flags)
 * note: the nested_object parameter must perfectly precise
 * note: object_size is the sum of the fields sizes
 * note: any_paged_obj is used to retrieve the context
 *
 * This function is the main way of creating a type. It spots the field_infos arrays,
 * and the fields dictionnaries.
 * Return value: The created type object
 */
void * compost_create_type(void * any_paged_obj, size_t nested_objects, size_t referencers, size_t object_size, uint8_t flags){
	size_t offsets = 1 + nested_objects; // add self offset
	object_size += referencers * PTRSZ;
	root_page_t * rp = get_root_page(any_paged_obj);

	void * new_type_refc = compost_spot((vartype_t){ &rp->rt });
	bool unprotect = compost_protect(new_type_refc);
	type_t * new_type = compost_get_c_object(new_type_refc);

	compost_prepare(new_type_refc, &rp->rt);

	new_type->object_size = object_size;
	new_type->offsets = offsets;
	new_type->page_list = NULL;
	new_type->flags = flags;

	void * dyn_f = compost_spot_dependent(&new_type->dynamic_fields, (vartype_t){ &rp->dht });
	void * stat_f = compost_spot_dependent(&new_type->static_fields, (vartype_t){ &rp->dht });
	compost_spot_array_dependent(&new_type->dfia, &rp->fiat, offsets);
	compost_spot_array_dependent(&new_type->dfib, &rp->fibt, object_size);

	*(dict_t *)compost_get_c_object(dyn_f) = (dict_t){ NULL, NULL };
	*(dict_t *)compost_get_c_object(stat_f) = (dict_t){ NULL, NULL };

	new_type->paged_size = compute_paged_size(new_type);
	new_type->variants = NULL;

	for (size_t i = 0; i < offsets; i++){
		*GET_FIA(new_type, i) = (field_info_a_t){ NULL, 0 };
	}

	for (size_t i = object_size - 1;; i--){
		field_info_b_t * fib = GET_FIB(new_type, i);
		if (((object_size - i) % sizeof(void *)) == 0){
			*fib = (field_info_b_t){ { .obj = (void *)i }, FIBF_PREV_OWNER };
		} else {
			*fib = (field_info_b_t){ { .type = GO_BACK }, FIBF_BASIC };
		}
		if (i == 0) break;
	}

	if (unprotect) compost_unprotect(new_type_refc);
	return new_type_refc;
}

/* find_and_fill_fia (private function)
 *
 * This function fills the first undefined fia with the provided fib_offset.
 * Return value: the freshly defined fia offset
 */
void * find_and_fill_fia(type_t * host_type, type_t * field_type, size_t fib_offset){
	for (size_t i = 1; i < host_type->offsets; i++){
		field_info_a_t * fia = GET_FIA(host_type, i);
		if (fia->field_type == NULL){
			*fia = (field_info_a_t){ field_type, fib_offset };
			return fia;
		}
	}
	// the next line will segfault to prevent further execution.
	// this is thrown if there is no more room for nested objects in this type spec.
	raise(SIGABRT);
	return NULL; // just to make all compilers happy
}

void find_and_fill_prev_owner(type_t * host_type, size_t fib_offset){
	size_t prev_owner_i = host_type->object_size;
	do {
		prev_owner_i -= sizeof(void *);
		field_info_b_t * fib = GET_FIB(host_type, prev_owner_i);
		if (fib->flags & FIBF_PREV_OWNER){
			if (fib->field_vartype.obj == (void *)prev_owner_i){
				fib->field_vartype.obj = (void *)fib_offset;
				return;
			}
		} else break;
	} while (true);
	printf("\nCompost anomaly: this type doesn\'t have room for referencers.\n");
	raise(SIGABRT);
}

/* compost_set_dynamic_field (type_t pointer type, type_t pointer field_type, 64bit offset, 8bit flags)
 * note: flags must be a combination of FIBF_BASIC, FIBF_POINTER, FIBF_DEPENDENT, FIBF_ARRAY and FIBF_AUTO_INST.
 * note: offset must be inferior to the type's object_size field.
 *
 * This function is the main way of adding a field to a type. It handles
 * nested objects by creating an appropriate entry in the field_info_a array.
 * It also adds the field into the dynamic_field dictionnary of the type.
 * It has a different method for pointers.
 * Return value: the size of the created field (sizeof(void *) for pointers)
 */
size_t compost_set_dynamic_field(type_t * host_type, vartype_t field_vartype, array field_name, size_t fib_offset, uint8_t flags){
	type_t * stripped_ft = strip_variant(field_vartype);
	bool nested = !(stripped_ft->flags & TYPE_PRIMITIVE) && !(flags & FIBF_POINTER);

	size_t field_size = stripped_ft->object_size;
	void * field_info;
	field_info_b_t * fib;

	if (nested){
		field_info = find_and_fill_fia(host_type, stripped_ft, fib_offset);
		// compost_print_cstr(field_name);
		// printf(" (%li) - nested at %li\n", fib_offset, offset_from_refc);
		if (stripped_ft->offsets > 1){
			for (size_t i = 1; i < stripped_ft->offsets; i++){
				field_info_a_t * fia = GET_FIA(stripped_ft, i);
				find_and_fill_fia(host_type, fia->field_type, fia->data_offset + fib_offset);
			}
		}

		size_t ref_fields_count = 0;
		for (size_t i = 0; i < stripped_ft->object_size; i++){
			fib = GET_FIB(stripped_ft, i);
			if (fib->flags & FIBF_PREV_OWNER){
				ref_fields_count--;
				field_size -= sizeof(void *);
				i += sizeof(void *) - 1;
			} else {
				if ((fib->flags & FIBF_REFERENCES) == FIBF_REFERENCES){
					find_and_fill_prev_owner(host_type, fib_offset + i);
					ref_fields_count++;
				}
				*GET_FIB(host_type, fib_offset + i) = *fib;
			}
		}
		if (ref_fields_count) printf("\nCompost anomaly: bad field type\n");
	} else {
		if (flags & FIBF_POINTER) field_size = sizeof(void *);
		for (size_t i = 0; i < field_size; i++){
			fib = GET_FIB(host_type, fib_offset + i);
			if (i == 0){
				*fib = (field_info_b_t){ field_vartype, flags };
				field_info = fib;
			} else *fib = (field_info_b_t){ { GO_BACK }, FIBF_BASIC };
		}
		if ((flags & FIBF_REFERENCES) == FIBF_REFERENCES){
			find_and_fill_prev_owner(host_type, fib_offset);
		}
	}
	compost_dict_set_pa(host_type->dynamic_fields, field_name, compost_get_obj(field_info));
	return field_size;
}

/* compost_get_flags (object pointer obj)
 *
 * This function fetches the flags of an object if it is the field
 * of another object.
 * Return value: 8bit FIBF_* combination
 */
uint8_t compost_get_flags(void * obj){
	obj_info_t info = get_info(obj);
	uint8_t result;

	if (info.page_type->offsets && info.offset >= info.page_type->offsets){
		size_t offset = info.offset - info.offsets_zone;
		field_info_b_t * fib = GET_FIB(info.page_type, offset);
		result = fib->flags;
	} else result = FIBF_NESTED | FIBF_AUTO_INST;

	return result;
}

void * advance_obj_ptr(void * obj, obj_info_t info, size_t field_offset, bool is_fib){
	bool ptr = is_fib && GET_FIB(info.page_type, field_offset)->flags & FIBF_POINTER;
	if (is_fib){
		field_offset += info.offsets_zone;
		if (info.offset > 0 && info.offset < info.page_type->offsets){
			field_offset += GET_FIA(info.page_type, info.offset)->data_offset;
		}
	} else field_offset += info.offset;

	void ** result = ((void *)compost_get_obj(obj) + field_offset);
	if (ptr && *result) result = *result;
	return result;
}

/* compost_get_field (object pointer obj, string field_name)
 * note: deprecated documentation
 *
 * This function gives a field's address given an object address
 * and a field name. It took *three days* to get it to work.
 * Return value: the field's address
 */
void * compost_get_field(void * obj, size_t length, char * name, bool dynamic){
	type_t * field_type = compost_type_of(obj);
	void * result = compost_dict_get_pa(
		dynamic
		? field_type->dynamic_fields
		: field_type->static_fields,
		(array){ length, name }
	);
	if (dynamic && result){
		bool is_fib = compost_type_of(result)->flags & TYPE_FIB;
		size_t new_offset = compost_array_find(is_fib ? field_type->dfib : field_type->dfia, result);

		result = advance_obj_ptr(obj, get_info(obj), new_offset, is_fib);
	}
	return result;
}

