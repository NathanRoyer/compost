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
// 	page_header_t header;

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

// 	refc_t pglt_refc; // page list type
// 	type_t pglt;

// 	refc_t dht_refc; // dict header type
// 	type_t dht;

// 	refc_t dbt_refc; // dict block type
// 	type_t dbt;

// 	refc_t art_refc; // array_type
// 	type_t art;

// 	refc_t free;
// } root_page_t;

typedef PTA_STRUCT f_info_a_page {
	page_header_t header;

	array_part_t rt_fia_ap; // size = 0
	array_part_t szt_fia_ap; // size = 0
	array_part_t chrt_fia_ap; // size = 0
	array_part_t fiat_fia_ap; // size = 0
	array_part_t fibt_fia_ap; // size = 0
	array_part_t pglt_fia_ap; // size = 0
	array_part_t dht_fia_ap; // size = 0
	array_part_t dbt_fia_ap; // size = 0
	array_part_t art_fia_ap; // size = 0

	array_part_t free;
} f_info_a_page_t;

typedef PTA_STRUCT {
	uint8_t offset_fib;
	field_info_b_t fib;
} fib_o_t;

typedef PTA_STRUCT f_info_b_page {
	page_header_t header;

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

	// page list
	array_part_t pglt_fib_ap; // size = 16

	fib_o_t pglt_next[8]; // field_type
	fib_o_t pglt_fi  [8]; // data_offset

	// dict header
	array_part_t dht_fib_ap; // size = 16

	fib_o_t dht_fb [8]; // first_block
	fib_o_t dht_ekv[8]; // empty_key_v

	// dict block
	array_part_t dbt_fib_ap; // size = 25

	fib_o_t dbt_eq  [8]; // equal
	fib_o_t dbt_uneq[8]; // unequal
	fib_o_t dbt_val [8]; // value
	fib_o_t dbt_keyp[1]; // key_part

	// array
	array_part_t art_fib_ap; // size = 24

	fib_o_t art_sz  [8]; // size
	fib_o_t art_ffsp[8]; // following_free_space
	fib_o_t art_next[8]; // next_part

	array_part_t free;
} f_info_b_page_t;

typedef PTA_STRUCT refc_pgl {
	refc_t refc;
	void * next;
	void * first_instance;
} refc_pgl_t;

typedef PTA_STRUCT page_list_page {
	page_header_t header;

	refc_pgl_t rt_p;
	refc_pgl_t fiat_p;
	refc_pgl_t fibt_p;
	refc_pgl_t pglt_p;
	refc_pgl_t dht_p;

	refc_t free;
} page_list_page_t;

typedef PTA_STRUCT dict_header_page {
	page_header_t header;

	refc_t rt_df_refc;
	dict_t rt_df;
	refc_t rt_sf_refc;
	dict_t rt_sf;

	refc_t szt_df_refc;
	dict_t szt_df;
	refc_t szt_sf_refc;
	dict_t szt_sf;

	refc_t chrt_df_refc;
	dict_t chrt_df;
	refc_t chrt_sf_refc;
	dict_t chrt_sf;

	refc_t fiat_df_refc;
	dict_t fiat_df;
	refc_t fiat_sf_refc;
	dict_t fiat_sf;

	refc_t fibt_df_refc;
	dict_t fibt_df;
	refc_t fibt_sf_refc;
	dict_t fibt_sf;

	refc_t pglt_df_refc;
	dict_t pglt_df;
	refc_t pglt_sf_refc;
	dict_t pglt_sf;

	refc_t dht_df_refc;
	dict_t dht_df;
	refc_t dht_sf_refc;
	dict_t dht_sf;

	refc_t dbt_df_refc;
	dict_t dbt_df;
	refc_t dbt_sf_refc;
	dict_t dbt_sf;

	refc_t art_df_refc;
	dict_t art_df;
	refc_t art_sf_refc;
	dict_t art_sf;

	refc_t free;
} dict_header_page_t;

