/*
 * Compost debugging console, C source
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
#include "compost.h"

#define CMD(actual) (strcmp(cmd, actual) == 0)
#define cstrarray(string) ((compost_array){ strlen(string), string })

void * variables;

#define L(a) a.length

void * get_var(compost_array arg){
	void * var = NULL;
	if (arg.data == NULL) printf("This command requires an argument.\n");
	else {
		var = compost_dict_get_pa(variables, arg);
		if (var == NULL) printf("Variable not found: \"%s\".\n", arg.data);
	}
	return var;
}

compost_array next_arg(compost_array * cmd){
	compost_array arg = { 0, NULL };
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
	compost_context_t ctx = compost_setup();
	variables = compost_prepare(compost_spot(ctx.dht), ctx.dht);
	compost_protect(variables);
	compost_dict_set_pa(variables, compost_const_array("type_t"), compost_get_obj(ctx.rt));
	compost_dict_set_pa(variables, compost_const_array("size_t"), compost_get_obj(ctx.szt));
	compost_dict_set_pa(variables, compost_const_array("char_t"), compost_get_obj(ctx.chrt));
	compost_dict_set_pa(variables, compost_const_array("dict_t"), compost_get_obj(ctx.dht));
	compost_dict_set_pa(variables, compost_const_array("array_t"), compost_get_obj(ctx.art));

	bool running = true;
	while (running){
		char * cmd = readline("> ");
		compost_array arg = next_arg(&cstrarray(cmd));

		if (CMD("exit")) running = false;
		else if (CMD("show")){
			void * var = get_var(arg);
			if (var) compost_show(var);
		} else if (CMD("fields")){
			void * var = get_var(arg);
			if (var){
				if (compost_type_of(var, true) == ctx.rt){
					compost_print_fields(NULL, compost_get_c_object(var));
				} else {
					compost_print_fields(var, NULL);
				}
			}
		} else if (CMD("new")){
			compost_array type_name = arg;
			compost_array instance_name = next_arg(&type_name);
			if (instance_name.length){
				void * type = get_var(type_name);
				if (type){
					if (compost_type_of(type, true) == ctx.rt){
						compost_type_t * var_t = compost_get_c_object(type);
						void * new_var = compost_spot(var_t);
						printf("spotted\n");
						compost_dict_set_pa(variables, instance_name, new_var);
						compost_protect(new_var);
					} else {
						printf("Argument ");
						compost_print_cstr(type_name);
						printf(" is not a type: %p, %p.\n", type, compost_type_of(type, true));
					}
				}
			} else printf("Invalid variable name.\n");
		} else if (CMD("type")){
			compost_array new_type_name = arg;
			if (new_type_name.length){

				char * object_size_str = readline("object size ? ");
				char * offsets_str = readline("nested objects ? ");
				char * referencers_str = readline("referencers ? ");
				size_t referencers = atoi(referencers_str);
				printf("referencers: %li\n", referencers);
				void * new_type_refc = compost_create_type(ctx.szt, atoi(offsets_str), referencers, atoi(object_size_str), COMPOST_TYPE_BASIC);
				free(referencers_str);
				free(offsets_str);
				free(object_size_str);

				printf("New type: ");
				compost_print_cstr(new_type_name);
				printf(", %p\n", new_type_refc);
				compost_protect(new_type_refc);
				compost_dict_set_pa(variables, new_type_name, new_type_refc);

				compost_type_t * new_type = compost_get_c_object(new_type_refc);
				size_t field_offset = 0;

				printf("Specify the fields :\n");
				printf("format: [distant] type_name field_name\n");
				for (;;){
					char * type_name_cstr = readline("T: ");
					if (strcmp(type_name_cstr, "end") == 0) break;
					compost_array type_name = cstrarray(type_name_cstr);
					compost_array field_name = next_arg(&type_name);
					bool is_pointer = strcmp(type_name.data, "distant") == 0;
					if (is_pointer){
						type_name = field_name;
						field_name = next_arg(&type_name);
					}

					uint8_t flags = is_pointer ? (COMPOST_FIELD_POINTER | COMPOST_FIELD_DEPENDENT | COMPOST_FIELD_AUTO_INST) : COMPOST_FIELD_BASIC;

					void * type_refc = get_var(type_name);
					compost_type_t * field_type = compost_get_c_object(type_refc);
					if (field_type){
						field_offset += compost_set_dynamic_field(new_type, field_type, field_name, field_offset, flags);
					}

					free(type_name_cstr);
				}

			} else printf("That ain't no good type name, you fool.\n");
		} else if (CMD("help")){
			printf("page VARIABLE        show a variable's page\n");
			printf("show VARIABLE        show a variable in its page\n");
			printf("new TYPE VARIABLE    create a new instance of a type\n");
			printf("type NEW_TYPE        create a new type\n");
		} else if (CMD("pages")){
			printf("%lu pages\n", compost_pages);
			compost_print_regs();
		} else if (!CMD("")) printf("Unknown command: \"%s\".\n", cmd);
		free(cmd);
	}
	return 0;
}