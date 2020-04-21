/*
 * LibPTA debugging features, C source
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

size_t pta_print_cstr(array string){
	for (int i = 0; i < string.length; i++) putc(char_at(string, i), stdout);
	return string.length;
}

void print_color(int n, bool inverted){
	if (n++ < 0) return;
	printf("\033[%c8:5:%im", inverted ? '4' : '3', ((n < 16) ? (n < 7) : ( (n < 232) ? (((n - 16) % 36) < 18) : (n < 244))) ? 15 : 0);
	printf("\033[%c8:5:%im", inverted ? '3' : '4', n);
}

void print_fields(void * obj){
	type_t * type = pta_type_of(find_raw_refc(obj));
	printf("fia:\n");
	for (size_t i = 1; i < type->offsets; i++){
		field_info_a_t * fia = GET_FIA(type, i);
		printf("%li | %p | %li\n", i, fia->field_type, fia->data_offset);
	}
	printf("fib:\n");
	for (size_t i = 0; i < type->object_size; i++){
		field_info_b_t * fib = GET_FIB(type, i);
		printf("%li | %p | %li\n", i, fib->field_type, fib->flags);
	}
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
	if (obj == NULL) return (void) printf(" (NULL)\n");
	else if (pta_is_pointer(obj)) obj = *(void **)obj;
	type_t * type = pta_type_of(obj);
	if (type == NULL) return (void) printf(" (Unknown type)\n");
	if (type->flags & TYPE_PRIMITIVE){
		printf(" = ");
		char * c_obj = pta_get_c_object(obj);
		if (type->flags & TYPE_CHAR){
			putchar('\'');
			print_char(*c_obj);
			putchar('\'');
		} else print_value(c_obj, type->object_size);
		putchar('\n');
	} else {
		if (recursion) putchar('\n');
		array str = { -1, NULL };
		do {
			pta_dict_get_next_index(type->dynamic_fields, &str);
			if (str.length > 0){
				printf("%0*s‣ ", recursion * 3, " ");
				pta_print_cstr(str);
				void * field = pta_get_field(obj, str);
				if (field){
					if (pta_is_pointer(field) && *(void **)field == NULL) printf(" (NULL)\n");
					else show_fields(field, recursion + 1);
				} else printf(" [unr. field]\n");
			}
		} while (str.length >= 0);
	}
	if (type->flags & TYPE_ARRAY){
		array_part_t * array_part = obj;
		printf("%0*s‣ content: ", recursion * 3, " ");
		if (PG_TYPE(obj)->flags & TYPE_CHAR){
			putchar('\"');
			for (size_t i = 0; i < array_part->size; i++){
				print_char(*(char *)pta_get_c_object(pta_array_get(array_part, i)));
			}
			printf("\"\n");
		} else {
			putchar('\n');
			recursion++;
			for (size_t i = 0; i < array_part->size; i++){
				printf("%0*s• [%li]", recursion * 3, " ", i);
				show_fields(pta_array_get(array_part, i), recursion + 1);
			}
		}
	}
}

void pta_show(void * obj){
	show_fields(obj, 0);
}