context_t pta_setup(){
	pta_pages = 5;
	root_page_t * rp = mmap(NULL, pta_pages * pta_sys_page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	f_info_a_page_t * fiap = PG_NEXT(rp);
	f_info_b_page_t * fibp = PG_NEXT(fiap);
	page_list_page_t * pglp = PG_NEXT(fibp);
	dict_header_page_t * dhp = PG_NEXT(pglp);

	// ROOT TYPE PAGE
	if (true){
		rp->header = (page_header_t){ &rp->rt, PAGE_BASIC };

		// ROOT TYPE
		rp->rt_refc = (void *) 1; // ---------------------------------- TO WATCH
		rp->rt = (type_t){
			&fiap->rt_fia_ap, &fibp->rt_fib_ap,
			sizeof(type_t), 1, // own offset--------------------------- TO WATCH
			0, 0, // computations later done
			&dhp->rt_df_refc, &dhp->rt_sf_refc,
			&pglp->rt_p, NULL,
			TYPE_INTERNAL | TYPE_ROOT
		};
		rp->rt.paged_size = compute_paged_size((&rp->rt));
		rp->rt.page_limit = compute_page_limit((&rp->rt));

		// SIZE TYPE
		rp->szt_refc = (void *) 1; // ---------------------------------- TO WATCH
		rp->szt = (type_t){
			&fiap->szt_fia_ap, &fibp->szt_fib_ap,
			sizeof(size_t), 0, // own offsets -------------------------- TO WATCH
			0, 0, // computations later done
			&dhp->szt_df_refc, &dhp->szt_sf_refc,
			NULL, NULL, // no pages
			TYPE_PRIMITIVE | TYPE_INTERNAL
		};
		rp->szt.paged_size = compute_paged_size((&rp->szt));
		rp->szt.page_limit = compute_page_limit((&rp->szt));

		// CHAR TYPE
		rp->chrt_refc = (void *) 1; // ---------------------------------- TO WATCH
		rp->chrt = (type_t){
			&fiap->chrt_fia_ap, &fibp->chrt_fib_ap,
			sizeof(uint8_t), 0, // own offsets -------------------------- TO WATCH
			0, 0, // computations later done
			&dhp->chrt_df_refc, &dhp->chrt_sf_refc,
			NULL, NULL, // no pages
			TYPE_PRIMITIVE | TYPE_INTERNAL | TYPE_CHAR
		};
		rp->chrt.paged_size = compute_paged_size((&rp->chrt));
		rp->chrt.page_limit = compute_page_limit((&rp->chrt));

		// FIAT TYPE
		rp->fiat_refc = (void *) 1; // ---------------------------------- TO WATCH
		rp->fiat = (type_t){
			&fiap->fiat_fia_ap, &fibp->fiat_fib_ap,
			sizeof(field_info_a_t), 1, // own offset--------------------- TO WATCH
			0, 0, // computations later done
			&dhp->fiat_df_refc, &dhp->fiat_sf_refc,
			&pglp->fiat_p, NULL,
			TYPE_INTERNAL
		};
		rp->fiat.paged_size = compute_paged_size((&rp->fiat));
		rp->fiat.page_limit = compute_page_limit((&rp->fiat));

		// FIBT TYPE
		rp->fibt_refc = (void *) 1; // ---------------------------------- TO WATCH
		rp->fibt = (type_t){
			&fiap->fibt_fia_ap, &fibp->fibt_fib_ap,
			sizeof(field_info_b_t), 1, // own offset--------------------- TO WATCH
			0, 0, // computations later done
			&dhp->fibt_df_refc, &dhp->fibt_sf_refc,
			&pglp->fibt_p, NULL,
			TYPE_INTERNAL
		};
		rp->fibt.paged_size = compute_paged_size((&rp->fibt));
		rp->fibt.page_limit = compute_page_limit((&rp->fibt));

		// PGL TYPE
		rp->pglt_refc = (void *) 1; // ---------------------------------- TO WATCH
		rp->pglt = (type_t){
			&fiap->pglt_fia_ap, &fibp->pglt_fib_ap,
			sizeof(page_list_t), 1, // own offset------------------------ TO WATCH
			0, 0, // computations later done
			&dhp->pglt_df_refc, &dhp->pglt_sf_refc,
			&pglp->pglt_p, NULL,
			TYPE_INTERNAL
		};
		rp->pglt.paged_size = compute_paged_size((&rp->pglt));
		rp->pglt.page_limit = compute_page_limit((&rp->pglt));

		// DHT TYPE
		rp->dht_refc = (void *) 1; // ---------------------------------- TO WATCH
		rp->dht = (type_t){
			&fiap->dht_fia_ap, &fibp->dht_fib_ap,
			sizeof(dict_t), 1, // own offset---------------------------- TO WATCH
			0, 0, // computations later done
			&dhp->dht_df_refc, &dhp->dht_sf_refc,
			&pglp->dht_p, NULL,
			TYPE_INTERNAL
		};
		rp->dht.paged_size = compute_paged_size((&rp->dht));
		rp->dht.page_limit = compute_page_limit((&rp->dht));

		// DBT TYPE
		rp->dbt_refc = (void *) 1; // ---------------------------------- TO WATCH
		rp->dbt = (type_t){
			&fiap->dbt_fia_ap, &fibp->dbt_fib_ap,
			sizeof(dict_block_t), 1, // own offset---------------------- TO WATCH
			0, 0, // computations later done
			&dhp->dbt_df_refc, &dhp->dbt_sf_refc,
			NULL, NULL, // no page yet
			TYPE_INTERNAL
		};
		rp->dbt.paged_size = compute_paged_size((&rp->dbt));
		rp->dbt.page_limit = compute_page_limit((&rp->dbt));

		// ARRAY TYPE
		rp->art_refc = (void *) 1; // ---------------------------------- TO WATCH
		rp->art = (type_t){
			&fiap->art_fia_ap, &fibp->art_fib_ap,
			24, 1, // own offset---------------------- TO WATCH
			0, 0, // computations later done
			&dhp->art_df_refc, &dhp->art_sf_refc,
			NULL, NULL, // no page yet
			TYPE_INTERNAL | TYPE_ARRAY
		};
		rp->art.paged_size = compute_paged_size((&rp->art));
		rp->art.page_limit = compute_page_limit((&rp->art));

		for (void * refc = &rp->free; PG_REL(refc) < rp->rt.page_limit; refc += rp->rt.paged_size){
			*(void **)refc = NULL;
		}
	}
	// FIA PAGE
	if (true){
		fiap->header = (page_header_t){ &rp->fiat, PAGE_DEPENDENT | PAGE_ARRAY };

		// refc, size, following free space, next part

		fiap->rt_fia_ap = (array_part_t){ &rp->rt_refc, 0, 0, NULL };
		fiap->szt_fia_ap = (array_part_t){ &rp->szt_refc, 0, 0, NULL };
		fiap->chrt_fia_ap = (array_part_t){ &rp->chrt_refc, 0, 0, NULL };
		fiap->fiat_fia_ap = (array_part_t){ &rp->fiat_refc, 0, 0, NULL };
		fiap->fibt_fia_ap = (array_part_t){ &rp->fibt_refc, 0, 0, NULL };
		fiap->pglt_fia_ap = (array_part_t){ &rp->pglt_refc, 0, 0, NULL };
		fiap->dht_fia_ap = (array_part_t){ &rp->dht_refc, 0, 0, NULL };
		fiap->dbt_fia_ap = (array_part_t){ &rp->dbt_refc, 0, 0, NULL };
		fiap->art_fia_ap = (array_part_t){ &rp->art_refc, 0, 0, NULL };

		fiap->free = (array_part_t){ NULL, 0, pta_sys_page_size - PG_REL((&fiap->free)) - sizeof(array_part_t), NULL };
	}
	// FIB PAGE
	if (true){
		fibp->header = (page_header_t){ &rp->fibt, PAGE_DEPENDENT | PAGE_ARRAY };

		// refc, size, following free space, next part
		// field_type, pointer_to_dependent
		field_info_b_t goback = { GO_BACK, FIBF_BASIC };

		// ROOT TYPE
		fibp->rt_fib_ap = (array_part_t){ &rp->rt_refc, 81, 0, NULL };

		fibp->rt_dfia [0].fib = (field_info_b_t){ &rp->fiat, FIBF_DEPENDENT | FIBF_POINTER | FIBF_ARRAY };
		fibp->rt_dfib [0].fib = (field_info_b_t){ &rp->fibt, FIBF_DEPENDENT | FIBF_POINTER | FIBF_ARRAY };
		fibp->rt_objsz[0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC };
		fibp->rt_ofs  [0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC };
		fibp->rt_pgsz [0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC };
		fibp->rt_pglim[0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC };
		fibp->rt_dynf [0].fib = (field_info_b_t){ &rp->dht, FIBF_DEPENDENT | FIBF_POINTER };
		fibp->rt_statf[0].fib = (field_info_b_t){ &rp->dht, FIBF_DEPENDENT | FIBF_POINTER };
		fibp->rt_pgl  [0].fib = (field_info_b_t){ &rp->pglt, FIBF_DEPENDENT | FIBF_POINTER };
		fibp->rt_cdat [0].fib = (field_info_b_t){ UNDEFINED, FIBF_BASIC }; // set by the client
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
		fibp->szt_fib_ap = (array_part_t){ &rp->szt_refc, 8, 0, NULL };

		fibp->szt_data[0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC };

		for (int i = 1; i < 8; i++) fibp->szt_data[i].fib = goback;

		// CHAR TYPE
		fibp->chrt_fib_ap = (array_part_t){ &rp->chrt_refc, 1, 0, NULL };

		fibp->chrt_data[0].fib = (field_info_b_t){ &rp->chrt, FIBF_BASIC };

		// FIAT TYPE
		fibp->fiat_fib_ap = (array_part_t){ &rp->fiat_refc, 16, 0, NULL };
		
		fibp->fiat_ft[0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC }; // field_type
		fibp->fiat_do[0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC }; // data offset
		for (int i = 1; i < 8; i++) fibp->fiat_ft[i].fib = goback;
		for (int i = 1; i < 8; i++) fibp->fiat_do[i].fib = goback;
		
		// FIBT TYPE
		fibp->fibt_fib_ap = (array_part_t){ &rp->fibt_refc, 9, 0, NULL };
		
		fibp->fibt_ft   [0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC }; // field_type
		fibp->fibt_flags[0].fib = (field_info_b_t){ &rp->chrt, FIBF_BASIC }; // flags
		for (int i = 1; i < 8; i++) fibp->fibt_ft[i].fib = goback;
		// fibt_flags is only 1 byte
		
		// PAGE LIST TYPE
		fibp->pglt_fib_ap = (array_part_t){ &rp->pglt_refc, 16, 0, NULL };
		
		fibp->pglt_next[0].fib = (field_info_b_t){ &rp->pglt, FIBF_DEPENDENT | FIBF_POINTER }; // next
		fibp->pglt_fi  [0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC }; // first_instance
		for (int i = 1; i < 8; i++) fibp->pglt_next[i].fib = goback;
		for (int i = 1; i < 8; i++) fibp->pglt_fi  [i].fib = goback;
		
		// DICT HEADER TYPE
		fibp->dht_fib_ap = (array_part_t){ &rp->dht_refc, 16, 0, NULL };
		
		fibp->dht_fb [0].fib = (field_info_b_t){ &rp->dbt, FIBF_DEPENDENT | FIBF_POINTER }; // first_block
		fibp->dht_ekv[0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC }; // empty_key_v
		for (int i = 1; i < 8; i++) fibp->dht_fb [i].fib = goback;
		for (int i = 1; i < 8; i++) fibp->dht_ekv[i].fib = goback;
		
		// DICT BLOCK TYPE
		fibp->dbt_fib_ap = (array_part_t){ &rp->dbt_refc, 25, 0, NULL };
		
		fibp->dbt_eq  [0].fib = (field_info_b_t){ &rp->dbt, FIBF_DEPENDENT | FIBF_POINTER }; // equal
		fibp->dbt_uneq[0].fib = (field_info_b_t){ &rp->dbt, FIBF_DEPENDENT | FIBF_POINTER }; // unequal
		fibp->dbt_val [0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC }; // value
		fibp->dbt_keyp[0].fib = (field_info_b_t){ &rp->chrt, FIBF_BASIC }; // key_part
		for (int i = 1; i < 8; i++) fibp->dbt_eq  [i].fib = goback;
		for (int i = 1; i < 8; i++) fibp->dbt_uneq[i].fib = goback;
		for (int i = 1; i < 8; i++) fibp->dbt_val [i].fib = goback;
		
		// ARRAY TYPE
		fibp->art_fib_ap = (array_part_t){ &rp->art_refc, 24, 0, NULL };
		
		fibp->art_sz  [0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC }; // size
		fibp->art_ffsp[0].fib = (field_info_b_t){ &rp->szt, FIBF_BASIC }; // following_free_space
		fibp->art_next[0].fib = (field_info_b_t){ &rp->art, FIBF_DEPENDENT | FIBF_POINTER }; // next
		for (int i = 1; i < 8; i++) fibp->art_sz  [i].fib = goback;
		for (int i = 1; i < 8; i++) fibp->art_ffsp[i].fib = goback;
		for (int i = 1; i < 8; i++) fibp->art_next[i].fib = goback;

		fibp->free = (array_part_t){ NULL, 0, pta_sys_page_size - PG_REL((&fibp->free)) - sizeof(array_part_t), NULL };
	}
	// PGL PAGE
	if (true){
		pglp->header = (page_header_t){ &rp->pglt, PAGE_DEPENDENT };

		// refc, next, first_instance

		pglp->rt_p = (refc_pgl_t){ &rp->rt_refc, NULL, &rp->rt_refc };
		pglp->fiat_p = (refc_pgl_t){ &rp->fiat_refc, NULL, &fiap->rt_fia_ap };
		pglp->fibt_p = (refc_pgl_t){ &rp->fibt_refc, NULL, &fibp->rt_fib_ap };
		pglp->pglt_p = (refc_pgl_t){ &rp->pglt_refc, NULL, &pglp->rt_p };
		pglp->dht_p = (refc_pgl_t){ &rp->dht_refc, NULL, &dhp->rt_df_refc };

		for (void * refc = &pglp->free; PG_REL(refc) < rp->pglt.page_limit; refc += rp->pglt.paged_size){
			*(void **)refc = NULL;
		}
	}
	// DH PAGE
	if (true){
		dhp->header = (page_header_t){ &rp->dht, PAGE_DEPENDENT };

		dhp->rt_df_refc = &rp->rt_refc;
		dhp->rt_sf_refc = &rp->rt_refc;
		dhp->rt_df = (dict_t){ NULL, NULL };
		dhp->rt_sf = (dict_t){ NULL, NULL };

		dhp->szt_df_refc = &rp->szt_refc;
		dhp->szt_sf_refc = &rp->szt_refc;
		dhp->szt_df = (dict_t){ NULL, NULL };
		dhp->szt_sf = (dict_t){ NULL, NULL };

		dhp->chrt_df_refc = &rp->chrt_refc;
		dhp->chrt_sf_refc = &rp->chrt_refc;
		dhp->chrt_df = (dict_t){ NULL, NULL };
		dhp->chrt_sf = (dict_t){ NULL, NULL };

		dhp->fiat_df_refc = &rp->fiat_refc;
		dhp->fiat_sf_refc = &rp->fiat_refc;
		dhp->fiat_df = (dict_t){ NULL, NULL };
		dhp->fiat_sf = (dict_t){ NULL, NULL };

		dhp->fibt_df_refc = &rp->fibt_refc;
		dhp->fibt_sf_refc = &rp->fibt_refc;
		dhp->fibt_df = (dict_t){ NULL, NULL };
		dhp->fibt_sf = (dict_t){ NULL, NULL };

		dhp->pglt_df_refc = &rp->pglt_refc;
		dhp->pglt_sf_refc = &rp->pglt_refc;
		dhp->pglt_df = (dict_t){ NULL, NULL };
		dhp->pglt_sf = (dict_t){ NULL, NULL };

		dhp->dht_df_refc = &rp->dht_refc;
		dhp->dht_sf_refc = &rp->dht_refc;
		dhp->dht_df = (dict_t){ NULL, NULL };
		dhp->dht_sf = (dict_t){ NULL, NULL };

		dhp->dbt_df_refc = &rp->dbt_refc;
		dhp->dbt_sf_refc = &rp->dbt_refc;
		dhp->dbt_df = (dict_t){ NULL, NULL };
		dhp->dbt_sf = (dict_t){ NULL, NULL };

		dhp->art_df_refc = &rp->art_refc;
		dhp->art_sf_refc = &rp->art_refc;
		dhp->art_df = (dict_t){ NULL, NULL };
		dhp->art_sf = (dict_t){ NULL, NULL };

		for (void * refc = &dhp->free; PG_REL(refc) < rp->dht.page_limit; refc += rp->dht.paged_size){
			*(void **)refc = NULL;
		}
	}

	context_t ctx = { &rp->rt, &rp->szt, &rp->chrt, &rp->dht };

	// FIB INIT
	if (true){
		// offset = fib + type.offsets

		// rt
		void * df = rp->rt.dynamic_fields;
		size_t i = rp->rt.offsets;
		pta_dict_set_pa(df, const_array("dfia"),            (void *)( i )); i += 8;
		pta_dict_set_pa(df, const_array("dfib"),            (void *)( i )); i += 8;
		pta_dict_set_pa(df, const_array("object_size"),     (void *)( i )); i += 8;
		pta_dict_set_pa(df, const_array("offsets"),         (void *)( i )); i += 8;
		pta_dict_set_pa(df, const_array("paged_size"),      (void *)( i )); i += 8;
		pta_dict_set_pa(df, const_array("page_limit"),      (void *)( i )); i += 8;
		pta_dict_set_pa(df, const_array("dynamic_fields"),  (void *)( i )); i += 8;
		pta_dict_set_pa(df, const_array("static_fields"),   (void *)( i )); i += 8;
		pta_dict_set_pa(df, const_array("page_list"),       (void *)( i )); i += 8;
		pta_dict_set_pa(df, const_array("client_data"),     (void *)( i )); i += 8;
		pta_dict_set_pa(df, const_array("flags"),           (void *)( i )); // i += 1;

		// fiat
		df = rp->fiat.dynamic_fields;
		i = rp->fiat.offsets;
		pta_dict_set_pa(df, const_array("field_type"),      (void *)( i )); i += 8;
		pta_dict_set_pa(df, const_array("data_offset"),     (void *)( i )); // i += 8;

		// fibt
		df = rp->fibt.dynamic_fields;
		i = rp->fibt.offsets;
		pta_dict_set_pa(df, const_array("field_type"),      (void *)( i )); i += 8;
		pta_dict_set_pa(df, const_array("flags"),           (void *)( i )); // i += 1;

		// pglt
		df = rp->pglt.dynamic_fields;
		i = rp->pglt.offsets;
		pta_dict_set_pa(df, const_array("next"),            (void *)( i )); i += 8;
		pta_dict_set_pa(df, const_array("first_instance"),  (void *)( i )); // i += 8;

		// dht
		df = rp->dht.dynamic_fields;
		i = rp->dht.offsets;
		pta_dict_set_pa(df, const_array("first_block"),     (void *)( i )); i += 8;
		pta_dict_set_pa(df, const_array("empty_key_v"),     (void *)( i )); // i += 8;

		// dbt
		df = rp->dbt.dynamic_fields;
		i = rp->dbt.offsets;
		pta_dict_set_pa(df, const_array("equal"),           (void *)( i )); i += 8;
		pta_dict_set_pa(df, const_array("unequal"),         (void *)( i )); i += 8;
		pta_dict_set_pa(df, const_array("value"),           (void *)( i )); i += 8;
		pta_dict_set_pa(df, const_array("key_part"),        (void *)( i )); // i += 8;

		// array
		df = rp->art.dynamic_fields;
		i = rp->art.offsets;
		pta_dict_set_pa(df, const_array("size"),           (void *)( i )); i += 8;
		pta_dict_set_pa(df, const_array("ffsp"),         (void *)( i )); i += 8;
		pta_dict_set_pa(df, const_array("next"),        (void *)( i )); // i += 8;
	}

	return ctx;
}

void pta_for_each_type(type_t * root_type, pta_for_each_type_callback cb, void * arg){
	page_list_t * pgl = pta_get_c_object(root_type->page_list);
	while (pgl){
		for (void * refc = pgl->first_instance; PG_REL(refc) < root_type->page_limit; refc += root_type->paged_size){
			if (*(void **)refc == NULL) continue;
			cb(refc, arg);
		}
		pgl = pta_get_c_object(pgl->next);
	}
}