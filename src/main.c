/*
 * LibPTA debugging console, C source
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

#include <stdio.h>
#include <string.h>
#include <readline/readline.h>
#include <stdlib.h>
#include "pta.h"

#define CMD(actual) (strcmp(cmd, actual) == 0)
#define cstrarray(string) ((pta_array){ strlen(string), string })

void * variables;

#define L(a) a.length

void * get_var(pta_array arg){
	void * var = NULL;
	if (arg.data == NULL) printf("This command requires an argument.\n");
	else {
		var = pta_dict_get_pa(variables, arg);
		if (var == NULL) printf("Variable not found: \"%s\".\n", arg.data);
	}
	return var;
}

pta_array next_arg(pta_array * cmd){
	pta_array arg = { 0, NULL };
	for (int i = 0; i < cmd->length; i++){
		if (cmd->data[i] == ' '){
			cmd->data[i] = '\0';
			arg = cstrarray(&(cmd->data[i + 1]));
			break;
		}
	}
	cmd->length -= arg.length + 1;
	return arg;
}

int main(int argc, char *argv[]){
	pta_context_t ctx = pta_setup();
	pta_obj variables1 = pta_prepare(pta_spot(ctx.dht), ctx.dht);
	pta_protect(variables1);
	variables = pta_prepare(pta_spot(ctx.dht), ctx.dht);
	pta_protect(variables);
	pta_dict_set_pa(variables, pta_const_array("type_t"), pta_get_obj(ctx.rt));
	pta_dict_set_pa(variables, pta_const_array("size_t"), pta_get_obj(ctx.szt));
	pta_dict_set_pa(variables, pta_const_array("char_t"), pta_get_obj(ctx.chrt));
	printf("1:\n");
	pta_show_references(ctx.dht);
	pta_dict_set_pa(variables, pta_const_array("dict_t"), pta_get_obj(ctx.dht));
	printf("2:\n");
	pta_show_references(ctx.dht);
	pta_dict_set_pa(variables1, pta_const_array("dict_t"), pta_get_obj(ctx.dht));
	printf("3:\n");
	pta_show_references(ctx.dht);
	pta_dict_set_pa(variables, pta_const_array("dict_t"), NULL);
	printf("4:\n");
	pta_show_references(ctx.dht);

	bool running = true;
	while (running){
		char * cmd = readline("> ");
		pta_array arg = next_arg(&cstrarray(cmd));

		if (CMD("exit")) running = false;
		else if (CMD("show")){
			void * var = get_var(arg);
			if (var) pta_show(var);
		} else if (CMD("new")){
			pta_array type_name = arg;
			pta_array instance_name = next_arg(&type_name);
			if (instance_name.length){
				void * type = get_var(type_name);
				if (type){
					if (pta_type_of(type) == ctx.rt){
						pta_type_t * var_t = pta_get_c_object(type);
						void * new_var = pta_prepare(pta_spot(var_t), var_t);
						printf("spotted\n");
						pta_dict_set_pa(variables, instance_name, new_var);
						pta_protect(new_var);
					} else {
						printf("Argument ");
						pta_print_cstr(type_name);
						printf(" is not a type: %p, %p.\n", type, pta_type_of(type));
					}
				}
			} else printf("Invalid variable name.\n");
		} else if (CMD("type")){
			pta_array new_type_name = arg;
			if (new_type_name.length){

				char * object_size_str = readline("object size ? ");
				char * offsets_str = readline("nested objects ? ");
				char * referencers_str = readline("referencers ? ");
				size_t referencers = atoi(referencers_str);
				printf("referencers: %li\n", referencers);
				void * new_type_refc = pta_create_type(ctx.szt, atoi(offsets_str), referencers, atoi(object_size_str), PTA_TYPE_BASIC);
				free(referencers_str);
				free(offsets_str);
				free(object_size_str);

				printf("New type: ");
				pta_print_cstr(new_type_name);
				printf(", %p\n", new_type_refc);
				pta_protect(new_type_refc);
				pta_dict_set_pa(variables, new_type_name, new_type_refc);

				pta_type_t * new_type = pta_get_c_object(new_type_refc);
				size_t field_offset = 0;

				printf("Specify the fields :\n");
				printf("format: [distant] type_name field_name\n");
				while (field_offset < (new_type->object_size - (referencers * sizeof(void *)))){
					char * type_name_cstr = readline("T: ");
					pta_array type_name = cstrarray(type_name_cstr);
					pta_array field_name = next_arg(&type_name);
					bool is_pointer = strcmp(type_name.data, "distant") == 0;
					if (is_pointer){
						type_name = field_name;
						field_name = next_arg(&type_name);
					}

					uint8_t flags = is_pointer ? (PTA_FIELD_POINTER | PTA_FIELD_DEPENDENT | PTA_FIELD_AUTO_INST) : PTA_FIELD_BASIC;

					void * type_refc = get_var(type_name);
					pta_type_t * field_type = pta_get_c_object(type_refc);
					if (field_type){
						field_offset += pta_set_dynamic_field(new_type, field_type, field_name, field_offset, flags);
					}

					free(type_name_cstr);
				}

			} else printf("That ain't no good type name, you fool.\n");
		} else if (CMD("help")){
			printf("page VARIABLE        show a variable's page\n");
			printf("show VARIABLE        show a variable in its page\n");
			printf("new TYPE VARIABLE    create a new instance of a type\n");
			printf("type NEW_TYPE        create a new type\n");
		} else if (!CMD("")) printf("Unknown command: \"%s\".\n", cmd);
		free(cmd);
	}
	return 0;
}