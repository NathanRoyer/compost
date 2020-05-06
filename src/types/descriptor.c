/*
 * LibPTA pages descriptors, C source
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
#include "types/descriptor.h"
#include "types/page.h"

size_t reg_mask;
size_t page_relative_bits;
ptr_t first_reg;

size_t reg_i_mask;
size_t reg_i_bits;

size_t reg_md_mask;
size_t reg_md_bits;

size_t reg_part_bits;
size_t reg_last_part_bits;

// called by the constructor in page.c
void compute_regs_config(){
	for (page_relative_bits = 0; page_rel_mask >> page_relative_bits; page_relative_bits++);

	reg_mask = (page_size / sizeof(void *)) - 1;
	for (reg_part_bits = 0; reg_mask >> reg_part_bits; reg_part_bits++);
	reg_last_part_bits = PTR_BITS - ((PTR_BITS - page_relative_bits) % reg_part_bits);
	for (reg_i_bits = 0; (PTR_BITS - 1) >> reg_i_bits; reg_i_bits++);
	if (reg_i_bits >= page_relative_bits){
		printf("Critical error: This system\'s paging configuration is not compatible with LibPTA.\n");
		exit(1);
	}
	reg_md_bits = page_relative_bits - reg_i_bits;
	reg_i_mask = (1 << reg_i_bits) - 1;
	reg_md_mask = (1 << reg_md_bits) - 1;
	first_reg.s = (new_random_page(1).s);
	first_reg.s |= page_relative_bits;
}

ptr_t new_random_page(size_t contig_len){
	return PP(mmap(NULL, page_size * contig_len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
}

/*
 * Registers have some metadata scattered between
 * "next" and 'i' bits. It represents the address
 * which expectedly led the program to this part.
 */
ptr_t get_reg_metadata(ptr_t reg){
	ptr_t metadata = SP(0);
	// printf("get-md %p", reg.p);
	for (size_t i = 0; i < PTR_BITS; i += reg_md_bits){
		// get pointer content as size_t and advance the pointer
		// right shift to skip the 'i' bits
		// AND to keep only the relevant portion
		// left shift to add at the correct portion of metadata
		// printf("   get-md (i=%li) = %p\n", i, (reg.p->s >> reg_i_bits) & reg_md_mask);
		metadata.s |= ((reg.p++->s >> reg_i_bits) & reg_md_mask) << i;
	}
	printf(" : %p\n", metadata.p);
	return metadata;
}

void set_reg_metadata(ptr_t reg, ptr_t metadata){
	// printf("set-md %p = %p\n", reg.p, metadata.p);
	for (size_t i = 0; i < PTR_BITS; i += reg_md_bits){
		// reverse operation
		// printf("   set-md (i=%li) = 0x%x\n", i, (metadata.s >> i) & reg_md_mask);
		reg.p++->s |= ((metadata.s >> i) & reg_md_mask) << reg_i_bits;
	}
}

page_desc_t * locate_page_descriptor(void * address){
	ptr_t reg = SP(first_reg.s & page_mask);
	size_t i = first_reg.s & reg_i_mask;
	while (reg.p != NULL){
		reg = reg.p[(PP(address).s >> i) & reg_mask];
		if (i <= page_relative_bits) break;
		i = reg.s & reg_i_mask;
		reg.s &= page_mask;
	}
	return (page_desc_t *)(reg.s & page_mask);
}

void create_page_descriptor(ptr_t address, page_desc_t * desc){
	ptr_t * reg = &first_reg;
	ptr_t reg_ct = first_reg;
	size_t i = first_reg.s & reg_i_mask;
	ptr_t last_md;
	do {
		i = reg_ct.s & reg_i_mask;
		if (i > page_relative_bits){
			size_t relevant_bits = ((~(size_t)0) >> i) << i;
			last_md = get_reg_metadata(SP(reg_ct.s & page_mask));
			if ((last_md.s & relevant_bits) == (address.s & relevant_bits)){
				reg = &(SP((reg_ct.s & page_mask)).p)[(address.s >> i) & reg_mask];
				reg_ct = *reg;
			} else break;
		} else break;
	} while (reg_ct.p != NULL);

	if (i > page_relative_bits){
		printf("LibPTA : new register (%lu)\n", i);
		if (reg_ct.p != NULL){
			ptr_t bck = reg_ct;
			ptr_t intermediate = new_random_page(1);
			// non-final registers must have metadata:
			set_reg_metadata(intermediate, address);

			for (i = reg_last_part_bits; i >= page_relative_bits; i -= reg_part_bits){
				size_t relevant_bits = ((~(size_t)0) >> i) << i;
				if ((last_md.s & relevant_bits) != (address.s & relevant_bits)) break;
			}

			reg->s &= (reg_md_mask << reg_i_bits);
			reg->s |= (intermediate.s | i);
			intermediate.p[(last_md.s >> i) & reg_mask].s |= bck.s;
			reg = &(intermediate.p[(address.s >> i) & reg_mask]);
		}

		ptr_t final_reg_page = new_random_page(1);

		reg->s &= (reg_md_mask << reg_i_bits);
		reg->s |= (final_reg_page.s | page_relative_bits);
		reg = &(final_reg_page.p[(address.s >> page_relative_bits) & reg_mask]);
	} else {
		reg = &(SP((reg_ct.s & page_mask)).p)[(address.s >> page_relative_bits) & reg_mask];
	}
	reg->s = (size_t)desc;
}