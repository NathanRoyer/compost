/*
 * Compost setup features, C source
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

#include "types/type.h"
#include "types/page.h"
#include "types/refc.h"
#include "types/dict.h"

/*
 * This file only contains one function: setup_types, intended to setup the paged-types environment.
 * 
 * It also contains many pages which are used to structure the first pages. What you
 * see below is what's inside the first 5 pages :
 * - internal types and free type slots
 * - field_infos_a arrays for internal types
 * - field_infos_b arrays for internal types
 * - page_list structures
 * - dictionnary headers for internal types' fields
 * the paged-types are ready to go when setup_types() returns.
 *
 */

// typedef COMPOST_STRUCT root_page {
// 	page_desc_t header;

// 	refc_t rt_refc; // root type
// 	type_t rt;

// 	refc_t szt_refc; // size type
// 	type_t szt;

// 	refc_t chrt_refc; // char type
// 	type_t chrt;

// 	refc_t fiat_refc; // field info a type
// 	type_t fiat;

// 	refc_t fibt_refc; // field info b type
// 	type_t fibt;

// 	refc_t dht_refc; // dict header type
// 	type_t dht;

// 	refc_t dbt_refc; // dict block type
// 	type_t dbt;

// 	refc_t art_refc; // array_type
// 	type_t art;

// } root_page_t;

typedef COMPOST_STRUCT {
	uint8_t offset_fib;
	field_info_b_t fib;
} fib_o_t;

typedef COMPOST_STRUCT array_page {
	page_desc_t header;

	// FIA

	array_obj_t rt_fia_ap; // size = 0
	array_obj_t szt_fia_ap; // size = 0
	array_obj_t chrt_fia_ap; // size = 0
	array_obj_t fiat_fia_ap; // size = 0
	array_obj_t fibt_fia_ap; // size = 0
	array_obj_t dht_fia_ap; // size = 0
	array_obj_t dbt_fia_ap; // size = 0
	array_obj_t art_fia_ap; // size = 0

	// FIB

	// root type
	array_obj_t rt_fib_ap; // size = 81

	fib_o_t rt_dfia [8]; // dfia
	fib_o_t rt_dfib [8]; // dfib

	fib_o_t rt_objsz[8]; // object_size
	fib_o_t rt_ofs  [8]; // offsets
	fib_o_t rt_pgsz [8]; // paged_size
	fib_o_t rt_vars [8]; // variants

	fib_o_t rt_dynf [8]; // dynamic_fields
	fib_o_t rt_statf[8]; // static_fields

	fib_o_t rt_pgl  [8]; // page_list
	fib_o_t rt_cdat [8]; // client_data

	fib_o_t rt_flags[1]; // flags

	// size
	array_obj_t szt_fib_ap; // size = 8

	fib_o_t szt_data[8];

	// char
	array_obj_t chrt_fib_ap; // size = 1

	fib_o_t chrt_data[1];

	// field_info_a
	array_obj_t fiat_fib_ap; // size = 16

	fib_o_t fiat_ft[8]; // field_type
	fib_o_t fiat_do[8]; // data_offset

	// field_info_b
	array_obj_t fibt_fib_ap; // size = 9

	fib_o_t fibt_ft   [8]; // field_type
	fib_o_t fibt_flags[1]; // flags

	// dict header
	array_obj_t dht_fib_ap; // size = 24

	fib_o_t dht_fb [8]; // first_block
	fib_o_t dht_ekv[8]; // empty_key_v
	fib_o_t dht_pown[8]; // empty_key_v.prev_owner

	// dict block
	array_obj_t dbt_fib_ap; // size = 33

	fib_o_t dbt_eq  [8]; // equal
	fib_o_t dbt_uneq[8]; // unequal
	fib_o_t dbt_val [8]; // value
	fib_o_t dbt_keyp[1]; // key_part
	fib_o_t dbt_pown[8]; // value.prev_owner

	// array
	array_obj_t art_fib_ap; // size = 24

	fib_o_t art_next[8]; // next
	fib_o_t art_cont[8]; // content_type
	fib_o_t art_cap [8]; // capacity

	// VARIANTS

	// fia array
	array_obj_t var_fia_ap;      // size = 4
	type_t      * var_fia_type;  // base_type
	array_obj_t * var_fia_next;  // next_var
	size_t        var_fia_ofst;  // constraint_offset
	void        * var_fia_value; // constraint_value

	// fib array
	array_obj_t var_fib_ap;      // size = 4
	type_t      * var_fib_type;  // base_type
	array_obj_t * var_fib_next;  // next_var
	size_t        var_fib_ofst;  // constraint_offset
	void        * var_fib_value; // constraint_value

	// ptr array
	array_obj_t var_var_ap;      // size = 4
	type_t      * var_var_type;  // base_type
	array_obj_t * var_var_next;  // next_var
	size_t        var_var_ofst;  // constraint_offset
	void        * var_var_value; // constraint_value

} array_page_t;

