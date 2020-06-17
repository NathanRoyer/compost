/*
 * Compost debugging features, C source
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

#include "types/debug.h"

size_t reg_cap;

void print_reg_content(ptr_t reg, size_t recursion, bool explore){
	printf("%0*s‣ %p\n", recursion * 3, " ", reg.p);
	if (explore){
		size_t i = reg.s & reg_i_mask;
		reg.s = reg.s & page_mask;
		ptr_t md = get_reg_metadata(reg);
		explore = i > page_relative_bits;
		if (explore) printf("MD = %p\n", recursion * 3, " ", md.p);
		for (size_t r = 0; r < reg_cap; r++){
			ptr_t content = reg.p[r];
			if (content.s & (explore ? page_mask : content.s)) print_reg_content(content, recursion + 1, i > page_relative_bits);
		}
	}
}

void compost_print_regs(){
	reg_cap = reg_mask + 1;
	print_reg_content(first_reg, 0, true);
}

size_t compost_print_cstr(array string){
	for (int i = 0; i < string.length; i++) putc(char_at(string, i), stdout);
	return string.length;
}

void print_color(int n, bool inverted){
	if (n++ < 0) return;
	printf("\033[%c8:5:%im", inverted ? '4' : '3', ((n < 16) ? (n < 7) : ( (n < 232) ? (((n - 16) % 36) < 18) : (n < 244))) ? 15 : 0);
	printf("\033[%c8:5:%im", inverted ? '3' : '4', n);
}

#define IS_(flags, flag) ((flags & flag) == flag)

char field_flags_str[] = "ffff";
void load_fields(uint8_t flags){
	int i = 0;
	/**/ if (IS_(flags, FIBF_DEPENDENT )) field_flags_str[i] = 'D';
	else if (IS_(flags, FIBF_REFERENCES)) field_flags_str[i] = 'R';
	else if (IS_(flags, FIBF_POINTER   )) field_flags_str[i] = 'P';
	else field_flags_str[i] = '-';
	i++;
	/**/ if (IS_(flags, FIBF_AUTO_INST )) field_flags_str[i] = 'I';
	else field_flags_str[i] = '-';
	i++;
	/**/ if (IS_(flags, FIBF_NESTED    )) field_flags_str[i] = 'N';
	else field_flags_str[i] = '-';
	i++;
	/**/ if (IS_(flags, FIBF_MALLOC    )) field_flags_str[i] = 'M';
	else if (IS_(flags, FIBF_PREV_OWNER)) field_flags_str[i] = 'O';
	else field_flags_str[i] = '-';
}

void compost_print_fields(void * obj, type_t * type){
	void * refc = obj ? compost_get_obj(obj) : NULL;
	if (!obj) obj = (void *)-1;
	if (type == NULL) type = compost_type_of(refc);
	printf("\n+- Field Info A -------------------+\n| OFST |        TYPE        | DEST |\n");
	printf("+------+--------------------+------+\n");
	for (size_t i = 0; i < type->offsets; i++){
		if (refc + i == obj) print_color(0, false);
		printf("| %04li | ", i);
		if (i == 0) printf("       self        | ----");
		else {
			field_info_a_t * fia = GET_FIA(type, i);
			if (fia->field_type == GO_BACK) printf("↑                ↑");
			else printf("%018p", fia->field_type);
			printf(" | %04li", fia->data_offset);
		}
		printf(" |\033[m\n");
	}
	printf("+------+--------------------+------+\n");
	printf("\n+- Field Info B -------------------+\n| OFST |        TYPE        | FLAG |\n");
	printf("+------+--------------------+------+\n");
	refc = compost_get_c_object(refc);
	for (size_t i = 0; i < type->object_size; i++){
		field_info_b_t * fib = GET_FIB(type, i);
		if (refc + i == obj) print_color(0, false);
		printf("| %04li | ", i);
		if (fib->field_vartype.type == GO_BACK) printf("↑                ↑");
		else printf("%018p", fib->field_vartype.obj);
		load_fields(fib->flags);
		printf(" | %s |\033[m\n", field_flags_str);
	}
	printf("+------+--------------------+------+\n");
}

