/*
 * LibPTA dictionnary features, C source
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

#include "types/dict.h"

/*
 * This file contains dictionnary-related functions
 *
 */

#define IS_DICT_EMPTY(d) (d->first_block == NULL && d->empty_key_v == NULL)

/* new_dict_block (private function)
 *
 * dict_set uses this function to locate new dictionnary blocks.
 * this function is a sort of constructor for new blocks.
 * Return value: freshly setup dict_block_t
 */
void new_dict_block(void * field, root_page_t * rp, int8_t key_part){
	*(dict_block_t *)pta_get_c_object(pta_spot_dependent(field, &rp->dbt)) = (dict_block_t){ NULL, NULL, NULL, key_part };
}

/* dict_get(dictionnary d, string key)
 *
 * given a key which exists in a dictionnary, you can obtain the value
 * held by the dictionnary at this specific key with this function.
 * Return value: the value corresponding to the key
 */
void * pta_dict_get(void * d_refc, array key){
	dict_t * d = pta_get_c_object(d_refc);
	if (key.length == 0) return d->empty_key_v;
	dict_block_t * dblk = pta_get_c_object(d->first_block);

	for (int i = 0; dblk != NULL; ){
		if (dblk->key_part == char_at(key, i)){
			if (++i == key.length) break;
			else dblk = pta_get_c_object(dblk->equal);
		} else if (dblk->key_part > char_at(key, i)) dblk = NULL;
		else dblk = pta_get_c_object(dblk->unequal);
	}

	return (dblk != NULL && dblk->value != NULL) ? dblk->value : NULL;
}

/* dict_has(dictionnary d, string key)
 *
 * this function tells you if a dictionnary has a value, given a specific key.
 * Return value: boolean
 */
bool pta_dict_has(void * d_refc, array key){
	return pta_dict_get(d_refc, key) == NULL;
}

/* dict_set(context c, dictionnary d, string key, pointer value)
 *
 * This function sets a key-value pair in a dictionnary.
 * Return value: the value parameter
 */
void * pta_dict_set(void * d_refc, array key, void * value){
	pta_increment_refc(d_refc);
	root_page_t * rp = get_root_page(d_refc);

	dict_t * d = pta_get_c_object(d_refc);
	char * key_data = key.data;
	void ** value_holder = NULL;
	if (key.length == 0) value_holder = &d->empty_key_v;
	else {
		if (d->first_block == NULL) new_dict_block(&d->first_block, rp, key_data[0]);
		void ** dblkp = &d->first_block;
		void * dblk_refc = *dblkp;
		dict_block_t * dblk = pta_get_c_object(dblk_refc);

		for (int i = 0; i < key.length && value_holder == NULL; ){
			if (dblk->key_part == key_data[i]){
				if (++i == key.length) value_holder = &dblk->value;
				else {
					// i was just incremented
					if (dblk->equal == NULL) new_dict_block(&dblk->equal, rp, key_data[i]);
					dblkp = &dblk->equal;
					dblk_refc = *dblkp;
					dblk = pta_get_c_object(dblk_refc);
				}
			} else if (dblk->key_part > key_data[i]){
				void * blk = pta_detach_dependent(dblkp);
				pta_protect_detached(blk);
				new_dict_block(dblkp, rp, key_data[i]);
				dblk_refc = *dblkp;
				dblk = pta_get_c_object(dblk_refc);
				pta_attach_dependent(&dblk->unequal, blk);
				;
			} else {
				if (dblk->unequal == NULL) new_dict_block(&dblk->unequal, rp, key_data[i]);
				dblkp = &dblk->unequal;
				dblk_refc = *dblkp;
				dblk = pta_get_c_object(dblk_refc);
			}
		}
	}

	pta_decrement_refc(d_refc);

	return *value_holder = value;
}

/* new_dict_block (private function)
 *
 * This recursive function increment a counter each time it
 * encounters a value in the dictionnary tree.
 * Return value: none
 */