typedef COMPOST_STRUCT type_dicts {
	refc_t df_refc;
	dict_t df;
	void * df_pown;
	refc_t sf_refc;
	dict_t sf;
	void * sf_pown;
} type_dicts_t;

typedef COMPOST_STRUCT dict_header_page {
	page_desc_t header;

	type_dicts_t rt;

	type_dicts_t szt;

	type_dicts_t chrt;

	type_dicts_t fiat;

	type_dicts_t fibt;

	type_dicts_t dht;

	type_dicts_t dbt;

	type_dicts_t art;
} dict_header_page_t;

context_t compost_setup(){
	compost_pages = 3;
	root_page_t        * rp  = (root_page_t        *)new_random_page(compost_pages).p;
	array_page_t       * arp = (array_page_t       *)(PP(rp).s  + page_size);
	dict_header_page_t * dhp = (dict_header_page_t *)(PP(arp).s + page_size);

	set_page_descriptor(PP((void *)rp),  &rp->header );
	set_page_descriptor(PP((void *)arp), &arp->header);
	set_page_descriptor(PP((void *)dhp), &dhp->header);

	printf("pgs: %lu\n", page_size);
	printf("ttp: %lu\n", sizeof(root_page_t));
	printf("arp: %lu\n", sizeof(array_page_t));
	printf("dhp: %lu\n", sizeof(dict_header_page_t));

	// ROOT TYPE PAGE
	if (true){
		// ROOT TYPE
		rp->rt_refc = &rp->rt_refc;
		rp->rt = (type_t){
			&arp->rt_fia_ap, &arp->rt_fib_ap,
			sizeof(type_t), 1, // own offset--------------------------- TO WATCH
			0, NULL, // computations later done
			&dhp->rt.df_refc, &dhp->rt.sf_refc,
			rp, NULL,
			TYPE_INTERNAL | TYPE_ROOT
		};
		rp->rt.paged_size = compute_paged_size((&rp->rt));

		prepare_page_desc((page_desc_t *)rp, &rp->rt, NULL, 1, PAGE_BASIC);

		// SIZE TYPE
		rp->szt_refc = &rp->szt_refc;
		rp->szt = (type_t){
			&arp->szt_fia_ap, &arp->szt_fib_ap,
			sizeof(size_t), 0, // own offsets -------------------------- TO WATCH
			0, NULL, // computations later done
			&dhp->szt.df_refc, &dhp->szt.sf_refc,
			NULL, NULL, // no pages
			TYPE_PRIMITIVE | TYPE_INTERNAL
		};
		rp->szt.paged_size = compute_paged_size((&rp->szt));

		// CHAR TYPE
		rp->chrt_refc = &rp->chrt_refc;
		rp->chrt = (type_t){
			&arp->chrt_fia_ap, &arp->chrt_fib_ap,
			sizeof(uint8_t), 0, // own offsets -------------------------- TO WATCH
			0, NULL, // computations later done
			&dhp->chrt.df_refc, &dhp->chrt.sf_refc,
			NULL, NULL, // no pages
			TYPE_PRIMITIVE | TYPE_INTERNAL | TYPE_CHAR
		};
		rp->chrt.paged_size = compute_paged_size((&rp->chrt));

		// FIAT TYPE
		rp->fiat_refc = &rp->fiat_refc;
		rp->fiat = (type_t){
			&arp->fiat_fia_ap, &arp->fiat_fib_ap,
			sizeof(field_info_a_t), 1, // own offset--------------------- TO WATCH
			0, NULL, // computations later done
			&dhp->fiat.df_refc, &dhp->fiat.sf_refc,
			NULL, NULL,
			TYPE_INTERNAL
		};
		rp->fiat.paged_size = compute_paged_size((&rp->fiat));

		// FIBT TYPE
		rp->fibt_refc = &rp->fibt_refc;
		rp->fibt = (type_t){
			&arp->fibt_fia_ap, &arp->fibt_fib_ap,
			sizeof(field_info_b_t), 1, // own offset--------------------- TO WATCH
			0, NULL, // computations later done
			&dhp->fibt.df_refc, &dhp->fibt.sf_refc,
			NULL, NULL,
			TYPE_INTERNAL | TYPE_FIB
		};
		rp->fibt.paged_size = compute_paged_size((&rp->fibt));

		// DHT TYPE
		rp->dht_refc = &rp->dht_refc;
		rp->dht = (type_t){
			&arp->dht_fia_ap, &arp->dht_fib_ap,
			sizeof(dict_t) + sizeof(void *) /* pown */, 1, // own offset---------------------------- TO WATCH
			0, NULL, // computations later done
			&dhp->dht.df_refc, &dhp->dht.sf_refc,
			dhp, NULL,
			TYPE_INTERNAL
		};
		rp->dht.paged_size = compute_paged_size((&rp->dht));

		prepare_page_desc((page_desc_t *)dhp, &rp->dht, NULL, 1, PAGE_DEPENDENT);

		// DBT TYPE
		rp->dbt_refc = &rp->dbt_refc;
		rp->dbt = (type_t){
			&arp->dbt_fia_ap, &arp->dbt_fib_ap,
			sizeof(dict_block_t) + sizeof(void *) /* pown */, 1, // own offset---------------------- TO WATCH
			0, NULL, // computations later done
			&dhp->dbt.df_refc, &dhp->dbt.sf_refc,
			NULL, NULL, // no page yet
			TYPE_INTERNAL
		};
		rp->dbt.paged_size = compute_paged_size((&rp->dbt));

		// ARRAY TYPE
		rp->art_refc = &rp->art_refc;
		rp->art = (type_t){
			&arp->art_fia_ap, &arp->art_fib_ap,
			24, 1, // own offset---------------------- TO WATCH
			0, &arp->var_fia_ap, // computations later done
			&dhp->art.df_refc, &dhp->art.sf_refc,
			arp, NULL, // no page yet
			TYPE_INTERNAL | TYPE_ARRAY
		};
		rp->art.paged_size = compute_paged_size((&rp->art));

		prepare_page_desc((page_desc_t *)arp, &rp->art, NULL, 1, PAGE_DEPENDENT);
	}
	// FIA ARRAYS
	if (true){
		// refc, size, following free space, next part

		arp->rt_fia_ap = (array_obj_t){ &rp->rt_refc, &arp->szt_fia_ap, &rp->fiat_refc, 0 };
		arp->szt_fia_ap = (array_obj_t){ &rp->szt_refc, &arp->chrt_fia_ap, &rp->fiat_refc, 0 };
		arp->chrt_fia_ap = (array_obj_t){ &rp->chrt_refc, &arp->fiat_fia_ap, &rp->fiat_refc, 0 };
		arp->fiat_fia_ap = (array_obj_t){ &rp->fiat_refc, &arp->fibt_fia_ap, &rp->fiat_refc, 0 };
		arp->fibt_fia_ap = (array_obj_t){ &rp->fibt_refc, &arp->dht_fia_ap, &rp->fiat_refc, 0 };
		arp->dht_fia_ap = (array_obj_t){ &rp->dht_refc, &arp->dbt_fia_ap, &rp->fiat_refc, 0 };
		arp->dbt_fia_ap = (array_obj_t){ &rp->dbt_refc, &arp->art_fia_ap, &rp->fiat_refc, 0 };
		arp->art_fia_ap = (array_obj_t){ &rp->art_refc, &arp->rt_fib_ap, &rp->fiat_refc, 0 };
	}
	// FIB ARRAYS
	if (true){
		// refc, size, following free space, next part
		// field_type, pointer_to_dependent
		field_info_b_t goback = { GO_BACK, FIBF_BASIC };

		// ROOT TYPE
		arp->rt_fib_ap = (array_obj_t){ &rp->rt_refc, &arp->szt_fib_ap, &rp->fibt, 81 };

		arp->rt_dfia [0].fib = (field_info_b_t){ (type_t *)&arp->var_fia_ap, FIBF_DEPENDENT };
		arp->rt_dfib [0].fib = (field_info_b_t){ (type_t *)&arp->var_fib_ap, FIBF_DEPENDENT };
		arp->rt_objsz[0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC };
		arp->rt_ofs  [0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC };
		arp->rt_pgsz [0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC };
		arp->rt_vars [0].fib = (field_info_b_t){ (type_t *)&arp->var_var_ap, FIBF_DEPENDENT };
		arp->rt_dynf [0].fib = (field_info_b_t){ &rp->dht, FIBF_DEPENDENT };
		arp->rt_statf[0].fib = (field_info_b_t){ &rp->dht, FIBF_DEPENDENT };
		arp->rt_pgl  [0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC };
		arp->rt_cdat [0].fib = (field_info_b_t){ NULL, FIBF_BASIC }; // set by the client
		arp->rt_flags[0].fib = (field_info_b_t){ &rp->chrt, FIBF_BASIC };

		for (int i = 1; i < 8; i++) arp->rt_dfia [i].fib = goback;
		for (int i = 1; i < 8; i++) arp->rt_dfib [i].fib = goback;
		for (int i = 1; i < 8; i++) arp->rt_objsz[i].fib = goback;
		for (int i = 1; i < 8; i++) arp->rt_ofs  [i].fib = goback;
		for (int i = 1; i < 8; i++) arp->rt_pgsz [i].fib = goback;
		for (int i = 1; i < 8; i++) arp->rt_vars [i].fib = goback;
		for (int i = 1; i < 8; i++) arp->rt_dynf [i].fib = goback;
		for (int i = 1; i < 8; i++) arp->rt_statf[i].fib = goback;
		for (int i = 1; i < 8; i++) arp->rt_pgl  [i].fib = goback;
		for (int i = 1; i < 8; i++) arp->rt_cdat [i].fib = goback;
		// rt_flags is only 1 byte


		// SIZE TYPE
		arp->szt_fib_ap = (array_obj_t){ &rp->szt_refc, &arp->chrt_fib_ap, &rp->fibt_refc, 8 };

		arp->szt_data[0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC };

		for (int i = 1; i < 8; i++) arp->szt_data[i].fib = goback;

		// CHAR TYPE
		arp->chrt_fib_ap = (array_obj_t){ &rp->chrt_refc, &arp->fiat_fib_ap, &rp->fibt_refc, 1 };

		arp->chrt_data[0].fib = (field_info_b_t){ &rp->chrt, FIBF_BASIC };

		// FIAT TYPE
		arp->fiat_fib_ap = (array_obj_t){ &rp->fiat_refc, &arp->fibt_fib_ap, &rp->fibt_refc, 16 };
		
		arp->fiat_ft[0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC }; // field_type
		arp->fiat_do[0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC }; // data offset
		for (int i = 1; i < 8; i++) arp->fiat_ft[i].fib = goback;
		for (int i = 1; i < 8; i++) arp->fiat_do[i].fib = goback;
		
		// FIBT TYPE
		arp->fibt_fib_ap = (array_obj_t){ &rp->fibt_refc, &arp->dht_fib_ap, &rp->fibt_refc, 9 };
		
		arp->fibt_ft   [0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC }; // field_type
		arp->fibt_flags[0].fib = (field_info_b_t){ &rp->chrt, FIBF_BASIC }; // flags
		for (int i = 1; i < 8; i++) arp->fibt_ft[i].fib = goback;
		// fibt_flags is only 1 byte
		
		// DICT HEADER TYPE
		arp->dht_fib_ap = (array_obj_t){ &rp->dht_refc, &arp->dbt_fib_ap, &rp->fibt_refc, 24 };
		
		arp->dht_fb [0].fib = (field_info_b_t){ &rp->dbt, FIBF_DEPENDENT }; // first_block
		arp->dht_ekv[0].fib = (field_info_b_t){ &rp->szt, FIBF_REFERENCES }; // empty_key_v
		arp->dht_pown[0].fib = (field_info_b_t){ (void *)8,  FIBF_PREV_OWNER }; // pown
		for (int i = 1; i < 8; i++) arp->dht_fb [i].fib = goback;
		for (int i = 1; i < 8; i++) arp->dht_ekv[i].fib = goback;
		
		// DICT BLOCK TYPE
		arp->dbt_fib_ap = (array_obj_t){ &rp->dbt_refc, &arp->art_fib_ap, &rp->fibt_refc, 33 };
		
		arp->dbt_eq  [0].fib = (field_info_b_t){ &rp->dbt,  FIBF_DEPENDENT }; // equal
		arp->dbt_uneq[0].fib = (field_info_b_t){ &rp->dbt,  FIBF_DEPENDENT }; // unequal
		arp->dbt_val [0].fib = (field_info_b_t){ &rp->szt,  FIBF_REFERENCES }; // value
		arp->dbt_keyp[0].fib = (field_info_b_t){ &rp->chrt, FIBF_BASIC }; // key_part
		arp->dbt_pown[0].fib = (field_info_b_t){ (void *)16,  FIBF_PREV_OWNER }; // pown
		for (int i = 1; i < 8; i++) arp->dbt_eq  [i].fib = goback;
		for (int i = 1; i < 8; i++) arp->dbt_uneq[i].fib = goback;
		for (int i = 1; i < 8; i++) arp->dbt_val [i].fib = goback;
		// would be useless to set goback on pown
		
		// ARRAY TYPE
		arp->art_fib_ap = (array_obj_t){ &rp->art_refc, &arp->var_fia_ap, &rp->fibt_refc, 24 };
		
		arp->art_next[0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC }; // next
		arp->art_cont[0].fib = (field_info_b_t){ &rp->szt,  FIBF_BASIC }; // content_type
		arp->art_cap [0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC }; // capacity
		for (int i = 1; i < 8; i++) arp->art_next[i].fib = goback;
		for (int i = 1; i < 8; i++) arp->art_cont[i].fib = goback;
		for (int i = 1; i < 8; i++) arp->art_cap [i].fib = goback;
	}
	// VARIANTS ARRAYS
	if (true){
		// refc, size, following free space, next part

		arp->var_fia_ap = (array_obj_t){ &rp->art_refc, &arp->var_fib_ap, &rp->szt_refc, 4 };
		arp->var_fib_ap = (array_obj_t){ &rp->art_refc, &arp->var_var_ap, &rp->szt_refc, 4 };
		arp->var_var_ap = (array_obj_t){ &rp->art_refc, NULL,             &rp->szt_refc, 4 };

		arp->var_fia_type = &rp->art;
		arp->var_fib_type = &rp->art;
		arp->var_var_type = &rp->art;

		arp->var_fia_next = &arp->var_fib_ap;
		arp->var_fib_next = &arp->var_var_ap;
		arp->var_var_next = NULL;

		arp->var_fia_ofst = sizeof(size_t);
		arp->var_fib_ofst = sizeof(size_t);
		arp->var_var_ofst = sizeof(size_t);

		arp->var_fia_value = &rp->fiat_refc;
		arp->var_fib_value = &rp->fibt_refc;
		arp->var_var_value = &rp->szt_refc;
	}
	// DH PAGE
	if (true){
		dhp->rt.df_refc = &rp->rt_refc;
		dhp->rt.sf_refc = &rp->rt_refc;
		dhp->rt.df = (dict_t){ NULL, NULL };
		dhp->rt.sf = (dict_t){ NULL, NULL };

		dhp->szt.df_refc = &rp->szt_refc;
		dhp->szt.sf_refc = &rp->szt_refc;
		dhp->szt.df = (dict_t){ NULL, NULL };
		dhp->szt.sf = (dict_t){ NULL, NULL };

		dhp->chrt.df_refc = &rp->chrt_refc;
		dhp->chrt.sf_refc = &rp->chrt_refc;
		dhp->chrt.df = (dict_t){ NULL, NULL };
		dhp->chrt.sf = (dict_t){ NULL, NULL };

		dhp->fiat.df_refc = &rp->fiat_refc;
		dhp->fiat.sf_refc = &rp->fiat_refc;
		dhp->fiat.df = (dict_t){ NULL, NULL };
		dhp->fiat.sf = (dict_t){ NULL, NULL };

		dhp->fibt.df_refc = &rp->fibt_refc;
		dhp->fibt.sf_refc = &rp->fibt_refc;
		dhp->fibt.df = (dict_t){ NULL, NULL };
		dhp->fibt.sf = (dict_t){ NULL, NULL };

		dhp->dht.df_refc = &rp->dht_refc;
		dhp->dht.sf_refc = &rp->dht_refc;
		dhp->dht.df = (dict_t){ NULL, NULL };
		dhp->dht.sf = (dict_t){ NULL, NULL };

		dhp->dbt.df_refc = &rp->dbt_refc;
		dhp->dbt.sf_refc = &rp->dbt_refc;
		dhp->dbt.df = (dict_t){ NULL, NULL };
		dhp->dbt.sf = (dict_t){ NULL, NULL };

		dhp->art.df_refc = &rp->art_refc;
		dhp->art.sf_refc = &rp->art_refc;
		dhp->art.df = (dict_t){ NULL, NULL };
		dhp->art.sf = (dict_t){ NULL, NULL };
	}

	// FIB INIT
	if (true){
		// offset = fib + type.offsets
#define FIBP(abc) &arp->abc
		// rt
		void * df = rp->rt.dynamic_fields;
		compost_dict_set_pa(df, const_array("dfia"),            FIBP(rt_dfia));
		compost_dict_set_pa(df, const_array("dfib"),            FIBP(rt_dfib));
		compost_dict_set_pa(df, const_array("variants"),        FIBP(rt_vars));
		compost_dict_set_pa(df, const_array("object_size"),     FIBP(rt_objsz));
		compost_dict_set_pa(df, const_array("offsets"),         FIBP(rt_ofs));
		compost_dict_set_pa(df, const_array("paged_size"),      FIBP(rt_pgsz));
		compost_dict_set_pa(df, const_array("dynamic_fields"),  FIBP(rt_dynf));
		compost_dict_set_pa(df, const_array("static_fields"),   FIBP(rt_statf));
		compost_dict_set_pa(df, const_array("page_list"),       FIBP(rt_pgl));
		compost_dict_set_pa(df, const_array("flags"),           FIBP(rt_flags));

		// fiat
		df = rp->fiat.dynamic_fields;
		compost_dict_set_pa(df, const_array("field_type"),      FIBP(fiat_ft));
		compost_dict_set_pa(df, const_array("data_offset"),     FIBP(fiat_do));

		// fibt
		df = rp->fibt.dynamic_fields;
		compost_dict_set_pa(df, const_array("field_type"),      FIBP(fibt_ft));
		compost_dict_set_pa(df, const_array("flags"),           FIBP(fibt_flags));

		// dht
		df = rp->dht.dynamic_fields;
		compost_dict_set_pa(df, const_array("first_block"),     FIBP(dht_fb));
		compost_dict_set_pa(df, const_array("empty_key_v"),     FIBP(dht_ekv));

		// dbt
		df = rp->dbt.dynamic_fields;
		compost_dict_set_pa(df, const_array("equal"),           FIBP(dbt_eq));
		compost_dict_set_pa(df, const_array("unequal"),         FIBP(dbt_uneq));
		compost_dict_set_pa(df, const_array("value"),           FIBP(dbt_val));
		compost_dict_set_pa(df, const_array("key_part"),        FIBP(dbt_keyp));

		// array
		df = rp->art.dynamic_fields;
		compost_dict_set_pa(df, const_array("content_type"),    FIBP(art_cont));
		compost_dict_set_pa(df, const_array("capacity"),        FIBP(art_cap));
	}

	return (context_t){ &rp->rt, &rp->szt, &rp->chrt, &rp->dht, &rp->art };
}

void compost_for_each_type(type_t * root_type, compost_for_each_type_callback cb, void * arg){
	page_desc_t * desc = root_type->page_list;
	while (desc){
		size_t pg_limit = PG_LIMIT(desc, root_type);
		for (void * refc = PG_REFC2(desc); PP(refc).s < pg_limit; refc += root_type->paged_size){
			if (*(void **)refc == NULL) continue;
			cb(refc, arg);
		}
		desc = PG_NEXT(desc);
	}
}