void print_char(char c){
	switch(c){
		case '\n':
			printf("\\n");
			break;
		case '\r':
			printf("\\r");
			break;
		case '\"':
			printf("\\\"");
			break;
		case '\'':
			printf("\\\'");
			break;
		case '\\':
			printf("\\\\");
			break;
		case '\t':
			printf("\\t");
			break;
		default:
			if (c < ' ' || c > '~') printf("\\x%02hhx", c);
			else putchar(c);
	}
}

size_t print_value(void * object, size_t size){
	bool has_output = false;
	size_t retv = 0;
	for (int i = size - 1; i >= 0; i--){
		char value = *(char *)(object + i);
		if (has_output || value != 0 || i == 0){
			if (!has_output){
				if (i == 0){
					retv += printf("%hhi", value);
					continue;
				}
				retv += printf("0x");
				has_output = true;
			}
			retv += printf("%02hhx", value);
		}
	}
	return retv;
}

void show_fields(void * obj, size_t recursion){
	// sleep(1);
	if (obj == NULL) return (void) printf(" (NULL)\n");
	type_t * type = compost_type_of(obj);
	if (type == NULL) return (void) printf(" (Unknown type)\n");
	if (type->flags & TYPE_PRIMITIVE){
		printf(" = ");
		char * c_obj = compost_get_c_object(obj);
		if (type->flags & TYPE_CHAR){
			putchar('\'');
			print_char(*c_obj);
			putchar('\'');
		} else print_value(c_obj, type->object_size);
		putchar('\n');
	} else {
		if (compost_is_pointer(obj)){
			obj = *(void **)obj;
			type = compost_type_of(obj);
		}
		if (recursion) putchar('\n');
		array str = { -1, NULL };
		do {
			compost_dict_get_next_index(type->dynamic_fields, &str);
			if (str.data != NULL){
				printf("%0*s‣ ", recursion * 3, " ");
				compost_print_cstr(str);
				void * field = compost_get_field(obj, str.length, str.data, true);
				if (field){
					if (compost_is_pointer(field) && *(void **)field == NULL) printf(" (NULL)\n");
					else show_fields(field, recursion + 1);
				} else printf(" [unr. field]\n");
			}
		} while (str.data != NULL);
	}
	if (type->flags & TYPE_ARRAY){
		array_obj_t * array_obj = obj;
		printf("%0*s‣ content: ", recursion * 3, " ");
		if (((type_t *)compost_get_c_object(array_obj->content_type))->flags & TYPE_CHAR){
			putchar('\"');
			for (size_t i = 0; i < array_obj->capacity; i++){
				print_char(*(char *)compost_get_c_object(compost_array_get(array_obj, i)));
			}
			printf("\"\n");
		} else {
			putchar('\n');
			recursion++;
			for (size_t i = 0; i < array_obj->capacity; i++){
				printf("%0*s• [%li]", recursion * 3, " ", i);
				show_fields(compost_array_get(array_obj, i), recursion + 1);
			}
		}
	}
}

void compost_show(void * obj){
	show_fields(obj, 0);
}

void compost_show_references(void * obj){
	void ** orig_refc = compost_get_final_obj(obj);
	void ** refc = orig_refc;
	while (*refc != NULL && *refc != orig_refc){
		printf("referencer: %p\n", *refc);
		refc = get_previous_owner(*refc);
	}
}

void compost_show_pages(void * any_paged_address){
	root_page_t * rp = get_root_page(any_paged_address);
	page_desc_t * desc = rp->rt.page_list;
	while (desc){
		for (void * refc = PG_REFC2(desc); PP(refc).s < PG_LIMIT(desc, &rp->rt); refc += rp->rt.paged_size){
			if (*(void **)refc == NULL) continue;
			type_t * type = compost_get_c_object(refc);
			printf("Pages of a type with object_size = %li:\n", type->object_size);
			page_desc_t * page = type->page_list;
			while (page){
				type_t * pgtype = strip_variant(PG_TYPE2(page));
				size_t instances = page_occupied_slots(PG_LIMIT(page, pgtype), PG_FLAGS(page), PG_REFC2(page), type);
				if (pgtype != type) printf("Error with the following page type:\n");
				printf("%p :\n\tflags: %hhx\n\tinstances: %lu\n", page, PG_FLAGS(page), instances);
				page = PG_NEXT(page);
			}

		}
		desc = PG_NEXT(desc);
	}
}