void dict_block_count(void * d_refc, size_t * count){
	dict_block_t * dblk = pta_get_c_object(d_refc);
	if (dblk->value != NULL) (*count)++;
	if (dblk->equal != NULL) dict_block_count(dblk->equal, count);
	if (dblk->unequal != NULL) dict_block_count(dblk->unequal, count);
}

/* dict_count (dictionnary d)
 *
 * This function counts the number of elements in a dictionnary.
 * Return value: the number of elements in d
 */
size_t pta_dict_count(void * d_refc){
	size_t count = 0;
	dict_t * d = pta_get_c_object(d_refc);
	if (d->empty_key_v != NULL) count++;
	if (d->first_block != NULL) dict_block_count(d->first_block, &count);
	return count;
}

/* get_longest_index_block (private function)
 *
 * This recursive function returns the maximum between:
 * - the length of the longest path in the sub-tree of a dict block.
 * - 1 if this block holds a value
 * Return value: the maximum length of pathes in the block sub-tree
 */
int get_longest_index_block(dict_block_t * dblk, int l){
	int l1 = 0, l2 = 0;
	if (dblk->equal != NULL) l2 = get_longest_index_block(pta_get_c_object(dblk->equal), l + 1);
	if (dblk->unequal != NULL) l1 = get_longest_index_block(pta_get_c_object(dblk->unequal), l);
	if (l1 > l || l2 > l) return (l1 > l2 ? l1 : l2);
	else return dblk->value == NULL ? 0 : l;
}

/* get_longest_index_block (dictionnary)
 *
 * This function finds the longest path in the dictionnary tree and returns its length
 * Return value: longest path length
 */
int get_longest_index(dict_t * d){
	int l1 = d->empty_key_v == NULL ? -1 : 0;
	int l2 = l1;
	if (d->first_block != NULL) l2 = get_longest_index_block(pta_get_c_object(d->first_block), 1);
	return l1 > l2 ? l1 : l2;
}

/* fill_index (private function)
 *
 * Recursive function progressively filling a string with the characters
 * forming an index of the dictionnary. The next block to fill the string
 * is guessed by comparison of key characters; if the block reached
 * by the submitted index is reached again, the function will return false
 * until it finds a block which has not been indexed, then the index is
 * changed accordingly.
 * Return value: boolean indicating if an unindexed path has been found.
 */
bool fill_index(dict_block_t * dblk, array * index, int i){
	bool result = false;
	if (index->data[i] < dblk->key_part){
		index->data[i] = dblk->key_part;
		for (int j = i + 1; j < index->length; j++) index->data[j] = '\0';
		if (dblk->value != NULL){
			index->length = i + 1;
			result = true;
		}
	}
	if (!result && index->data[i] == dblk->key_part && dblk->equal != NULL){
		result = fill_index(pta_get_c_object(dblk->equal), index, i + 1);
	}
	if (!result && dblk->unequal != NULL){
		result = fill_index(pta_get_c_object(dblk->unequal), index, i);
	}
	return result;
}

/* get_next_index (dictionnary d, string key)
 * note: key should be { -1, NULL } upon first call
 * note: this function uses libc's malloc and free
 * note: this function is meant to be used while debugging or initializing.
 * note: the dictionnary must not be modified before the key-finding is
 * terminated, or it may raise a segmentation fault.
 *
 * This function assembles a new key at each call until the last key of the
 * dictionnary has been reached. The next call will always set an index
 * higher in the alphabetical order. All the keys are guaranteed to exist
 * in the dictionnary. When all keys have been found, the index is set to
 * { -1, NULL } as it was initially.
 * Return value: none.
 */
void pta_dict_get_next_index(void * d_refc, array * index){
	dict_t * d = pta_get_c_object(d_refc);
	if (index->length == -1){
		index->length = get_longest_index(d);
		if (index->length > 0){
			index->data = malloc(index->length);
			for (int i = 0; i < index->length; i++) index->data[i] = '\0';
			if (d->empty_key_v){
				index->length = 0;
				return;
			}
		} else return;
	}
	if (!fill_index(pta_get_c_object(d->first_block), index, 0)){
		free(index->data);
		index->data = NULL;
		index->length = -1;
	}
}