/*
 * LibPTA setup features, C source
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

// typedef PTA_STRUCT root_page {
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

typedef PTA_STRUCT f_info_a_page {
	page_desc_t header;

	array_part_t rt_fia_ap; // size = 0
	array_part_t szt_fia_ap; // size = 0
	array_part_t chrt_fia_ap; // size = 0
	array_part_t fiat_fia_ap; // size = 0
	array_part_t fibt_fia_ap; // size = 0
	array_part_t dht_fia_ap; // size = 0
	array_part_t dbt_fia_ap; // size = 0
	array_part_t art_fia_ap; // size = 0
} f_info_a_page_t;

typedef PTA_STRUCT {
	uint8_t offset_fib;
	field_info_b_t fib;
} fib_o_t;

typedef PTA_STRUCT f_info_b_page {
	page_desc_t header;

	// root type
	array_part_t rt_fib_ap; // size = 81

	fib_o_t rt_dfia [8]; // dfia
	fib_o_t rt_dfib [8]; // dfib

	fib_o_t rt_objsz[8]; // object_size
	fib_o_t rt_ofs  [8]; // offsets
	fib_o_t rt_pgsz [8]; // paged_size
	fib_o_t rt_pglim[8]; // page_limit

	fib_o_t rt_dynf [8]; // dynamic_fields
	fib_o_t rt_statf[8]; // static_fields

	fib_o_t rt_pgl  [8]; // page_list
	fib_o_t rt_cdat [8]; // client_data

	fib_o_t rt_flags[1]; // flags

	// size
	array_part_t szt_fib_ap; // size = 8

	fib_o_t szt_data[8];

	// char
	array_part_t chrt_fib_ap; // size = 1

	fib_o_t chrt_data[1];

	// field_info_a
	array_part_t fiat_fib_ap; // size = 16

	fib_o_t fiat_ft[8]; // field_type
	fib_o_t fiat_do[8]; // data_offset

	// field_info_b
	array_part_t fibt_fib_ap; // size = 9

	fib_o_t fibt_ft   [8]; // field_type
	fib_o_t fibt_flags[1]; // flags

	// dict header
	array_part_t dht_fib_ap; // size = 24

	fib_o_t dht_fb [8]; // first_block
	fib_o_t dht_ekv[8]; // empty_key_v
	fib_o_t dht_pown[8]; // empty_key_v.prev_owner

	// dict block
	array_part_t dbt_fib_ap; // size = 33

	fib_o_t dbt_eq  [8]; // equal
	fib_o_t dbt_uneq[8]; // unequal
	fib_o_t dbt_val [8]; // value
	fib_o_t dbt_keyp[1]; // key_part
	fib_o_t dbt_pown[8]; // value.prev_owner

	// array
	array_part_t art_fib_ap; // size = 16

	fib_o_t art_next[8]; // next
	fib_o_t art_cap [8]; // capacity
} f_info_b_page_t;

typedef PTA_STRUCT type_dicts {
	refc_t df_refc;
	dict_t df;
	void * df_pown;
	refc_t sf_refc;
	dict_t sf;
	void * sf_pown;
} type_dicts_t;

typedef PTA_STRUCT dict_header_page {
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

context_t pta_setup(){
	pta_pages = 5;
	root_page_t * rp = mmap(NULL, pta_pages * page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	f_info_a_page_t    * fiap = (f_info_a_page_t    *)(PP(rp).s   + page_size);
	f_info_b_page_t    * fibp = (f_info_b_page_t    *)(PP(fiap).s + page_size);
	dict_header_page_t * dhp  = (dict_header_page_t *)(PP(fibp).s + page_size);

	set_page_descriptor(PP((void *)rp),   &rp->header);
	set_page_descriptor(PP((void *)fiap), &fiap->header);
	set_page_descriptor(PP((void *)fibp), &fibp->header);
	set_page_descriptor(PP((void *)dhp),  &dhp->header);

	// ROOT TYPE PAGE
	if (true){
		// ROOT TYPE
		rp->rt_refc = &rp->rt_refc;
		rp->rt = (type_t){
			&fiap->rt_fia_ap, &fibp->rt_fib_ap,
			sizeof(type_t), 1, // own offset--------------------------- TO WATCH
			0, 1, // computations later done
			&dhp->rt.df_refc, &dhp->rt.sf_refc,
			rp, NULL,
			TYPE_INTERNAL | TYPE_ROOT
		};
		rp->rt.paged_size = compute_paged_size((&rp->rt));

		prepare_page_desc((page_desc_t *)rp, &rp->rt, NULL, 1, PAGE_BASIC);

		// SIZE TYPE
		rp->szt_refc = &rp->szt_refc;
		rp->szt = (type_t){
			&fiap->szt_fia_ap, &fibp->szt_fib_ap,
			sizeof(size_t), 0, // own offsets -------------------------- TO WATCH
			0, 1, // computations later done
			&dhp->szt.df_refc, &dhp->szt.sf_refc,
			NULL, NULL, // no pages
			TYPE_PRIMITIVE | TYPE_INTERNAL
		};
		rp->szt.paged_size = compute_paged_size((&rp->szt));

		// CHAR TYPE
		rp->chrt_refc = &rp->chrt_refc;
		rp->chrt = (type_t){
			&fiap->chrt_fia_ap, &fibp->chrt_fib_ap,
			sizeof(uint8_t), 0, // own offsets -------------------------- TO WATCH
			0, 1, // computations later done
			&dhp->chrt.df_refc, &dhp->chrt.sf_refc,
			NULL, NULL, // no pages
			TYPE_PRIMITIVE | TYPE_INTERNAL | TYPE_CHAR
		};
		rp->chrt.paged_size = compute_paged_size((&rp->chrt));

		// FIAT TYPE
		rp->fiat_refc = &rp->fiat_refc;
		rp->fiat = (type_t){
			&fiap->fiat_fia_ap, &fibp->fiat_fib_ap,
			sizeof(field_info_a_t), 1, // own offset--------------------- TO WATCH
			0, 1, // computations later done
			&dhp->fiat.df_refc, &dhp->fiat.sf_refc,
			fiap, NULL,
			TYPE_INTERNAL
		};
		rp->fiat.paged_size = compute_paged_size((&rp->fiat));

		prepare_page_desc((page_desc_t *)fiap, &rp->fiat, NULL, 1, PAGE_ARRAY | PAGE_DEPENDENT);

		// FIBT TYPE
		rp->fibt_refc = &rp->fibt_refc;
		rp->fibt = (type_t){
			&fiap->fibt_fia_ap, &fibp->fibt_fib_ap,
			sizeof(field_info_b_t), 1, // own offset--------------------- TO WATCH
			0, 1, // computations later done
			&dhp->fibt.df_refc, &dhp->fibt.sf_refc,
			fibp, NULL,
			TYPE_INTERNAL | TYPE_FIB
		};
		rp->fibt.paged_size = compute_paged_size((&rp->fibt));

		prepare_page_desc((page_desc_t *)fibp, &rp->fibt, NULL, 1, PAGE_ARRAY | PAGE_DEPENDENT);

		// DHT TYPE
		rp->dht_refc = &rp->dht_refc;
		rp->dht = (type_t){
			&fiap->dht_fia_ap, &fibp->dht_fib_ap,
			sizeof(dict_t) + sizeof(void *) /* pown */, 1, // own offset---------------------------- TO WATCH
			0, 1, // computations later done
			&dhp->dht.df_refc, &dhp->dht.sf_refc,
			dhp, NULL,
			TYPE_INTERNAL
		};
		rp->dht.paged_size = compute_paged_size((&rp->dht));

		prepare_page_desc((page_desc_t *)dhp, &rp->dht, NULL, 1, PAGE_DEPENDENT);

		// DBT TYPE
		rp->dbt_refc = &rp->dbt_refc;
		rp->dbt = (type_t){
			&fiap->dbt_fia_ap, &fibp->dbt_fib_ap,
			sizeof(dict_block_t) + sizeof(void *) /* pown */, 1, // own offset---------------------- TO WATCH
			0, 1, // computations later done
			&dhp->dbt.df_refc, &dhp->dbt.sf_refc,
			NULL, NULL, // no page yet
			TYPE_INTERNAL
		};
		rp->dbt.paged_size = compute_paged_size((&rp->dbt));

		// ARRAY TYPE
		rp->art_refc = &rp->art_refc;
		rp->art = (type_t){
			&fiap->art_fia_ap, &fibp->art_fib_ap,
			16, 1, // own offset---------------------- TO WATCH
			0, 1, // computations later done
			&dhp->art.df_refc, &dhp->art.sf_refc,
			NULL, NULL, // no page yet
			TYPE_INTERNAL | TYPE_ARRAY
		};
		rp->art.paged_size = compute_paged_size((&rp->art));
	}
	// FIA PAGE
	if (true){
		// refc, size, following free space, next part

		fiap->rt_fia_ap = (array_part_t){ &rp->rt_refc, &fiap->szt_fia_ap, 0 };
		fiap->szt_fia_ap = (array_part_t){ &rp->szt_refc, &fiap->chrt_fia_ap, 0 };
		fiap->chrt_fia_ap = (array_part_t){ &rp->chrt_refc, &fiap->fiat_fia_ap, 0 };
		fiap->fiat_fia_ap = (array_part_t){ &rp->fiat_refc, &fiap->fibt_fia_ap, 0 };
		fiap->fibt_fia_ap = (array_part_t){ &rp->fibt_refc, &fiap->dht_fia_ap, 0 };
		fiap->dht_fia_ap = (array_part_t){ &rp->dht_refc, &fiap->dbt_fia_ap, 0 };
		fiap->dbt_fia_ap = (array_part_t){ &rp->dbt_refc, &fiap->art_fia_ap, 0 };
		fiap->art_fia_ap = (array_part_t){ &rp->art_refc, NULL, 0 };
	}
	// FIB PAGE
	if (true){
		// refc, size, following free space, next part
		// field_type, pointer_to_dependent
		field_info_b_t goback = { GO_BACK, FIBF_BASIC };

		// ROOT TYPE
		fibp->rt_fib_ap = (array_part_t){ &rp->rt_refc, &fibp->szt_fib_ap, 81 };

		fibp->rt_dfia [0].fib = (field_info_b_t){ &rp->fiat, FIBF_DEPENDENT | FIBF_ARRAY };
		fibp->rt_dfib [0].fib = (field_info_b_t){ &rp->fibt, FIBF_DEPENDENT | FIBF_ARRAY };
		fibp->rt_objsz[0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC };
		fibp->rt_ofs  [0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC };
		fibp->rt_pgsz [0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC };
		fibp->rt_pglim[0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC };
		fibp->rt_dynf [0].fib = (field_info_b_t){ &rp->dht, FIBF_DEPENDENT };
		fibp->rt_statf[0].fib = (field_info_b_t){ &rp->dht, FIBF_DEPENDENT };
		fibp->rt_pgl  [0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC };
		fibp->rt_cdat [0].fib = (field_info_b_t){ NULL, FIBF_BASIC }; // set by the client
		fibp->rt_flags[0].fib = (field_info_b_t){ &rp->chrt, FIBF_BASIC };

		for (int i = 1; i < 8; i++) fibp->rt_dfia [i].fib = goback;
		for (int i = 1; i < 8; i++) fibp->rt_dfib [i].fib = goback;
		for (int i = 1; i < 8; i++) fibp->rt_objsz[i].fib = goback;
		for (int i = 1; i < 8; i++) fibp->rt_ofs  [i].fib = goback;
		for (int i = 1; i < 8; i++) fibp->rt_pgsz [i].fib = goback;
		for (int i = 1; i < 8; i++) fibp->rt_pglim[i].fib = goback;
		for (int i = 1; i < 8; i++) fibp->rt_dynf [i].fib = goback;
		for (int i = 1; i < 8; i++) fibp->rt_statf[i].fib = goback;
		for (int i = 1; i < 8; i++) fibp->rt_pgl  [i].fib = goback;
		for (int i = 1; i < 8; i++) fibp->rt_cdat [i].fib = goback;
		// rt_flags is only 1 byte


		// SIZE TYPE
		fibp->szt_fib_ap = (array_part_t){ &rp->szt_refc, &fibp->chrt_fib_ap, 8 };

		fibp->szt_data[0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC };

		for (int i = 1; i < 8; i++) fibp->szt_data[i].fib = goback;

		// CHAR TYPE
		fibp->chrt_fib_ap = (array_part_t){ &rp->chrt_refc, &fibp->fiat_fib_ap, 1 };

		fibp->chrt_data[0].fib = (field_info_b_t){ &rp->chrt, FIBF_BASIC };

		// FIAT TYPE
		fibp->fiat_fib_ap = (array_part_t){ &rp->fiat_refc, &fibp->fibt_fib_ap, 16 };
		
		fibp->fiat_ft[0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC }; // field_type
		fibp->fiat_do[0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC }; // data offset
		for (int i = 1; i < 8; i++) fibp->fiat_ft[i].fib = goback;
		for (int i = 1; i < 8; i++) fibp->fiat_do[i].fib = goback;
		
		// FIBT TYPE
		fibp->fibt_fib_ap = (array_part_t){ &rp->fibt_refc, &fibp->dht_fib_ap, 9 };
		
		fibp->fibt_ft   [0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC }; // field_type
		fibp->fibt_flags[0].fib = (field_info_b_t){ &rp->chrt, FIBF_BASIC }; // flags
		for (int i = 1; i < 8; i++) fibp->fibt_ft[i].fib = goback;
		// fibt_flags is only 1 byte
		
		// DICT HEADER TYPE
		fibp->dht_fib_ap = (array_part_t){ &rp->dht_refc, &fibp->dbt_fib_ap, 24 };
		
		fibp->dht_fb [0].fib = (field_info_b_t){ &rp->dbt, FIBF_DEPENDENT }; // first_block
		fibp->dht_ekv[0].fib = (field_info_b_t){ &rp->szt, FIBF_REFERENCES }; // empty_key_v
		fibp->dht_pown[0].fib = (field_info_b_t){ (void *)8,  FIBF_PREV_OWNER }; // pown
		for (int i = 1; i < 8; i++) fibp->dht_fb [i].fib = goback;
		for (int i = 1; i < 8; i++) fibp->dht_ekv[i].fib = goback;
		
		// DICT BLOCK TYPE
		fibp->dbt_fib_ap = (array_part_t){ &rp->dbt_refc, &fibp->art_fib_ap, 33 };
		
		fibp->dbt_eq  [0].fib = (field_info_b_t){ &rp->dbt,  FIBF_DEPENDENT }; // equal
		fibp->dbt_uneq[0].fib = (field_info_b_t){ &rp->dbt,  FIBF_DEPENDENT }; // unequal
		fibp->dbt_val [0].fib = (field_info_b_t){ &rp->szt,  FIBF_REFERENCES }; // value
		fibp->dbt_keyp[0].fib = (field_info_b_t){ &rp->chrt, FIBF_BASIC }; // key_part
		fibp->dbt_pown[0].fib = (field_info_b_t){ (void *)16,  FIBF_PREV_OWNER }; // pown
		for (int i = 1; i < 8; i++) fibp->dbt_eq  [i].fib = goback;
		for (int i = 1; i < 8; i++) fibp->dbt_uneq[i].fib = goback;
		for (int i = 1; i < 8; i++) fibp->dbt_val [i].fib = goback;
		// would be useless to set goback on pown
		
		// ARRAY TYPE
		fibp->art_fib_ap = (array_part_t){ &rp->art_refc, NULL, 16 };
		
		fibp->art_next[0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC }; // next
		fibp->art_cap [0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC }; // capacity
		for (int i = 1; i < 8; i++) fibp->art_next[i].fib = goback;
		for (int i = 1; i < 8; i++) fibp->art_cap [i].fib = goback;
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

	context_t ctx = { &rp->rt, &rp->szt, &rp->chrt, &rp->dht };

	// FIB INIT
	if (true){
		// offset = fib + type.offsets
#define FIBP(abc) &fibp->abc
		// rt
		void * df = rp->rt.dynamic_fields;
		pta_dict_set_pa(df, const_array("dfia"),            FIBP(rt_dfia));
		pta_dict_set_pa(df, const_array("dfib"),            FIBP(rt_dfib));
		pta_dict_set_pa(df, const_array("object_size"),     FIBP(rt_objsz));
		pta_dict_set_pa(df, const_array("offsets"),         FIBP(rt_ofs));
		pta_dict_set_pa(df, const_array("paged_size"),      FIBP(rt_pgsz));
		pta_dict_set_pa(df, const_array("page_limit"),      FIBP(rt_pglim));
		pta_dict_set_pa(df, const_array("dynamic_fields"),  FIBP(rt_dynf));
		pta_dict_set_pa(df, const_array("static_fields"),   FIBP(rt_statf));
		pta_dict_set_pa(df, const_array("page_list"),       FIBP(rt_pgl));
		pta_dict_set_pa(df, const_array("flags"),           FIBP(rt_flags));

		// fiat
		df = rp->fiat.dynamic_fields;
		pta_dict_set_pa(df, const_array("field_type"),      FIBP(fiat_ft));
		pta_dict_set_pa(df, const_array("data_offset"),     FIBP(fiat_do));

		// fibt
		df = rp->fibt.dynamic_fields;
		pta_dict_set_pa(df, const_array("field_type"),      FIBP(fibt_ft));
		pta_dict_set_pa(df, const_array("flags"),           FIBP(fibt_flags));

		// dht
		df = rp->dht.dynamic_fields;
		pta_dict_set_pa(df, const_array("first_block"),     FIBP(dht_fb));
		pta_dict_set_pa(df, const_array("empty_key_v"),     FIBP(dht_ekv));

		// dbt
		df = rp->dbt.dynamic_fields;
		pta_dict_set_pa(df, const_array("equal"),           FIBP(dbt_eq));
		pta_dict_set_pa(df, const_array("unequal"),         FIBP(dbt_uneq));
		pta_dict_set_pa(df, const_array("value"),           FIBP(dbt_val));
		pta_dict_set_pa(df, const_array("key_part"),        FIBP(dbt_keyp));

		// array
		df = rp->art.dynamic_fields;
		pta_dict_set_pa(df, const_array("capacity"),        FIBP(art_cap));
	}

	return ctx;
}

void pta_for_each_type(type_t * root_type, pta_for_each_type_callback cb, void * arg){
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