// Copyright (c) 2012 The Trustees of University of Illinois. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <dmapp.h>
#include <stdlib.h>
#include <string.h>

#include "mpitypes.h"
#include "fompi.h"

/*signatures helper functions*/
static inline int foMPI_Rma_test_wait(void *extra_state, int *flag, MPI_Status *status, int wait);
static int simple_rma_wait(void *extra_state, MPI_Status *status);
static int simple_rma_test(void *extra_state, int *flag, MPI_Status *status);

#ifdef UGNI
static int communication_and_datatype_handling_ugni_put_notify_only(const void* origin_addr,
		int origin_count, MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp,
		int target_count, MPI_Datatype target_datatype, foMPI_Win win, dmapp_pe_t target_pe,
		int tag);
static int communication_and_datatype_handling_ugni_get_notify_only(const void* origin_addr,
		int origin_count, MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp,
		int target_count, MPI_Datatype target_datatype, foMPI_Win win, dmapp_pe_t target_pe,
		int tag);
//static inline int _foMPI_EncodeID(uint16_t rank, uint16_t id_msg, uint32_t *id_encoded);
static inline void ugni_simple_put(dmapp_pe_t target_pe, MPI_Aint target_offset,
		MPI_Aint origin_offset, fompi_seg_desc_t* src_seg_ptr, fompi_seg_desc_t* dst_seg_ptr,
		MPI_Aint size, foMPI_Win win, int tag);
static inline void ugni_simple_get(dmapp_pe_t target_pe, MPI_Aint target_offset,
		MPI_Aint origin_offset, fompi_seg_desc_t* src_seg_ptr, fompi_seg_desc_t* dst_seg_ptr,
		MPI_Aint size, foMPI_Win win, int tag);
static inline void ugni_simple_rma(dmapp_pe_t target_pe, MPI_Aint target_offset,
		MPI_Aint origin_offset, fompi_seg_desc_t* src_seg_ptr, fompi_seg_desc_t* dest_seg_ptr,
		MPI_Aint size, foMPI_Win win, int tag, int operation);
static inline int _foMPI_Rrma_notify(const void *origin_addr, int origin_count,
		MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp, int target_count,
		MPI_Datatype target_datatype, foMPI_Win win, foMPI_Request *request, int tag, int operation);
static inline int _foMPI_Rma_notify(const void *origin_addr, int origin_count,
		MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp, int target_count,
		MPI_Datatype target_datatype, foMPI_Win win, int tag, int operation);
#endif
inline void sse_memcpy(char *to, const char *from, size_t len) {
	unsigned long src = (unsigned long) from;
	unsigned long dst = (unsigned long) to;
	int i;

	/* check alignment */
	if ((src ^ dst) & 0xf)
		goto unaligned;

	if (src & 0xf) {
		int chunk = 0x10 - (src & 0xf);
		if (chunk > len) {
			chunk = len; /* chunk can be more than the actual amount to copy */
		}

		/* copy chunk until next 16-byte  */
		memcpy(to, from, chunk);
		len -= chunk;
		to += chunk;
		from += chunk;
	}

	/*
	 * copy in 256 Byte portions
	 */
	for (i = 0; i < (len & ~0xff); i += 256) {
		asm volatile(
				"movaps 0x0(%0),  %%xmm0\n\t"
				"movaps 0x10(%0), %%xmm1\n\t"
				"movaps 0x20(%0), %%xmm2\n\t"
				"movaps 0x30(%0), %%xmm3\n\t"
				"movaps 0x40(%0), %%xmm4\n\t"
				"movaps 0x50(%0), %%xmm5\n\t"
				"movaps 0x60(%0), %%xmm6\n\t"
				"movaps 0x70(%0), %%xmm7\n\t"
				"movaps 0x80(%0), %%xmm8\n\t"
				"movaps 0x90(%0), %%xmm9\n\t"
				"movaps 0xa0(%0), %%xmm10\n\t"
				"movaps 0xb0(%0), %%xmm11\n\t"
				"movaps 0xc0(%0), %%xmm12\n\t"
				"movaps 0xd0(%0), %%xmm13\n\t"
				"movaps 0xe0(%0), %%xmm14\n\t"
				"movaps 0xf0(%0), %%xmm15\n\t"

				"movaps %%xmm0,  0x0(%1)\n\t"
				"movaps %%xmm1,  0x10(%1)\n\t"
				"movaps %%xmm2,  0x20(%1)\n\t"
				"movaps %%xmm3,  0x30(%1)\n\t"
				"movaps %%xmm4,  0x40(%1)\n\t"
				"movaps %%xmm5,  0x50(%1)\n\t"
				"movaps %%xmm6,  0x60(%1)\n\t"
				"movaps %%xmm7,  0x70(%1)\n\t"
				"movaps %%xmm8,  0x80(%1)\n\t"
				"movaps %%xmm9,  0x90(%1)\n\t"
				"movaps %%xmm10, 0xa0(%1)\n\t"
				"movaps %%xmm11, 0xb0(%1)\n\t"
				"movaps %%xmm12, 0xc0(%1)\n\t"
				"movaps %%xmm13, 0xd0(%1)\n\t"
				"movaps %%xmm14, 0xe0(%1)\n\t"
				"movaps %%xmm15, 0xf0(%1)\n\t"
				: : "r" (from), "r" (to) : "memory");

		from += 256;
		to += 256;
	}

	goto trailer;

	unaligned:
	/*
	 * copy in 256 Byte portions unaligned
	 */
	for (i = 0; i < (len & ~0xff); i += 256) {
		asm volatile(
				"movups 0x0(%0),  %%xmm0\n\t"
				"movups 0x10(%0), %%xmm1\n\t"
				"movups 0x20(%0), %%xmm2\n\t"
				"movups 0x30(%0), %%xmm3\n\t"
				"movups 0x40(%0), %%xmm4\n\t"
				"movups 0x50(%0), %%xmm5\n\t"
				"movups 0x60(%0), %%xmm6\n\t"
				"movups 0x70(%0), %%xmm7\n\t"
				"movups 0x80(%0), %%xmm8\n\t"
				"movups 0x90(%0), %%xmm9\n\t"
				"movups 0xa0(%0), %%xmm10\n\t"
				"movups 0xb0(%0), %%xmm11\n\t"
				"movups 0xc0(%0), %%xmm12\n\t"
				"movups 0xd0(%0), %%xmm13\n\t"
				"movups 0xe0(%0), %%xmm14\n\t"
				"movups 0xf0(%0), %%xmm15\n\t"

				"movups %%xmm0,  0x0(%1)\n\t"
				"movups %%xmm1,  0x10(%1)\n\t"
				"movups %%xmm2,  0x20(%1)\n\t"
				"movups %%xmm3,  0x30(%1)\n\t"
				"movups %%xmm4,  0x40(%1)\n\t"
				"movups %%xmm5,  0x50(%1)\n\t"
				"movups %%xmm6,  0x60(%1)\n\t"
				"movups %%xmm7,  0x70(%1)\n\t"
				"movups %%xmm8,  0x80(%1)\n\t"
				"movups %%xmm9,  0x90(%1)\n\t"
				"movups %%xmm10, 0xa0(%1)\n\t"
				"movups %%xmm11, 0xb0(%1)\n\t"
				"movups %%xmm12, 0xc0(%1)\n\t"
				"movups %%xmm13, 0xd0(%1)\n\t"
				"movups %%xmm14, 0xe0(%1)\n\t"
				"movups %%xmm15, 0xf0(%1)\n\t"
				: : "r" (from), "r" (to) : "memory");

		from += 256;
		to += 256;
	}

	trailer:
	memcpy(to, from, len & 0xff);
}

/* some global variables */

/* some helper functions */
static int find_seg_and_size(foMPI_Win_dynamic_element_t* current, MPI_Aint size,
		MPI_Aint target_offset, fompi_seg_desc_t** seg_ptr, uint64_t* real_size) {

	int found = 0;

	if (current != NULL) {
		if ((target_offset >= current->base) && (target_offset < (current->base + current->size))) {
			found = 1;
		}
		while ((current->next != NULL) && (found == 0)) {
			current = current->next;
			if ((target_offset >= current->base)
					&& (target_offset < (current->base + current->size))) {
				found = 1;
			}
		}
	}

	if (found == 0) {
		return foMPI_ERROR_RMA_WIN_MEM_NOT_FOUND;
	}

	*seg_ptr = &(current->seg);
	if ((target_offset + size) > (current->base + current->size)) {
		*real_size = current->base + current->size - target_offset;
	} else {
		*real_size = size;
	}

	return MPI_SUCCESS;
}

static void get_dynamic_window_regions(dmapp_return_t* status, int target_rank,
		dmapp_pe_t target_pe, foMPI_Win win) {

	int64_t mutex;
	uint32_t remote_dyn_id;
	foMPI_Win_dynamic_element_t* current;
	foMPI_Win_dynamic_element_t* temp;
	int times;
	foMPI_Win_ptr_seg_comb_t remote_list_head;

	/* get the mutex */
	do {
		/* TODO: busy waiting */
		*status = dmapp_acswap_qw(&mutex,
				(void*) &(win->dynamic_list[target_rank].win_ptr->dynamic_mutex),
				&(win->dynamic_list[target_rank].win_ptr_seg.dmapp), target_pe,
				(int64_t) foMPI_MUTEX_NONE, (int64_t) win->commrank);
		_check_dmapp_status(*status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
	} while (mutex != foMPI_MUTEX_NONE);

	/* get the current remote id */
	*status = dmapp_get(&remote_dyn_id,
			(void*) &(win->dynamic_list[target_rank].win_ptr->dynamic_id),
			&(win->dynamic_list[target_rank].win_ptr_seg.dmapp), target_pe, 1, DMAPP_QW);
	_check_dmapp_status(*status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);

	if (remote_dyn_id != win->dynamic_list[target_rank].dynamic_id) {

		win->dynamic_list[target_rank].dynamic_id = remote_dyn_id;

		/* delete the current list */
		if (win->dynamic_list[target_rank].remote_mem_regions != NULL) {
			current = win->dynamic_list[target_rank].remote_mem_regions;
			while (current->next != NULL) {
				temp = current;
				current = current->next;
				_foMPI_FREE(temp);
			}
			_foMPI_FREE(current);
			win->dynamic_list[target_rank].remote_mem_regions = NULL;
		}

		assert(win->dynamic_list[target_rank].remote_mem_regions == NULL);

		times = sizeof(foMPI_Win_ptr_seg_comb_t) / 8; /* TODO: what if the size is not exactly divisible by eight */

		/* get new list */
		/* get the pointer to the list */
		*status = dmapp_get(&remote_list_head,
				&(win->dynamic_list[target_rank].win_ptr->win_dynamic_mem_regions_seg.dmapp),
				&(win->dynamic_list[target_rank].win_ptr_seg.dmapp), target_pe, times, DMAPP_QW);
		_check_dmapp_status(*status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
		if (remote_list_head.ptr != NULL) {
			/* get the first element */
			temp = _foMPI_ALLOC(sizeof(foMPI_Win_dynamic_element_t));
			times = sizeof(foMPI_Win_dynamic_element_t) / 8; /* TODO: what if the size is not exactly divisible by eight */

			*status = dmapp_get(temp, remote_list_head.ptr, &remote_list_head.seg.dmapp, target_pe,
					times, DMAPP_QW);
			_check_dmapp_status(*status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
			win->dynamic_list[target_rank].remote_mem_regions = temp;
			current = temp;

			while (current->next != NULL) { /* get remaining elements */
				temp = _foMPI_ALLOC(sizeof(foMPI_Win_dynamic_element_t));
				*status = dmapp_get(temp, (void*) current->next, &(current->next_seg.dmapp),
						target_pe, times, DMAPP_QW);
				_check_dmapp_status(*status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
				current->next = temp;
				current = current->next;
			}
		} /* no else needed since win->dynamic_list[target_rank].remote_mem_regions is already NULL */
	}

	/* release the mutex */
	/* TODO: nonblocking ? */
	/* we have to do this with an atomic operation, with a simple put it was deadlocking some times */
	*status = dmapp_acswap_qw(&mutex,
			(void*) &(win->dynamic_list[target_rank].win_ptr->dynamic_mutex),
			&(win->dynamic_list[target_rank].win_ptr_seg.dmapp), target_pe, (int64_t) win->commrank,
			(int64_t) foMPI_MUTEX_NONE);
	_check_dmapp_status(*status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);

}

/* pointer to the last two entries */
static int dcache_init_blocks(MPI_Datatype dtype, int count, dcache_blocks_t* blocks) {

	int number_blocks;
	MPI_Aint lb;
	MPI_Aint extent;
	MPI_Aint true_extent;
	MPI_Aint count_extent;

	MPIT_Type_init(dtype);

	MPI_Type_get_extent(dtype, &lb, &extent);
	MPI_Type_get_true_extent(dtype, &lb, &true_extent);
	count_extent = (count - 1) * extent + true_extent;

	MPIT_Type_blockct(count, dtype, 0, count_extent, &(blocks->number_blocks));

	blocks->displ = _foMPI_ALLOC(blocks->number_blocks * sizeof(MPI_Aint));
	blocks->blocklen = _foMPI_ALLOC(blocks->number_blocks * sizeof(int));

	/* get offsets/blocklengths of the origin and the target datatypes */
	number_blocks = blocks->number_blocks;
	/* the resulting offset will be relative, this assumes that MPI_Aint can handle negative offsets */
	MPIT_Type_flatten(0, count, dtype, 0, count_extent, &(blocks->displ[0]), &(blocks->blocklen[0]),
			&number_blocks);

	blocks->type = dtype;
	blocks->type_count = count;
	blocks->pos = 0;
	blocks->block_remaining_size = 0;

	return MPI_SUCCESS;
}

/* the names target_* and origin_* arbitrarily chosen, the algorithm doesn't rely on these "position" information */
static int dcache_get_next_block(MPI_Datatype origin_dtype, int origin_count,
		MPI_Aint* origin_offset, MPI_Datatype target_dtype, int target_count,
		MPI_Aint* target_offset, uint64_t* size, int* last_block) {

	int i;
	int origin_entry;
	int target_entry;

	/* search for the first datatype in the cache */
	i = glob_info.dcache_origin_last_entry;
	origin_entry = -1;
	do {
		if ((glob_info.dcache_blocks[i].type == origin_dtype)
				&& (glob_info.dcache_blocks[i].type_count == origin_count)) {
			origin_entry = i;
			glob_info.dcache_origin_last_entry = i;
		}
		i = (i + 1) % 10;
	} while ((i != glob_info.dcache_origin_last_entry) && (origin_entry == -1));

	if (origin_entry == -1) {
		origin_entry = glob_info.dcache_last_entry;
		glob_info.dcache_last_entry = (glob_info.dcache_last_entry + 1) % 10;
		/* clean up if that cache element was already used */
		if (glob_info.dcache_blocks[origin_entry].type != MPI_DATATYPE_NULL) {
			_foMPI_FREE(glob_info.dcache_blocks[origin_entry].displ);
			_foMPI_FREE(glob_info.dcache_blocks[origin_entry].blocklen);
		}
		dcache_init_blocks(origin_dtype, origin_count, &glob_info.dcache_blocks[origin_entry]);
	}

	/* search for the second datatype in the cache */
	i = glob_info.dcache_target_last_entry;
	target_entry = -1;
	do {
		if ((glob_info.dcache_blocks[i].type == target_dtype)
				&& (glob_info.dcache_blocks[i].type_count == target_count)) {
			target_entry = i;
			glob_info.dcache_target_last_entry = i;
		}
		i = (i + 1) % 10;
	} while ((i != glob_info.dcache_target_last_entry) && (target_entry == -1));

	if (target_entry == -1) {
		if (origin_entry == glob_info.dcache_last_entry) {
			target_entry = (glob_info.dcache_last_entry + 1) % 10;
			glob_info.dcache_last_entry = (glob_info.dcache_last_entry + 2) % 10;
		} else {
			target_entry = glob_info.dcache_last_entry;
			glob_info.dcache_last_entry = (glob_info.dcache_last_entry + 1) % 10;
		}
		/* clean up if that cache element was already used */
		if (glob_info.dcache_blocks[target_entry].type != MPI_DATATYPE_NULL) {
			_foMPI_FREE(glob_info.dcache_blocks[target_entry].displ);
			_foMPI_FREE(glob_info.dcache_blocks[target_entry].blocklen);
		}
		dcache_init_blocks(target_dtype, target_count, &glob_info.dcache_blocks[target_entry]);
	}

	/* find the next block */
	/* begin a new block of the origin datatype */
	if (glob_info.dcache_blocks[origin_entry].block_remaining_size == 0) {
		i = glob_info.dcache_blocks[origin_entry].pos++;
		glob_info.dcache_blocks[origin_entry].block_remaining_size =
				glob_info.dcache_blocks[origin_entry].blocklen[i];
		glob_info.dcache_blocks[origin_entry].block_offset =
				glob_info.dcache_blocks[origin_entry].displ[i];
	}
	/* begin a new block of the target datatype */
	if (glob_info.dcache_blocks[target_entry].block_remaining_size == 0) {
		i = glob_info.dcache_blocks[target_entry].pos++;
		glob_info.dcache_blocks[target_entry].block_remaining_size =
				glob_info.dcache_blocks[target_entry].blocklen[i];
		glob_info.dcache_blocks[target_entry].block_offset =
				glob_info.dcache_blocks[target_entry].displ[i];
	}

	if (glob_info.dcache_blocks[origin_entry].block_remaining_size
			< glob_info.dcache_blocks[target_entry].block_remaining_size) {
		*size = glob_info.dcache_blocks[origin_entry].block_remaining_size;
	} else {
		*size = glob_info.dcache_blocks[target_entry].block_remaining_size;
	}

	*origin_offset = glob_info.dcache_blocks[origin_entry].block_offset;
	*target_offset = glob_info.dcache_blocks[target_entry].block_offset;

	/* set the remainder
	 * it is done with arithmetic operations instead of if statements
	 * one of remainder will always be zero */
	/* the if statement handles the case when the two datatypes are the same */
	if (origin_entry != target_entry) {
		glob_info.dcache_blocks[origin_entry].block_remaining_size -= *size;
		glob_info.dcache_blocks[target_entry].block_remaining_size -= *size;
		glob_info.dcache_blocks[origin_entry].block_offset += *size;
		glob_info.dcache_blocks[target_entry].block_offset += *size;
	} else {
		glob_info.dcache_blocks[origin_entry].block_remaining_size = 0;
	}

	if ((glob_info.dcache_blocks[origin_entry].pos
			== glob_info.dcache_blocks[origin_entry].number_blocks)
			&& (glob_info.dcache_blocks[target_entry].pos
					== glob_info.dcache_blocks[target_entry].number_blocks)) {
		*last_block = 1;
		glob_info.dcache_blocks[origin_entry].pos = 0;
		glob_info.dcache_blocks[target_entry].pos = 0;
		glob_info.dcache_blocks[origin_entry].block_remaining_size = 0;
		glob_info.dcache_blocks[target_entry].block_remaining_size = 0;
	} else {
		*last_block = 0;
	}

	return MPI_SUCCESS;
}

/* TODO: handle of MPI_COMBINER_F90_REAL, MPI_COMBINER_F90_COMPLEX, MPI_COMBINER_F90_INTEGER */
static int check_basic_type(MPI_Datatype type, int* flag, MPI_Datatype* basic_type) {

	dtype_list_t* datatype_list;
	dtype_list_t* current;
	int num_integers;
	int num_addresses;
	int num_datatypes;
	int combiner;

	int init = 0;
	MPI_Datatype* array_datatypes;

	int max_alloc = 1;
	int i;

	int* array_int;
	MPI_Aint* array_addresses;

	*flag = 1;

	datatype_list = _foMPI_ALLOC(sizeof(dtype_list_t));
	datatype_list->next = NULL;
	datatype_list->type = type;

	array_datatypes = _foMPI_ALLOC(sizeof(MPI_Datatype));
	array_int = _foMPI_ALLOC(sizeof(MPI_Datatype));
	array_addresses = _foMPI_ALLOC(sizeof(MPI_Aint));

	while (datatype_list != NULL) {
		MPI_Type_get_envelope(datatype_list->type, &num_integers, &num_addresses, &num_datatypes,
				&combiner);
		if (combiner == MPI_COMBINER_NAMED) {
			if (init == 0) {
				init = 1;
				*basic_type = datatype_list->type;
			} else {
				if (*basic_type != datatype_list->type) {
					/* not the same basic datatypes */
					*flag = 0;
					/* clean up */
					while (datatype_list != NULL) {
						current = datatype_list;
						datatype_list = datatype_list->next;
						_foMPI_FREE(current);
					}
					break;
				} else {
					current = datatype_list;
					datatype_list = datatype_list->next;
					_foMPI_FREE(current);
				}
			}
		} else {
			assert(
					(combiner != MPI_COMBINER_F90_REAL) || (combiner != MPI_COMBINER_F90_COMPLEX)
							|| (combiner != MPI_COMBINER_F90_INTEGER));
			if (num_integers > max_alloc) {
				array_int = realloc(array_int, num_integers * sizeof(int));
				array_addresses = realloc(array_addresses, num_integers * sizeof(MPI_Aint));
				array_datatypes = realloc(array_datatypes, num_integers * sizeof(MPI_Datatype));
				max_alloc = num_integers;
			}
			MPI_Type_get_contents(datatype_list->type, num_integers, num_addresses, num_datatypes,
					&array_int[0], &array_addresses[0], &array_datatypes[0]);
			/* free the datatype and remove it from the queue */
			if (datatype_list->type != type) {
				MPI_Type_free(&(datatype_list->type));
			}

			if (num_datatypes == 1) {
				datatype_list->type = array_datatypes[0];
			} else {
				/* only with a struct there is more than one sub datatypes */
				current = datatype_list;
				datatype_list = datatype_list->next;
				_foMPI_FREE(current);

				for (i = 0; i < num_datatypes; i++) {
					current = _foMPI_ALLOC(sizeof(dtype_list_t));
					current->next = datatype_list;
					current->type = array_datatypes[i];
					datatype_list = current;
				}
			}
		}
	}

	_foMPI_FREE(array_datatypes);
	_foMPI_FREE(array_int);
	_foMPI_FREE(array_addresses);

	return MPI_SUCCESS;
}

static void accumulate_bor_8byte(dmapp_pe_t target_pe, MPI_Aint target_offset,
		MPI_Aint origin_offset, fompi_seg_desc_t* src_seg_ptr, fompi_seg_desc_t* dst_seg_ptr,
		MPI_Aint size, foMPI_Win win, int tag) {

	dmapp_return_t status;

	int nelements;
	int i;

	char* origin_addr = (char*) origin_offset;
	char* target_addr = (char*) target_offset;

	nelements = size / 8;

	for (i = 0; i < nelements; i++) {
		status = dmapp_aor_qw_nbi(target_addr + i * 8, &(dst_seg_ptr->dmapp), target_pe,
				*(int64_t*) (origin_addr + i * 8));
		_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
	}
	win->nbi_counter++; /* little hack, since we actually just need a flag to indicate that there are nbi operations in progress */
}

static void accumulate_band_8byte(dmapp_pe_t target_pe, MPI_Aint target_offset,
		MPI_Aint origin_offset, fompi_seg_desc_t* src_seg_ptr, fompi_seg_desc_t* dst_seg_ptr,
		MPI_Aint size, foMPI_Win win, int tag) {

	dmapp_return_t status;

	int nelements;
	int i;

	char* origin_addr = (char*) origin_offset;
	char* target_addr = (char*) target_offset;

	nelements = size / 8;

	for (i = 0; i < nelements; i++) {
		status = dmapp_aand_qw_nbi(target_addr + i * 8, &(dst_seg_ptr->dmapp), target_pe,
				*(int64_t*) (origin_addr + i * 8));
		_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
	}
	win->nbi_counter++; /* little hack, since we actually just need a flag to indicate that there are nbi operations in progress */
}

static void accumulate_bxor_8byte(dmapp_pe_t target_pe, MPI_Aint target_offset,
		MPI_Aint origin_offset, fompi_seg_desc_t* src_seg_ptr, fompi_seg_desc_t* dst_seg_ptr,
		MPI_Aint size, foMPI_Win win, int tag) {

	dmapp_return_t status;

	int nelements;
	int i;

	char* origin_addr = (char*) origin_offset;
	char* target_addr = (char*) target_offset;

	nelements = size / 8;

	for (i = 0; i < nelements; i++) {
		status = dmapp_axor_qw_nbi(target_addr + i * 8, &(dst_seg_ptr->dmapp), target_pe,
				*(int64_t*) (origin_addr + i * 8));
		_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
	}
	win->nbi_counter++; /* little hack, since we actually just need a flag to indicate that there are nbi operations in progress */
}

static void accumulate_sum_8byte(dmapp_pe_t target_pe, MPI_Aint target_offset,
		MPI_Aint origin_offset, fompi_seg_desc_t* src_seg_ptr, fompi_seg_desc_t* dst_seg_ptr,
		MPI_Aint size, foMPI_Win win, int tag) {

	dmapp_return_t status;

	int nelements;
	int i;

	char* origin_addr = (char*) origin_offset;
	char* target_addr = (char*) target_offset;

	nelements = size / 8;

	for (i = 0; i < nelements; i++) {
		status = dmapp_aadd_qw_nbi(target_addr + i * 8, &(dst_seg_ptr->dmapp), target_pe,
				*(int64_t*) (origin_addr + i * 8));
		_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
	}
	win->nbi_counter++; /* little hack, since we actually just need a flag to indicate that there are nbi operations in progress */
}

static void dmapp_simple_put(dmapp_pe_t target_pe, MPI_Aint target_offset, MPI_Aint origin_offset,
		fompi_seg_desc_t* src_seg_ptr, fompi_seg_desc_t* dst_seg_ptr, MPI_Aint size, foMPI_Win win,
		int tag) {

	dmapp_return_t status;

	uint64_t nelements;
	uint64_t nelements_modulo;
	int offset_modulo;

	char* origin_addr = (char*) origin_offset;
	char* target_addr = (char*) target_offset;

	nelements = size / 16;
	offset_modulo = nelements * 16;
	nelements_modulo = size - offset_modulo;

#ifndef NDEBUG
	int check = _foMPI_is_addr_in_seg((void *) target_addr, dst_seg_ptr)
			&& _foMPI_is_addr_in_seg((void *) (target_addr + size), dst_seg_ptr);
	_foMPI_TRACE_LOG(2, "dmapp_put CHECK REMOTE BOUNDS -> %s \n", check ? "OK" : "ERROR");
#endif

	/* just use *_nbi since it's easier and maybe even faster */
	/* the problem is that we synchronize too much */
#ifdef PAPI
	timing_record( 1 );
#endif
	if (nelements != 0) {
		status = dmapp_put_nbi(target_addr, &(dst_seg_ptr->dmapp), target_pe, origin_addr,
				nelements, DMAPP_DQW);
		_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
		_foMPI_TRACE_LOG(2,
				"dmapp_put local mem:  [0x%lx , 0x%lx] remote mem: [0x%lx , 0x%lx] data length: %4i\n",
				origin_addr, origin_addr + size, target_addr, target_addr + size, (int ) size);
		_foMPI_TRACE_LOG(2, "dmapp_put  targetPE: %d  tag: %d  successful\n", target_pe, tag);

		win->nbi_counter++;
	}
	if (nelements_modulo != 0) {
		status = dmapp_put_nbi(target_addr + offset_modulo, &(dst_seg_ptr->dmapp), target_pe,
				origin_addr + offset_modulo, nelements_modulo, DMAPP_BYTE);
		_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
		_foMPI_TRACE_LOG(2,
				"dmapp_put local mem:  [0x%lx , 0x%lx] remote mem: [0x%lx , 0x%lx] data length: %4i\n",
				origin_addr, origin_addr + size, target_addr, target_addr + size, (int ) size);
		_foMPI_TRACE_LOG(2, "dmapp_put  targetPE: %d  tag: %d  successful\n", target_pe, tag);
		win->nbi_counter++;
	}

#ifdef PAPI
	timing_record( 2 );
#endif
}

#ifdef XPMEM
static void xpmem_simple_put(dmapp_pe_t target_pe, MPI_Aint target_offset, MPI_Aint origin_offset,
		fompi_seg_desc_t* src_seg_ptr, fompi_seg_desc_t* dst_seg_ptr, MPI_Aint size, foMPI_Win win,
		int tag) {

#ifdef PAPI
	timing_record( 1 );
#endif

	if (size > 524288) {
		memcpy((char* ) target_offset, (char* ) origin_offset, size);
	} else {
		sse_memcpy((char*) target_offset, (char*) origin_offset, size);
	}

#ifdef PAPI
	timing_record( 2 );
#endif
}
#endif

static void dmapp_simple_get(dmapp_pe_t target_pe, MPI_Aint target_offset, MPI_Aint origin_offset,
		fompi_seg_desc_t* src_seg_ptr, fompi_seg_desc_t* dst_seg_ptr, MPI_Aint size, foMPI_Win win,
		int tag) {

	dmapp_return_t status;

	uint64_t nelements;
	uint64_t nelements_modulo;
	int offset_modulo;

	char* origin_addr = (char*) origin_offset;
	char* target_addr = (char*) target_offset;

	nelements = size / 16;
	offset_modulo = nelements * 16;
	nelements_modulo = size - offset_modulo;

	/* just use *_nbi since it's easier and maybe even faster */
	/* the problem is that we synchronize too much */
#ifdef PAPI
	timing_record( 5 );
#endif
	if (nelements != 0) {
		status = dmapp_get_nbi(origin_addr, target_addr, &(dst_seg_ptr->dmapp), target_pe,
				nelements, DMAPP_DQW);
		_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
		win->nbi_counter++;
	}
	if (nelements_modulo != 0) {
		status = dmapp_get_nbi(origin_addr + offset_modulo, target_addr + offset_modulo,
				&(dst_seg_ptr->dmapp), target_pe, nelements_modulo, DMAPP_BYTE);
		_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
		win->nbi_counter++;
	}
#ifdef PAPI
	timing_record( 6 );
#endif
}

#ifdef XPMEM
static void xpmem_simple_get(dmapp_pe_t target_pe, MPI_Aint target_offset, MPI_Aint origin_offset,
		fompi_seg_desc_t* src_seg_ptr, fompi_seg_desc_t* dst_seg_ptr, MPI_Aint size, foMPI_Win win,
		int tag) {
#ifdef PAPI
	timing_record( 5 );
#endif

	if (size > 524288) {
		memcpy((char* ) origin_offset, (char* ) target_offset, size);
	} else {
		sse_memcpy((char*) origin_offset, (char*) target_offset, size);
	}

#ifdef PAPI
	timing_record( 6 );
#endif

}
#endif

static int communication_and_datatype_handling(const void* origin_addr, int origin_count,
		MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp, int target_count,
		MPI_Datatype target_datatype, foMPI_Win win, dmapp_pe_t target_pe,
		void (*commfunc)(dmapp_pe_t, MPI_Aint, MPI_Aint, fompi_seg_desc_t*, fompi_seg_desc_t*,
				MPI_Aint, foMPI_Win, int), int tag) {

	dmapp_return_t status;
	fompi_seg_desc_t *dst_seg_ptr;
	fompi_seg_desc_t *src_seg_ptr = NULL;

	foMPI_Win_dynamic_element_t *dst_current;

	int origin_type_size, target_type_size;
	MPI_Aint lb;
	MPI_Aint target_extent;
	MPI_Aint target_true_extent;
	MPI_Aint target_count_extent;

	uint64_t transport_size;
	uint64_t remaining_size;

	MPI_Aint target_block_offset;
	MPI_Aint origin_block_offset;

	int last_block;

#ifndef NDEBUG
	int res;
#endif

	MPI_Aint origin_offset = (MPI_Aint) origin_addr;
	MPI_Aint target_offset;

	/* determine the target_addr and some other create-flavor dependent parameter */
	if (win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC) {

		/* checks if the local information about the remote dynamic window are up-to-date: if not, they are fetched */
		get_dynamic_window_regions(&status, target_rank, target_pe, win);

		target_offset = (MPI_Aint) MPI_BOTTOM + target_disp;
	} else {
#ifdef XPMEM
		int local_rank = foMPI_onnode_rank_global_to_local(target_rank, win);
		if (local_rank != -1) {
			target_offset = (MPI_Aint) win->xpmem_array[local_rank].base
					+ target_disp * win->win_array[target_rank].disp_unit;
		} else {
			target_offset = (MPI_Aint) win->win_array[target_rank].base
					+ target_disp * win->win_array[target_rank].disp_unit;
		}
#else
		target_offset = (MPI_Aint) win->win_array[target_rank].base
		+ target_disp * win->win_array[target_rank].disp_unit;
#endif
		/* TODO: other intrinsic datatypes */
		if ((origin_datatype == target_datatype)
				&& ((origin_datatype == MPI_CHAR) || (origin_datatype == MPI_UINT64_T)
						|| (origin_datatype == MPI_DOUBLE) || (origin_datatype == MPI_INT)
						|| (origin_datatype == MPI_BYTE))) {
			/* fast path for intrinsic datatypes */

			MPI_Type_size(origin_datatype, &origin_type_size);

			dst_seg_ptr = &(win->win_array[target_rank].seg);
			(*commfunc)(target_pe, target_offset, origin_offset, src_seg_ptr, dst_seg_ptr,
					origin_count * origin_type_size, win, tag);

			return MPI_SUCCESS;
		}
	}

	/* process the datatypes */
	MPI_Type_size(origin_datatype, &origin_type_size);
	MPI_Type_size(target_datatype, &target_type_size);

	/* TODO: should this return a error? */
	assert((origin_type_size > 0) && (target_type_size > 0));
	assert((origin_type_size * origin_count) == (target_type_size * target_count));

	/* TODO: real test that the datatypes are identical */
	MPI_Type_get_extent(target_datatype, &lb, &target_extent);

	MPI_Type_get_true_extent(target_datatype, &lb, &target_true_extent);

	target_count_extent = (target_count - 1) * target_extent + target_true_extent;

	if (win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC) {
		/* already checks if the requested memory region is within the boundaries */

		/* check if datatype work in one memory region */
		dst_current = win->dynamic_list[target_rank].remote_mem_regions;
		dst_seg_ptr = NULL;
		if (dst_current != NULL) {
			if ((target_offset >= dst_current->base)
					&& ((target_offset + target_count_extent)
							<= (dst_current->base + dst_current->size))) {
				dst_seg_ptr = &(dst_current->seg);
			}
			while (dst_current->next != NULL) {
				dst_current = dst_current->next;
				if ((target_offset >= dst_current->base)
						&& ((target_offset + target_count_extent)
								<= (dst_current->base + dst_current->size))) {
					dst_seg_ptr = &(dst_current->seg);
				}
			}
		}

	} else {
		dst_seg_ptr = &(win->win_array[target_rank].seg);
	}

	if (dst_seg_ptr != NULL) {
		/* only one memory region */

		do {
			dcache_get_next_block(origin_datatype, origin_count, &origin_block_offset,
					target_datatype, target_count, &target_block_offset, &transport_size,
					&last_block);

			(*commfunc)(target_pe, target_offset + target_block_offset,
					origin_offset + origin_block_offset, src_seg_ptr, dst_seg_ptr, transport_size,
					win, foMPI_ANY_TAG);
		} while (last_block == 0);

	} else {
		/* more than one memory region */

		do {
			dcache_get_next_block(origin_datatype, origin_count, &origin_block_offset,
					target_datatype, target_count, &target_block_offset, &remaining_size,
					&last_block);

			/* TODO: maybe we can save the last state and check if it is still true */
			/* check local list for entry */
			while (remaining_size > 0) {

#ifdef NDEBUG
				find_seg_and_size( win->dynamic_list[target_rank].remote_mem_regions, remaining_size, target_offset+target_block_offset, &dst_seg_ptr, &transport_size );
#else      
				res = find_seg_and_size(win->dynamic_list[target_rank].remote_mem_regions,
						remaining_size, target_offset + target_block_offset, &dst_seg_ptr,
						&transport_size);
				assert(res == MPI_SUCCESS); /* TODO: return a real error code? */

#endif
				(*commfunc)(target_pe, target_offset + target_block_offset,
						origin_offset + origin_block_offset, src_seg_ptr, dst_seg_ptr,
						transport_size, win, foMPI_ANY_TAG);

				target_block_offset += transport_size;
				origin_block_offset += transport_size;
				remaining_size -= transport_size;
			}

		} while (last_block == 0);
	}

	return MPI_SUCCESS;
}

#ifdef UGNI

static int communication_and_datatype_handling_ugni_put_notify_only(const void* origin_addr,
		int origin_count, MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp,
		int target_count, MPI_Datatype target_datatype, foMPI_Win win, dmapp_pe_t target_pe,
		int tag) {

	dmapp_return_t status;
	fompi_seg_desc_t *dst_seg_ptr;
	foMPI_Win_dynamic_element_t *dst_current;
	fompi_seg_desc_t *src_seg_ptr = NULL;
	foMPI_Win_dynamic_element_t *src_current;

	int origin_type_size, target_type_size;
	MPI_Aint lb;
	MPI_Aint target_extent;
	MPI_Aint target_true_extent;
	MPI_Aint target_count_extent;

	/*needed for finding the source segment*/
	MPI_Aint lc;
	MPI_Aint origin_extent;
	MPI_Aint origin_true_extent;
	MPI_Aint origin_count_extent;

	uint64_t transport_size;
	uint64_t remaining_size;

	MPI_Aint target_block_offset;
	MPI_Aint origin_block_offset;

	int last_block;

	int count_blocks = 0;
#ifndef NDEBUG
	int res;
#endif

	MPI_Aint origin_offset = (MPI_Aint) origin_addr;
	MPI_Aint target_offset;

	/* determine the target_addr and some other create-flavor dependent parameter */
	if (win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC) {

		/* checks if the local information about the remote dynamic window are up-to-date: if not, they are fetched */
		get_dynamic_window_regions(&status, target_rank, target_pe, win);

		target_offset = (MPI_Aint) MPI_BOTTOM + target_disp;
	} else {
#ifdef XPMEM
		int local_rank = foMPI_onnode_rank_global_to_local(target_rank, win);
		if (local_rank != -1) {
			target_offset = (MPI_Aint) win->xpmem_array[local_rank].base
					+ target_disp * win->win_array[target_rank].disp_unit;
		} else {
			target_offset = (MPI_Aint) win->win_array[target_rank].base
					+ target_disp * win->win_array[target_rank].disp_unit;
		}
#else
		target_offset = (MPI_Aint) win->win_array[target_rank].base
		+ target_disp * win->win_array[target_rank].disp_unit;
#endif
		/* TODO: other intrinsic datatypes */
		if ((origin_datatype == target_datatype)
				&& ((origin_datatype == MPI_CHAR) || (origin_datatype == MPI_UINT64_T)
						|| (origin_datatype == MPI_DOUBLE) || (origin_datatype == MPI_INT)
						|| (origin_datatype == MPI_BYTE))) {
			/* fast path for intrinsic datatypes */
			_foMPI_TRACE_LOG(1, " Communication_hdl_ugni_put -> fast path\n");
			MPI_Type_size(origin_datatype, &origin_type_size);

			dst_seg_ptr = &(win->win_array[target_rank].seg);
			/*in order to use the uGNI API we need also the handle of the source memory segment*/
			if (_foMPI_is_addr_in_seg((void *) origin_offset,
					&(win->win_array[win->commrank].seg))== _foMPI_TRUE) {
				src_seg_ptr = &(win->win_array[win->commrank].seg);
			}

			if (origin_count * origin_type_size < _foMPI_MAX_RDMA_TRANFER_LENGHT) {
				ugni_simple_put(target_pe, target_offset, origin_offset, src_seg_ptr, dst_seg_ptr,
						origin_count * origin_type_size, win, tag);
			} else {
				dmapp_simple_put(target_pe, target_offset, origin_offset, src_seg_ptr, dst_seg_ptr,
						origin_count * origin_type_size, win, MPI_ANY_TAG);
				foMPI_Win_flush_all(win);
				ugni_simple_put(target_pe, target_offset, origin_offset, src_seg_ptr, dst_seg_ptr,
						0, win, tag);
			}

			return MPI_SUCCESS;
		}
	}

	/* process the datatypes */
	MPI_Type_size(origin_datatype, &origin_type_size);
	MPI_Type_size(target_datatype, &target_type_size);

	/* TODO: should this return a error? */
	assert((origin_type_size > 0) && (target_type_size > 0));
	assert((origin_type_size * origin_count) == (target_type_size * target_count));

	/* TODO: real test that the datatypes are identical */
	MPI_Type_get_extent(target_datatype, &lb, &target_extent);

	MPI_Type_get_true_extent(target_datatype, &lb, &target_true_extent);

	target_count_extent = (target_count - 1) * target_extent + target_true_extent;
	/*
	 * this part is needed for finding the source segment handle.
	 * The src segment handle is needed only by uGNI
	 * uGNI is used only for get_notify
	 * */

	MPI_Type_get_extent(origin_datatype, &lc, &origin_extent);
	MPI_Type_get_true_extent(origin_datatype, &lc, &origin_true_extent);
	origin_count_extent = (origin_count - 1) * origin_extent + origin_true_extent;

	if (win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC) {
		/* already checks if the requested memory region is within the boundaries */

		/* check if datatype work in one memory region */
		dst_current = win->dynamic_list[target_rank].remote_mem_regions;
		dst_seg_ptr = NULL;
		if (dst_current != NULL) {
			if ((target_offset >= dst_current->base)
					&& ((target_offset + target_count_extent)
							<= (dst_current->base + dst_current->size))) {
				dst_seg_ptr = &(dst_current->seg);
			}
			while (dst_current->next != NULL) {
				dst_current = dst_current->next;
				if ((target_offset >= dst_current->base)
						&& ((target_offset + target_count_extent)
								<= (dst_current->base + dst_current->size))) {
					dst_seg_ptr = &(dst_current->seg);
				}
			}
		}

		/*finds the source memory segment descriptor */
		src_current = win->win_dynamic_mem_regions;
		src_seg_ptr = NULL;
		if (src_current != NULL) {
			if ((origin_offset >= src_current->base)
					&& ((origin_offset + origin_count_extent)
							<= (src_current->base + src_current->size))) {
				src_seg_ptr = &(src_current->seg);
			}
			while (src_current->next != NULL) {
				src_current = src_current->next;
				if ((origin_offset >= src_current->base)
						&& ((origin_offset + origin_count_extent)
								<= (src_current->base + src_current->size))) {
					src_seg_ptr = &(src_current->seg);
				}
			}
		}

	} else {
		dst_seg_ptr = &(win->win_array[target_rank].seg);
		/* The source addr can be not included in the window */
		if (_foMPI_is_addr_in_seg((void *) origin_offset,
				&(win->win_array[win->commrank].seg))== _foMPI_TRUE) {
			src_seg_ptr = &(win->win_array[win->commrank].seg);
		}
	}

	if (dst_seg_ptr != NULL) {
		/* only one memory region */

		do {
			dcache_get_next_block(origin_datatype, origin_count, &origin_block_offset,
					target_datatype, target_count, &target_block_offset, &transport_size,
					&last_block);
			if (tag != foMPI_ANY_TAG) {
				/*we have to send a notification*/
				if (last_block == 1) {
					/*last transfer*/
					if (count_blocks == 0) {
						/*first and last block transfer*/
						if (transport_size < _foMPI_MAX_RDMA_TRANFER_LENGHT) {
							ugni_simple_put(target_pe, target_offset + target_block_offset,
									origin_offset + origin_block_offset, src_seg_ptr, dst_seg_ptr,
									transport_size, win, tag);
							_foMPI_TRACE_LOG(2,
									" Communication_hdl_ugni_put -> one mem reg single block\n");
						} else {
							dmapp_simple_put(target_pe, target_offset + target_block_offset,
									origin_offset + origin_block_offset, src_seg_ptr, dst_seg_ptr,
									transport_size, win, MPI_ANY_TAG);
							foMPI_Win_flush_all(win);
							ugni_simple_put(target_pe, target_offset + target_block_offset,
									origin_offset + origin_block_offset, src_seg_ptr, dst_seg_ptr,
									0, win, tag);
							_foMPI_TRACE_LOG(2,
									" Communication_hdl_ugni_put -> one mem reg single block comb\n");
						}
					} else {
						/*last block but not first*/
						/*local wait for completion of all transfers and subsequent notification sending*/
						dmapp_simple_put(target_pe, target_offset + target_block_offset,
								origin_offset + origin_block_offset, src_seg_ptr, dst_seg_ptr,
								transport_size, win, foMPI_ANY_TAG);
						foMPI_Win_flush_all(win);
						/*0 sized put (notification only)*/
						ugni_simple_put(target_pe, target_offset + target_block_offset,
								origin_offset + origin_block_offset, src_seg_ptr, dst_seg_ptr, 0,
								win, tag);
						_foMPI_TRACE_LOG(2,
								" Communication_hdl_ugni_put -> one mem reg multiple block comb\n");

					}
				} else {
					/*not last block --> no notification*/
					dmapp_simple_put(target_pe, target_offset + target_block_offset,
							origin_offset + origin_block_offset, src_seg_ptr, dst_seg_ptr,
							transport_size, win, foMPI_ANY_TAG);
					count_blocks++;
				}
			} else {
				/*Anytag No notification needed*/
				dmapp_simple_put(target_pe, target_offset + target_block_offset,
						origin_offset + origin_block_offset, src_seg_ptr, dst_seg_ptr,
						transport_size, win, foMPI_ANY_TAG);
				count_blocks++;
			}

		} while (last_block == 0);

	} else {
		/* more than one memory region */
		do {
			dcache_get_next_block(origin_datatype, origin_count, &origin_block_offset,
					target_datatype, target_count, &target_block_offset, &remaining_size,
					&last_block);

			/* TODO: maybe we can save the last state and check if it is still true */
			/* check local list for entry */
			while (remaining_size > 0) {

#ifdef NDEBUG
				find_seg_and_size( win->dynamic_list[target_rank].remote_mem_regions, remaining_size, target_offset+target_block_offset, &dst_seg_ptr, &transport_size );
				//TODO:
				/*find_seg_and_size(win->win_dynamic_mem_regions ,
				 remaining_size, origin_offset + origin_block_offset, &src_seg_ptr,
				 &transport_size);	*/
#else
				res = find_seg_and_size(win->dynamic_list[target_rank].remote_mem_regions,
						remaining_size, target_offset + target_block_offset, &dst_seg_ptr,
						&transport_size);
				assert(res == MPI_SUCCESS); /* TODO: return a real error code? */
				/*res = find_seg_and_size(win->win_dynamic_mem_regions ,
				 remaining_size, origin_offset + origin_block_offset, &src_seg_ptr,
				 &transport_size);*/

#endif

				/* Anytag No notification needed*/
				dmapp_simple_put(target_pe, target_offset + target_block_offset,
						origin_offset + origin_block_offset, src_seg_ptr, dst_seg_ptr,
						transport_size, win, foMPI_ANY_TAG);
				count_blocks++;

				target_block_offset += transport_size;
				origin_block_offset += transport_size;
				remaining_size -= transport_size;
			}

		} while (last_block == 0);
		if (tag != foMPI_ANY_TAG) {
			foMPI_Win_flush_all(win);
			ugni_simple_put(target_pe, target_offset + target_block_offset,
					origin_offset + origin_block_offset, src_seg_ptr, dst_seg_ptr, 0, win, tag);
			_foMPI_TRACE_LOG(2, " Communication_hdl_ugni_put -> multi mem reg multi block comb\n");
		}
	}

	return MPI_SUCCESS;
}

/*this function is a copy of the other but permits to verify the alignment used by ugni get
 * comm_fun_align permits to send notification, but needs aligned pointers
 * the other one doesn't needd aligned pointers but cannot send notifications
 * */
static int communication_and_datatype_handling_ugni_get_notify_only(const void* origin_addr,
		int origin_count, MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp,
		int target_count, MPI_Datatype target_datatype, foMPI_Win win, dmapp_pe_t target_pe,
		int tag) {

	dmapp_return_t status;
	fompi_seg_desc_t *dst_seg_ptr;
	foMPI_Win_dynamic_element_t *dst_current;
	fompi_seg_desc_t *src_seg_ptr;
	foMPI_Win_dynamic_element_t *src_current;
	int align = _foMPI_UGNI_GET_ALIGN;

	int origin_type_size, target_type_size;
	MPI_Aint lb;
	MPI_Aint lc;
	MPI_Aint target_extent;
	MPI_Aint target_true_extent;
	MPI_Aint target_count_extent;
	MPI_Aint origin_extent;
	MPI_Aint origin_true_extent;
	MPI_Aint origin_count_extent;
	uint64_t transport_size;
	uint64_t remaining_size;

	MPI_Aint target_block_offset;
	MPI_Aint origin_block_offset;

	int last_block;

	int count_blocks = 0;
#ifndef NDEBUG
	int res;
#endif

	MPI_Aint origin_offset = (MPI_Aint) origin_addr;
	MPI_Aint target_offset;

	/* determine the target_addr and some other create-flavor dependent parameter */
	if (win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC) {

		/* checks if the local information about the remote dynamic window are up-to-date: if not, they are fetched */
		get_dynamic_window_regions(&status, target_rank, target_pe, win);

		target_offset = (MPI_Aint) MPI_BOTTOM + target_disp;
	} else {
#ifdef XPMEM
		int local_rank = foMPI_onnode_rank_global_to_local(target_rank, win);
		if (tag == foMPI_ANY_TAG && local_rank != -1) {
			/*we use XPMEM only if notification is not needed*/
			target_offset = (MPI_Aint) win->xpmem_array[local_rank].base
					+ target_disp * win->win_array[target_rank].disp_unit;
		} else {
			target_offset = (MPI_Aint) win->win_array[target_rank].base
					+ target_disp * win->win_array[target_rank].disp_unit;
		}
#else
		target_offset = (MPI_Aint) win->win_array[target_rank].base
		+ target_disp * win->win_array[target_rank].disp_unit;
#endif
		/* TODO: other intrinsic datatypes */
		if ((origin_datatype == target_datatype)
				&& ((origin_datatype == MPI_CHAR) || (origin_datatype == MPI_UINT64_T)
						|| (origin_datatype == MPI_DOUBLE) || (origin_datatype == MPI_INT)
						|| (origin_datatype == MPI_BYTE))) {
			/* fast path for intrinsic datatypes */

			MPI_Type_size(origin_datatype, &origin_type_size);

			dst_seg_ptr = &(win->win_array[target_rank].seg);
			/*in order to use the uGNI API we need also the handle of the source memory segment*/
			src_seg_ptr = &(win->win_array[win->commrank].seg);
			if (is_aligned(origin_offset,
					align) && is_aligned(target_offset, align) && (origin_count * origin_type_size) < _foMPI_MAX_RDMA_TRANFER_LENGHT) {
				/*transfer + notification */
				_foMPI_TRACE_LOG(2,
						"communication_and_datatype_handling_ugni_get_only  ALIGN  src_addr: %ld  dst_addr: %ld\n",
						origin_offset, target_offset);
				ugni_simple_get(target_pe, target_offset, origin_offset, src_seg_ptr, dst_seg_ptr,
						origin_count * origin_type_size, win, tag);
			} else {
				/*transfer without notification*/
				_foMPI_TRACE_LOG(2,
						"communication_and_datatype_handling_ugni_get_only  NO ALIGN  src_addr: %ld  dst_addr: %ld\n",
						origin_offset, target_offset);
				dmapp_simple_get(target_pe, target_offset, origin_offset, src_seg_ptr, dst_seg_ptr,
						origin_count * origin_type_size, win, foMPI_ANY_TAG);
				foMPI_Win_flush_all(win);
				/*transfer globally visible*/
				/*0 sized put (notification)*/
				/*
				 * we need to use the put because does not have any restriction on alignment,
				 * if we align the pointers to use the get we can potentially go out
				 * of the window
				 * */
				ugni_simple_put(target_pe, target_offset, origin_offset, src_seg_ptr, dst_seg_ptr,
						0, win, tag);

			}
			return MPI_SUCCESS;
		}
	}

	/* process the datatypes */
	MPI_Type_size(origin_datatype, &origin_type_size);
	MPI_Type_size(target_datatype, &target_type_size);

	/* TODO: should this return a error? */
	assert((origin_type_size > 0) && (target_type_size > 0));
	assert((origin_type_size * origin_count) == (target_type_size * target_count));

	/* TODO: real test that the datatypes are identical */
	MPI_Type_get_extent(target_datatype, &lb, &target_extent);

	MPI_Type_get_true_extent(target_datatype, &lb, &target_true_extent);

	target_count_extent = (target_count - 1) * target_extent + target_true_extent;
	/*
	 * this part is needed for finding the source segment handle.
	 * The src segment handle is needed only by uGNI
	 * uGNI is used only for get_notify
	 * */

	MPI_Type_get_extent(origin_datatype, &lc, &origin_extent);
	MPI_Type_get_true_extent(origin_datatype, &lc, &origin_true_extent);
	origin_count_extent = (origin_count - 1) * origin_extent + origin_true_extent;

	if (win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC) {
		/* already checks if the requested memory region is within the boundaries */

		/* check if datatype work in one memory region */
		dst_current = win->dynamic_list[target_rank].remote_mem_regions;
		dst_seg_ptr = NULL;

		/*finds the remote memory segment descriptor */
		if (dst_current != NULL) {
			if ((target_offset >= dst_current->base)
					&& ((target_offset + target_count_extent)
							<= (dst_current->base + dst_current->size))) {
				dst_seg_ptr = &(dst_current->seg);
			}
			while (dst_current->next != NULL) {
				dst_current = dst_current->next;
				if ((target_offset >= dst_current->base)
						&& ((target_offset + target_count_extent)
								<= (dst_current->base + dst_current->size))) {
					dst_seg_ptr = &(dst_current->seg);
				}
			}
		}

		/*finds the source memory segment descriptor */
		src_current = win->win_dynamic_mem_regions;
		src_seg_ptr = NULL;
		if (src_current != NULL) {
			if ((origin_offset >= src_current->base)
					&& ((origin_offset + origin_count_extent)
							<= (src_current->base + src_current->size))) {
				src_seg_ptr = &(src_current->seg);
			}
			while (src_current->next != NULL) {
				src_current = src_current->next;
				if ((origin_offset >= src_current->base)
						&& ((origin_offset + origin_count_extent)
								<= (src_current->base + src_current->size))) {
					src_seg_ptr = &(src_current->seg);
				}
			}
		}

	} else {
		dst_seg_ptr = &(win->win_array[target_rank].seg);
		src_seg_ptr = &(win->win_array[win->commrank].seg);
	}
	_foMPI_TRACE_LOG(2, "SEGMENT SEARCH 1 dst: %p src: %p\n", dst_seg_ptr, src_seg_ptr);
	if (dst_seg_ptr != NULL) {
		/* only one memory region */

		do {
			dcache_get_next_block(origin_datatype, origin_count, &origin_block_offset,
					target_datatype, target_count, &target_block_offset, &transport_size,
					&last_block);

			if (tag != foMPI_ANY_TAG) {
				/*we have to send a notification*/
				if (last_block == 1) {
					/*last transfer*/
					if (count_blocks == 0) {
						/*first and last block transfer*/
						if (is_aligned((origin_offset + origin_block_offset), align)
								&& is_aligned((target_offset + target_block_offset), align)
								&& (transport_size) < _foMPI_MAX_RDMA_TRANFER_LENGHT
								&& src_seg_ptr) {
							_foMPI_TRACE_LOG(2,
									"communication_and_datatype_handling_ugni_get_only  ALIGN  src_addr: %ld  dst_addr: %ld\n",
									origin_offset + origin_block_offset,
									target_offset + target_block_offset);
							ugni_simple_get(target_pe, target_offset + target_block_offset,
									origin_offset + origin_block_offset, src_seg_ptr, dst_seg_ptr,
									transport_size, win, tag);
						} else {
							/* only communication without notification */
							dmapp_simple_get(target_pe, target_offset + target_block_offset,
									origin_offset + origin_block_offset, src_seg_ptr, dst_seg_ptr,
									transport_size, win, foMPI_ANY_TAG);
							foMPI_Win_flush_all(win);
							/*0 sized rma (notification)*/
							ugni_simple_put(target_pe, target_offset + target_block_offset,
									origin_offset + origin_block_offset, src_seg_ptr, dst_seg_ptr,
									0, win, tag);
							_foMPI_TRACE_LOG(2,
									"communication_and_datatype_handling_ugni_get_only NO ALIGN  src_addr: %ld  dst_addr: %ld\n",
									origin_offset + origin_block_offset,
									target_offset + target_block_offset);
						}
					} else {
						/*last block transfer but not first*/
						/*local wait for completion of all transfers and subsequent notification sending*/
						dmapp_simple_get(target_pe, target_offset + target_block_offset,
								origin_offset + origin_block_offset, src_seg_ptr, dst_seg_ptr,
								transport_size, win, foMPI_ANY_TAG);
						foMPI_Win_flush_all(win);
						/*0 sized get (notification)*/
						ugni_simple_put(target_pe, target_offset + target_block_offset,
								origin_offset + origin_block_offset, src_seg_ptr, dst_seg_ptr, 0,
								win, tag);
						_foMPI_TRACE_LOG(2,
								"communication_and_datatype_handling_ugni_get_only Last of multiple blocks  src_addr: %ld  dst_addr: %ld\n",
								origin_offset + origin_block_offset,
								target_offset + target_block_offset);

					}
				} else {
					/*not last block --> no notification*/
					dmapp_simple_get(target_pe, target_offset + target_block_offset,
							origin_offset + origin_block_offset, src_seg_ptr, dst_seg_ptr,
							transport_size, win, foMPI_ANY_TAG);
					count_blocks++;
				}
			} else {

				/*Anytag No notification needed*/
				dmapp_simple_get(target_pe, target_offset + target_block_offset,
						origin_offset + origin_block_offset, src_seg_ptr, dst_seg_ptr,
						transport_size, win, foMPI_ANY_TAG);
				count_blocks++;
			}

		} while (last_block == 0);

	} else {
		/* more than one memory region */

		do {
			dcache_get_next_block(origin_datatype, origin_count, &origin_block_offset,
					target_datatype, target_count, &target_block_offset, &remaining_size,
					&last_block);

			/* TODO: maybe we can save the last state and check if it is still true */
			/* check local list for entry */
			while (remaining_size > 0) {

#ifdef NDEBUG
				find_seg_and_size( win->dynamic_list[target_rank].remote_mem_regions, remaining_size, target_offset+target_block_offset, &dst_seg_ptr, &transport_size );

#else
				res = find_seg_and_size(win->dynamic_list[target_rank].remote_mem_regions,
						remaining_size, target_offset + target_block_offset, &dst_seg_ptr,
						&transport_size);
				assert(res == MPI_SUCCESS); /* TODO: return a real error code? */

#endif

				/*Anytag No notification needed*/
				dmapp_simple_get(target_pe, target_offset + target_block_offset,
						origin_offset + origin_block_offset, src_seg_ptr, dst_seg_ptr,
						transport_size, win, foMPI_ANY_TAG);
				count_blocks++;

				target_block_offset += transport_size;
				origin_block_offset += transport_size;
				remaining_size -= transport_size;
			}

		} while (last_block == 0);
		if (tag != foMPI_ANY_TAG) {
			foMPI_Win_flush_all(win);
			ugni_simple_put(target_pe, target_offset + target_block_offset,
					origin_offset + origin_block_offset, src_seg_ptr, dst_seg_ptr, 0, win, tag);
			_foMPI_TRACE_LOG(2, " Communication_hdl_ugni_get -> multi mem reg multi block comb\n");
		}
	}

	return MPI_SUCCESS;
}
#endif

static int communication_and_datatype_handling_dmapp_only(const void* origin_addr, int origin_count,
		MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp, int target_count,
		MPI_Datatype target_datatype, foMPI_Win win, dmapp_pe_t target_pe,
		void (*commfunc)(dmapp_pe_t, MPI_Aint, MPI_Aint, fompi_seg_desc_t*, fompi_seg_desc_t*,
				MPI_Aint, foMPI_Win, int)) {

	dmapp_return_t status;
	fompi_seg_desc_t* dest_seg_ptr;
	foMPI_Win_dynamic_element_t* current;

	int origin_type_size, target_type_size;
	MPI_Aint lb;
	MPI_Aint target_extent;
	MPI_Aint target_true_extent;
	MPI_Aint target_count_extent;
	uint64_t transport_size;
	uint64_t remaining_size;

	MPI_Aint target_block_offset;
	MPI_Aint origin_block_offset;

	int last_block;
#ifndef NDEBUG
	int res;
#endif

	MPI_Aint origin_offset = (MPI_Aint) origin_addr;
	MPI_Aint target_offset;

	/* determine the target_addr and some other create-flavor dependent parameter */
	if (win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC) {

		/* checks if the local information about the remote dynamic window are up-to-date: if not, they are fetched */
		get_dynamic_window_regions(&status, target_rank, target_pe, win);

		target_offset = (MPI_Aint) MPI_BOTTOM + target_disp;
	} else {
		target_offset = (MPI_Aint) win->win_array[target_rank].base
				+ target_disp * win->win_array[target_rank].disp_unit;
		/* TODO: other intrinsic datatypes */
		if ((origin_datatype == target_datatype)
				&& ((origin_datatype == MPI_CHAR) || (origin_datatype == MPI_UINT64_T)
						|| (origin_datatype == MPI_DOUBLE) || (origin_datatype == MPI_INT))) {
			/* fast path for intrinsic datatypes */

			dest_seg_ptr = &(win->win_array[target_rank].seg);

			MPI_Type_size(origin_datatype, &origin_type_size);
			(*commfunc)(target_pe, target_offset, origin_offset, NULL, dest_seg_ptr,
					origin_count * origin_type_size, win, foMPI_ANY_TAG);

			return MPI_SUCCESS;
		}
	}

	/* process the datatypes */
	MPI_Type_size(origin_datatype, &origin_type_size);
	MPI_Type_size(target_datatype, &target_type_size);

	/* TODO: should this return a error? */
	assert((origin_type_size > 0) && (target_type_size > 0));
	assert((origin_type_size * origin_count) == (target_type_size * target_count));

	/* TODO: real test that the datatypes are identical */
	MPI_Type_get_extent(target_datatype, &lb, &target_extent);

	MPI_Type_get_true_extent(target_datatype, &lb, &target_true_extent);

	target_count_extent = (target_count - 1) * target_extent + target_true_extent;

	if (win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC) {
		/* already checks if the requested memory region is within the boundaries */

		/* check if datatype work in one memory region */
		current = win->dynamic_list[target_rank].remote_mem_regions;
		dest_seg_ptr = NULL;
		if (current != NULL) {
			if ((target_offset >= current->base)
					&& ((target_offset + target_count_extent) <= (current->base + current->size))) {
				dest_seg_ptr = &(current->seg);
			}
			while (current->next != NULL) {
				current = current->next;
				if ((target_offset >= current->base)
						&& ((target_offset + target_count_extent) <= (current->base + current->size))) {
					dest_seg_ptr = &(current->seg);
				}
			}
		}
	} else {
		dest_seg_ptr = &(win->win_array[target_rank].seg);
	}

	if (dest_seg_ptr != NULL) {
		/* only one memory region */

		do {
			dcache_get_next_block(origin_datatype, origin_count, &origin_block_offset,
					target_datatype, target_count, &target_block_offset, &transport_size,
					&last_block);

			(*commfunc)(target_pe, target_offset + target_block_offset,
					origin_offset + origin_block_offset, NULL, dest_seg_ptr, transport_size, win,
					foMPI_ANY_TAG);
		} while (last_block == 0);

	} else {
		/* more than one memory region */

		do {
			dcache_get_next_block(origin_datatype, origin_count, &origin_block_offset,
					target_datatype, target_count, &target_block_offset, &remaining_size,
					&last_block);

			/* TODO: maybe we can save the last state and check if it is still true */
			/* check local list for entry */
			while (remaining_size > 0) {

#ifdef NDEBUG
				find_seg_and_size( win->dynamic_list[target_rank].remote_mem_regions, remaining_size, target_offset+target_block_offset, &dest_seg_ptr, &transport_size );
#else      
				res = find_seg_and_size(win->dynamic_list[target_rank].remote_mem_regions,
						remaining_size, target_offset + target_block_offset, &dest_seg_ptr,
						&transport_size);
#endif
				assert(res == MPI_SUCCESS); /* TODO: return a real error code? */

				(*commfunc)(target_pe, target_offset + target_block_offset,
						origin_offset + origin_block_offset, NULL, dest_seg_ptr, transport_size,
						win, foMPI_ANY_TAG);

				target_block_offset += transport_size;
				origin_block_offset += transport_size;
				remaining_size -= transport_size;
			}

		} while (last_block == 0);
	}

	return MPI_SUCCESS;
}

static int simple_rma_wait(void *extra_state, MPI_Status *status) {
	int dummy_flag = 0;
	return foMPI_Rma_test_wait(extra_state, &dummy_flag, status, _foMPI_TRUE);
}

static int simple_rma_test(void *extra_state, int *flag, MPI_Status *status) {
	return foMPI_Rma_test_wait(extra_state, flag, status, _foMPI_FALSE);
}

static int free_wrapper(void * ptr) {
	_foMPI_FREE(ptr);
	return MPI_SUCCESS;
}

/* TODO: the error field is not set correctly */
static inline int foMPI_Rma_test_wait(void *extra_state, int *flag, MPI_Status *status, int wait) {

	foMPI_Rma_request_t *args = (foMPI_Rma_request_t *) extra_state;
	dmapp_return_t dmapp_status;
	*flag = 0;

	if (args->win->nbi_counter == 0
#ifdef UGNI
			&& args->win->fompi_comm->counter_ugni_nbi == 0
#endif
					) {

		/*synchronized*/
		if (status != MPI_STATUS_IGNORE) {
			status->MPI_SOURCE = args->source;
			//TODO: Why not using MPI_UNDEFINED ??? or foMPI_ANY_TAG ?
			status->MPI_TAG = foMPI_UNDEFINED;
			status->MPI_ERROR = foMPI_UNDEFINED;
			MPI_Status_set_elements(status, args->datatype, args->elements);
			MPI_Status_set_cancelled(status, 0);
		}
		*flag = 1;
		return MPI_SUCCESS;
	}
	if (wait == _foMPI_TRUE) {
		foMPI_Win_flush_all(args->win);
		*flag = 1;
	} else {
#ifdef XPMEM
		/* XPMEM completion */
		__sync_synchronize();
#endif
		/*set the flag*/
		dmapp_status = dmapp_gsync_test(flag); /* flag = 1, if all non-blocking implicit requests are globally visible, if not flag = 0 */
		_check_dmapp_status(dmapp_status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
#ifdef UGNI
		if (args->win->fompi_comm->counter_ugni_nbi > 0) {
			*flag = 0;
		}
#endif
	}
	return MPI_SUCCESS;
}

/* real MPI functions */
int foMPI_Put(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype,
		int target_rank, MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype,
		foMPI_Win win) {

#ifdef PAPI
	timing_record( -1 );
#endif

	int res;

	if ((target_rank == MPI_PROC_NULL) || (origin_count == 0) || (target_count == 0)) {
		return MPI_SUCCESS;
	}

	assert((target_rank >= 0) && (target_rank < win->commsize));
	dmapp_pe_t target_pe = (dmapp_pe_t) win->group_ranks[target_rank];

#ifdef XPMEM
	if ((win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC)
			&& (foMPI_onnode_rank_global_to_local(target_rank, win) != -1)) {
		res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype,
				target_rank, target_disp, target_count, target_datatype, win, target_pe,
				&xpmem_simple_put, foMPI_ANY_TAG);
	} else {
		res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype,
				target_rank, target_disp, target_count, target_datatype, win, target_pe,
				&dmapp_simple_put, foMPI_ANY_TAG);
	}
#else
	res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype,
			target_rank, target_disp, target_count, target_datatype, win, target_pe,
			&dmapp_simple_put, foMPI_ANY_TAG);
#endif

#ifdef PAPI
	timing_record( 3 );
#endif

	return res;
}

int foMPI_Get(void *origin_addr, int origin_count, MPI_Datatype origin_datatype, int target_rank,
		MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype, foMPI_Win win) {

#ifdef PAPI
	timing_record( -1 );
#endif

	int res;

	if ((target_rank == MPI_PROC_NULL) || (origin_count == 0) || (target_count == 0)) {
		return MPI_SUCCESS;
	}

	assert((target_rank >= 0) && (target_rank < win->commsize));
	dmapp_pe_t target_pe = (dmapp_pe_t) win->group_ranks[target_rank];

#ifdef XPMEM
	if ((win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC)
			&& (foMPI_onnode_rank_global_to_local(target_rank, win) != -1)) {
		res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype,
				target_pe, target_disp, target_count, target_datatype, win, target_pe,
				&xpmem_simple_get, foMPI_ANY_TAG);
	} else {
		res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype,
				target_pe, target_disp, target_count, target_datatype, win, target_pe,
				&dmapp_simple_get, foMPI_ANY_TAG);
	}
#else
	res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype,
			target_rank, target_disp, target_count, target_datatype, win, target_pe,
			&dmapp_simple_get, foMPI_ANY_TAG);
#endif

#ifdef PAPI
	timing_record( 7 );
#endif

	return res;
}

int foMPI_Rput(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype,
		int target_rank, MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype,
		foMPI_Win win, foMPI_Request *request) {

	int res;

	if ((target_rank == MPI_PROC_NULL) || (origin_count == 0) || (target_count == 0)) {
		return MPI_SUCCESS;
	}

	assert((target_rank >= 0) && (target_rank < win->commsize));
	dmapp_pe_t target_pe = (dmapp_pe_t) win->group_ranks[target_rank];

	*request = _foMPI_ALLOC(sizeof(foMPI_Request_t));
	(*request)->type = _foMPI_REQUEST_NOT_PERSISTENT;
	foMPI_Rma_request_t * args = (foMPI_Rma_request_t *) _foMPI_ALLOC(sizeof(foMPI_Rma_request_t));
	args->win = win;
	args->elements = target_count;
	args->datatype = target_datatype;
	args->source = target_rank;
	(*request)->extra_state = (void *) args;
	(*request)->test_fun = &simple_rma_test;
	(*request)->wait_fun = &simple_rma_wait;
	(*request)->free_fun = &free_wrapper;
	(*request)->cancel_fun = NULL;

#ifdef XPMEM
	if ((win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC)
			&& (foMPI_onnode_rank_global_to_local(target_rank, win) != -1)) {
		res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype,
				target_rank, target_disp, target_count, target_datatype, win, target_pe,
				&xpmem_simple_put, foMPI_ANY_TAG);
	} else {
		res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype,
				target_rank, target_disp, target_count, target_datatype, win, target_pe,
				&dmapp_simple_put, foMPI_ANY_TAG);
	}
#else
	res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype,
			target_rank, target_disp, target_count, target_datatype, win, target_pe,
			&dmapp_simple_put, foMPI_ANY_TAG);
#endif

	return res;
}

int foMPI_Rget(void *origin_addr, int origin_count, MPI_Datatype origin_datatype, int target_rank,
		MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype, foMPI_Win win,
		foMPI_Request *request) {

	int res;

	if ((target_rank == MPI_PROC_NULL) || (origin_count == 0) || (target_count == 0)) {
		return MPI_SUCCESS;
	}

	assert((target_rank >= 0) && (target_rank < win->commsize));
	dmapp_pe_t target_pe = (dmapp_pe_t) win->group_ranks[target_rank];

	*request = _foMPI_ALLOC(sizeof(foMPI_Request_t));
	(*request)->type = _foMPI_REQUEST_NOT_PERSISTENT;
	foMPI_Rma_request_t * args = (foMPI_Rma_request_t *) _foMPI_ALLOC(sizeof(foMPI_Rma_request_t));
	args->win = win;
	args->elements = target_count;
	args->datatype = target_datatype;
	args->source = target_rank;
	(*request)->extra_state = (void *) args;
	(*request)->test_fun = &simple_rma_test;
	(*request)->wait_fun = &simple_rma_wait;
	(*request)->free_fun = &free_wrapper;
	(*request)->cancel_fun = NULL;

#ifdef XPMEM
	if ((win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC)
			&& (foMPI_onnode_rank_global_to_local(target_rank, win) != -1)) {
		res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype,
				target_rank, target_disp, target_count, target_datatype, win, target_pe,
				&xpmem_simple_get, foMPI_ANY_TAG);
	} else {
		res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype,
				target_rank, target_disp, target_count, target_datatype, win, target_pe,
				&dmapp_simple_get, foMPI_ANY_TAG);
	}
#else
	res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype,
			target_rank, target_disp, target_count, target_datatype, win, target_pe,
			&dmapp_simple_get, foMPI_ANY_TAG);
#endif

	return res;
}

int foMPI_Accumulate(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype,
		int target_rank, MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype,
		foMPI_Op op, foMPI_Win win) {

#ifdef PAPI
	timing_record( -1 );
#endif

	void* buffer;
	dmapp_return_t status;
	foMPI_Win_desc_t* win_ptr = NULL;
	fompi_seg_desc_t* win_ptr_seg = NULL;
#ifndef NDEBUG
	int res = MPI_SUCCESS;
#endif

	int flag;

	MPI_Datatype origin_basic_type;
	MPI_Datatype target_basic_type;

	int origin_type_size;

	int origin_basic_type_size;

	MPI_Aint buffer_block_offset;
	MPI_Aint origin_block_offset;

	uint64_t transport_size;

	int elements_in_buffer;

	int last_block;

	if ((target_rank == MPI_PROC_NULL) || (origin_count == 0) || (target_count == 0)) {
		return MPI_SUCCESS;
	}

	assert((target_rank >= 0) && (target_rank < win->commsize));
	dmapp_pe_t target_pe = (dmapp_pe_t) win->group_ranks[target_rank];

	assert((op >= foMPI_SUM) && (op <= foMPI_REPLACE));

	if ((origin_datatype == target_datatype) && (target_datatype == MPI_INT64_T)
			&& (op == foMPI_SUM)) {
#ifdef NDEBUG
		communication_and_datatype_handling_dmapp_only( origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &accumulate_sum_8byte );
#else
		res = communication_and_datatype_handling_dmapp_only(origin_addr, origin_count,
				origin_datatype, target_rank, target_disp, target_count, target_datatype, win,
				target_pe, &accumulate_sum_8byte);
#endif
		return MPI_SUCCESS;
	}

	if ((origin_datatype == target_datatype)
			&& ((origin_datatype == MPI_DOUBLE) || (target_datatype == MPI_INT64_T))
			&& ((op == foMPI_BOR) || (op == foMPI_BAND) || (op == foMPI_BXOR))) { /* fast path */

		switch (op) {
		case foMPI_BAND:
#ifdef NDEBUG
			communication_and_datatype_handling( origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &accumulate_band_8byte , foMPI_ANY_TAG);
#else
			res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype,
					target_rank, target_disp, target_count, target_datatype, win, target_pe,
					&accumulate_band_8byte, foMPI_ANY_TAG);
#endif
			break;
		case foMPI_BOR:
#ifdef NDEBUG
			communication_and_datatype_handling( origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &accumulate_bor_8byte, foMPI_ANY_TAG);
#else
			res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype,
					target_rank, target_disp, target_count, target_datatype, win, target_pe,
					&accumulate_bor_8byte, foMPI_ANY_TAG);
#endif
			break;
		case foMPI_BXOR:
#ifdef NDEBUG
			communication_and_datatype_handling( origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &accumulate_bxor_8byte, foMPI_ANY_TAG);
#else
			res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype,
					target_rank, target_disp, target_count, target_datatype, win, target_pe,
					&accumulate_bxor_8byte, foMPI_ANY_TAG);
#endif
			break;
		}
		assert(res == MPI_SUCCESS);

	} else {

		if (op == foMPI_REPLACE) {
#ifdef XPMEM
			if ((win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC)
					&& (foMPI_onnode_rank_global_to_local(target_rank, win) != -1)) {
#ifdef NDEBUG
				communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win,
						target_pe, &xpmem_simple_put, foMPI_ANY_TAG);
#else
				res = communication_and_datatype_handling(origin_addr, origin_count,
						origin_datatype, target_rank, target_disp, target_count, target_datatype,
						win, target_pe, &xpmem_simple_put, foMPI_ANY_TAG);
#endif
			} else {
#ifdef NDEBUG
				communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win,
						target_pe, &dmapp_simple_put, foMPI_ANY_TAG);
#else
				res = communication_and_datatype_handling(origin_addr, origin_count,
						origin_datatype, target_rank, target_disp, target_count, target_datatype,
						win, target_pe, &dmapp_simple_put, foMPI_ANY_TAG);
#endif
			}
#else
#ifdef NDEBUG
			communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win,
					target_pe, &dmapp_simple_put, foMPI_ANY_TAG);
#else
			res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype,
					target_rank, target_disp, target_count, target_datatype, win, target_pe,
					&dmapp_simple_put, foMPI_ANY_TAG);
#endif
#endif
			assert(res == MPI_SUCCESS);

		} else {
			/* determine the target_addr and some other create-flavor dependent parameter */
			if (win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC) {
				win_ptr = win->dynamic_list[target_rank].win_ptr;
				win_ptr_seg = &(win->dynamic_list[target_rank].win_ptr_seg);
			} else {
				win_ptr = win->win_array[target_rank].win_ptr;
				win_ptr_seg = &(win->win_array[target_rank].win_ptr_seg);
			}

			check_basic_type(origin_datatype, &flag, &origin_basic_type);
			if (flag == 0) {
				return foMPI_DATATYPE_NOT_SUPPORTED;
			}

			check_basic_type(target_datatype, &flag, &target_basic_type);
			if (flag == 0) {
				return foMPI_DATATYPE_NOT_SUPPORTED;
			}

			if (origin_basic_type != target_basic_type) {
				return foMPI_ERROR_RMA_DATATYPE_MISMATCH;
			}

			/* process the datatypes */
			MPI_Type_size(origin_datatype, &origin_type_size);
			MPI_Type_size(origin_basic_type, &origin_basic_type_size);

			/* set the mutex */
			foMPI_Set_window_mutex(&status, target_pe, win_ptr, &(win_ptr_seg->dmapp), win);

			buffer = _foMPI_ALLOC(origin_count * origin_type_size);

			/* get the elements */
			elements_in_buffer = origin_count * origin_type_size / origin_basic_type_size;

#ifdef XPMEM
			if ((win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC)
					&& (foMPI_onnode_rank_global_to_local(target_rank, win) != -1)) {
#ifdef NDEBUG
				communication_and_datatype_handling(buffer, elements_in_buffer, origin_basic_type, target_rank, target_disp, target_count, target_datatype, win, target_pe, &xpmem_simple_get , foMPI_ANY_TAG );
#else
				res = communication_and_datatype_handling(buffer, elements_in_buffer,
						origin_basic_type, target_rank, target_disp, target_count, target_datatype,
						win, target_pe, &xpmem_simple_get, foMPI_ANY_TAG);
#endif
			} else {
#ifdef NDEBUG
				communication_and_datatype_handling(buffer, elements_in_buffer, origin_basic_type, target_rank, target_disp, target_count, target_datatype, win, target_pe, &dmapp_simple_get, foMPI_ANY_TAG );
#else
				res = communication_and_datatype_handling(buffer, elements_in_buffer,
						origin_basic_type, target_rank, target_disp, target_count, target_datatype,
						win, target_pe, &dmapp_simple_get, foMPI_ANY_TAG);
#endif
			}
#else

#ifdef NDEBUG
			communication_and_datatype_handling(buffer, elements_in_buffer, origin_basic_type, target_rank, target_disp, target_count, target_datatype, win, target_pe, &dmapp_simple_get , foMPI_ANY_TAG);
#else
			res = communication_and_datatype_handling(buffer, elements_in_buffer, origin_basic_type,
					target_rank, target_disp, target_count, target_datatype, win, target_pe,
					&dmapp_simple_get, foMPI_ANY_TAG);
#endif
#endif

			assert(res == MPI_SUCCESS);
			status = dmapp_gsync_wait();
			_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);

			/* operate on the elements */
			do {
				dcache_get_next_block(origin_basic_type, elements_in_buffer, &buffer_block_offset,
						origin_datatype, origin_count, &origin_block_offset, &transport_size,
						&last_block);
#ifdef NDEBUG
				foMPI_RMA_op( (char*) buffer+buffer_block_offset, (char*) origin_addr+origin_block_offset, (char*) buffer+buffer_block_offset, op, origin_basic_type, transport_size/origin_basic_type_size );
#else
				res = foMPI_RMA_op((char*) buffer + buffer_block_offset,
						(char*) origin_addr + origin_block_offset,
						(char*) buffer + buffer_block_offset, op, origin_basic_type,
						transport_size / origin_basic_type_size);
#endif
				assert(res == MPI_SUCCESS);

			} while (last_block == 0);

			/* put the elements back */
			/* since we must free the buffer, there is a global wait after the put operations */
#ifdef XPMEM
			if ((win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC)
					&& (foMPI_onnode_rank_global_to_local(target_rank, win) != -1)) {
#ifdef NDEBUG
				communication_and_datatype_handling( buffer, elements_in_buffer, origin_basic_type, target_rank, target_disp, target_count, target_datatype, win, target_pe, &xpmem_simple_put , foMPI_ANY_TAG);
#else
				res = communication_and_datatype_handling(buffer, elements_in_buffer,
						origin_basic_type, target_rank, target_disp, target_count, target_datatype,
						win, target_pe, &xpmem_simple_put, foMPI_ANY_TAG);
#endif
			} else {
#ifdef NDEBUG
				communication_and_datatype_handling( buffer, elements_in_buffer, origin_basic_type, target_rank, target_disp, target_count, target_datatype, win, target_pe, &dmapp_simple_put , foMPI_ANY_TAG);
#else
				res = communication_and_datatype_handling(buffer, elements_in_buffer,
						origin_basic_type, target_rank, target_disp, target_count, target_datatype,
						win, target_pe, &dmapp_simple_put, foMPI_ANY_TAG);
#endif
			}
#else
#ifdef NDEBUG
			communication_and_datatype_handling( buffer, elements_in_buffer, origin_basic_type, target_rank, target_disp, target_count, target_datatype, win, target_pe, &dmapp_simple_put , foMPI_ANY_TAG);
#else
			res = communication_and_datatype_handling(buffer, elements_in_buffer, origin_basic_type,
					target_rank, target_disp, target_count, target_datatype, win, target_pe,
					&dmapp_simple_put, foMPI_ANY_TAG);
#endif
#endif
			assert(res == MPI_SUCCESS);
			status = dmapp_gsync_wait();
			_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
			win->nbi_counter = 0;

			/* release the mutex if necessary */
			foMPI_Release_window_mutex(&status, target_pe, win_ptr, &(win_ptr_seg->dmapp), win);

			_foMPI_FREE(buffer);
		}
	}

#ifdef PAPI
	timing_record( 21 );
#endif

	return MPI_SUCCESS;
}

/* possible other implementation of MPI_Get_accumulate:
 * 1. get with blocks of target datatype in an extra buffer
 * 2. copy with blocks of the combination of target and result datatypes
 * 3. MPI_Op with blocks of origin datatype
 * 4. put with blocks of target datatype */

/* TODO: MPI_REPLACE/MPI_NO_OP without mutex, and nonblocking dmapp calls without wait */

int foMPI_Get_accumulate(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype,
		void *result_addr, int result_count, MPI_Datatype result_datatype, int target_rank,
		MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype, foMPI_Op op,
		foMPI_Win win) {

#ifdef PAPI
	timing_record( -1 );
#endif

	void* buffer = NULL; /* to get rid of the maybe-uninitialized warnings */
	dmapp_return_t status;
	foMPI_Win_desc_t* win_ptr = NULL; /* to get rid of the maybe-uninitialized warnings */
	fompi_seg_desc_t* win_ptr_seg = NULL; /* to get rid of the maybe-uninitialized warnings */
#ifndef NDEBUG
	int res = MPI_SUCCESS;
#endif

	int flag;

	MPI_Datatype origin_basic_type;

	int origin_type_size;

	char* buffer_ptr;

	int origin_basic_type_size;

	MPI_Aint result_block_offset;
	MPI_Aint origin_block_offset;

	uint64_t transport_size;

	int last_block;

	if ((target_rank == MPI_PROC_NULL) || (target_count == 0) || (result_count == 0)) {
		return MPI_SUCCESS;
	}

	assert((target_rank >= 0) && (target_rank < win->commsize));
	dmapp_pe_t target_pe = (dmapp_pe_t) win->group_ranks[target_rank];

	assert((op >= foMPI_SUM) && (op <= foMPI_NO_OP));

	if (win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC) {
		win_ptr = win->dynamic_list[target_rank].win_ptr;
		win_ptr_seg = &(win->dynamic_list[target_rank].win_ptr_seg);
	} else {
		win_ptr = win->win_array[target_rank].win_ptr;
		win_ptr_seg = &(win->win_array[target_rank].win_ptr_seg);
	}

	/* fast path for INT64_T */
	if ((origin_datatype == MPI_INT64_T) && (origin_datatype == target_datatype)
			&& (origin_datatype == result_datatype)
			&& (win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC) && (op == foMPI_SUM)) {

		MPI_Aint i, nelements;
		int64_t* acc_target_addr = (int64_t*) ((char*) win->win_array[target_rank].base
				+ target_disp * win->win_array[target_rank].disp_unit);
		fompi_seg_desc_t* seg_ptr = &(win->win_array[target_rank].seg);

		nelements = target_count * win->win_array[target_rank].disp_unit / 8;

		for (i = 0; i < nelements; i++) {
			status = dmapp_afadd_qw_nbi((int64_t*) result_addr + i, acc_target_addr + i,
					&(seg_ptr->dmapp), target_pe, *((int64_t*) origin_addr + i));
			_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
		}
		win->nbi_counter++; /* little hack, since we actually just need a flag to indicate that there are nbi operations in progress */
		return MPI_SUCCESS;
	}

	/* set the mutex if necessary */
	foMPI_Set_window_mutex(&status, target_pe, win_ptr, &(win_ptr_seg->dmapp), win);

	/* get the elements with blocks of the combination of the result and target datatypes */
#ifdef XPMEM
	if ((win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC)
			&& (foMPI_onnode_rank_global_to_local(target_rank, win) != -1)) {
#ifdef NDEBUG
		communication_and_datatype_handling(result_addr, result_count, result_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &xpmem_simple_get, foMPI_ANY_TAG);
#else
		res = communication_and_datatype_handling(result_addr, result_count, result_datatype,
				target_rank, target_disp, target_count, target_datatype, win, target_pe,
				&xpmem_simple_get, foMPI_ANY_TAG);
#endif
	} else {
#ifdef NDEBUG
		communication_and_datatype_handling(result_addr, result_count, result_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &dmapp_simple_get , foMPI_ANY_TAG);
#else
		res = communication_and_datatype_handling(result_addr, result_count, result_datatype,
				target_rank, target_disp, target_count, target_datatype, win, target_pe,
				&dmapp_simple_get, foMPI_ANY_TAG);
#endif
	}
#else
#ifdef NDEBUG
	communication_and_datatype_handling(result_addr, result_count, result_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &dmapp_simple_get, foMPI_ANY_TAG );
#else
	res = communication_and_datatype_handling(result_addr, result_count, result_datatype,
			target_rank, target_disp, target_count, target_datatype, win, target_pe,
			&dmapp_simple_get, foMPI_ANY_TAG);
#endif
#endif
	assert(res == MPI_SUCCESS);
	status = dmapp_gsync_wait();
	_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);

	/* operate on the elements */
	if ((op != foMPI_NO_OP) && (op != foMPI_REPLACE)) {

		/* process the datatypes */
		check_basic_type(origin_datatype, &flag, &origin_basic_type);
		assert(flag != 0);

		MPI_Type_size(origin_datatype, &origin_type_size);

		MPI_Type_size(origin_basic_type, &origin_basic_type_size);

		buffer = _foMPI_ALLOC(origin_count * origin_type_size);

		buffer_ptr = (char*) buffer;

		do {
			dcache_get_next_block(result_datatype, result_count, &result_block_offset,
					origin_datatype, origin_count, &origin_block_offset, &transport_size,
					&last_block);

#ifdef NDEBUG
			foMPI_RMA_op( buffer_ptr, (char*) origin_addr+origin_block_offset, (char*) result_addr+result_block_offset, op, origin_basic_type, transport_size/origin_basic_type_size );
#else
			res = foMPI_RMA_op(buffer_ptr, (char*) origin_addr + origin_block_offset,
					(char*) result_addr + result_block_offset, op, origin_basic_type,
					transport_size / origin_basic_type_size);
#endif
			assert(res == MPI_SUCCESS);

			buffer_ptr += transport_size;
		} while (last_block == 0);

		/* since we must free the buffer, there is a global wait after the put operations */
#ifdef XPMEM
		if ((win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC)
				&& (foMPI_onnode_rank_global_to_local(target_rank, win) != -1)) {
#ifdef NDEBUG
			communication_and_datatype_handling( buffer, origin_type_size*origin_count/origin_basic_type_size, origin_basic_type, target_rank, target_disp,
					target_count, target_datatype, win, target_pe, &xpmem_simple_put, foMPI_ANY_TAG );
#else
			res = communication_and_datatype_handling(buffer,
					origin_type_size * origin_count / origin_basic_type_size, origin_basic_type,
					target_rank, target_disp, target_count, target_datatype, win, target_pe,
					&xpmem_simple_put, foMPI_ANY_TAG);
#endif
		} else {
#ifdef NDEBUG
			communication_and_datatype_handling( buffer, origin_type_size*origin_count/origin_basic_type_size, origin_basic_type, target_rank, target_disp,
					target_count, target_datatype, win, target_pe, &dmapp_simple_put , foMPI_ANY_TAG);
#else
			res = communication_and_datatype_handling(buffer,
					origin_type_size * origin_count / origin_basic_type_size, origin_basic_type,
					target_rank, target_disp, target_count, target_datatype, win, target_pe,
					&dmapp_simple_put, foMPI_ANY_TAG);
#endif
		}
#else
#ifdef NDEBUG
		communication_and_datatype_handling( buffer, origin_type_size*origin_count/origin_basic_type_size, origin_basic_type, target_rank, target_disp,
				target_count, target_datatype, win, target_pe, &dmapp_simple_put , foMPI_ANY_TAG);
#else
		res = communication_and_datatype_handling(buffer,
				origin_type_size * origin_count / origin_basic_type_size, origin_basic_type,
				target_rank, target_disp, target_count, target_datatype, win, target_pe,
				&dmapp_simple_put, foMPI_ANY_TAG);
#endif
#endif
		assert(res == MPI_SUCCESS);
		status = dmapp_gsync_wait();
		_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
		win->nbi_counter = 0;

		_foMPI_FREE(buffer);
	} else {
		if (op == foMPI_REPLACE) {
			/* put the elements with blocks of the combination of the result and target datatypes */
#ifdef XPMEM
			if ((win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC)
					&& (foMPI_onnode_rank_global_to_local(target_rank, win) != -1)) {
#ifdef NDEBUG
				communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &xpmem_simple_put, foMPI_ANY_TAG);
#else
				res = communication_and_datatype_handling(origin_addr, origin_count,
						origin_datatype, target_rank, target_disp, target_count, target_datatype,
						win, target_pe, &xpmem_simple_put, foMPI_ANY_TAG);
#endif
			} else {
#ifdef NDEBUG
				communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &dmapp_simple_put , foMPI_ANY_TAG);
#else
				res = communication_and_datatype_handling(origin_addr, origin_count,
						origin_datatype, target_rank, target_disp, target_count, target_datatype,
						win, target_pe, &dmapp_simple_put, foMPI_ANY_TAG);
#endif
			}
#else
#ifdef NDEBUG
			communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &dmapp_simple_put , foMPI_ANY_TAG);
#else
			res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype,
					target_rank, target_disp, target_count, target_datatype, win, target_pe,
					&dmapp_simple_put, foMPI_ANY_TAG);
#endif
#endif
			assert(res == MPI_SUCCESS);
			status = dmapp_gsync_wait();
			_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
			win->nbi_counter = 0;
		}
	}

	/* release the mutex if necessary */
	foMPI_Release_window_mutex(&status, target_pe, win_ptr, &(win_ptr_seg->dmapp), win);

#ifdef PAPI
	timing_record( 22 );
#endif

	return MPI_SUCCESS;
}

int foMPI_Raccumulate(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype,
		int target_rank, MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype,
		foMPI_Op op, foMPI_Win win, foMPI_Request *request) {

	int res = MPI_SUCCESS;

	if ((origin_count > 0) && (target_count > 0)) {
		res = foMPI_Accumulate(origin_addr, origin_count, origin_datatype, target_rank, target_disp,
				target_count, target_datatype, op, win);
	}

	*request = _foMPI_ALLOC(sizeof(foMPI_Request_t));
	(*request)->type = _foMPI_REQUEST_NOT_PERSISTENT;
	foMPI_Rma_request_t * args = (foMPI_Rma_request_t *) _foMPI_ALLOC(sizeof(foMPI_Rma_request_t));
	args->win = win;
	args->elements = target_count;
	args->datatype = target_datatype;
	args->source = target_rank;
	(*request)->extra_state = (void *) args;
	(*request)->test_fun = &simple_rma_test;
	(*request)->wait_fun = &simple_rma_wait;
	(*request)->free_fun = &free_wrapper;
	(*request)->cancel_fun = NULL;

	return res;
}

int foMPI_Rget_accumulate(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype,
		void *result_addr, int result_count, MPI_Datatype result_datatype, int target_rank,
		MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype, foMPI_Op op,
		foMPI_Win win, foMPI_Request *request) {

	int res = MPI_SUCCESS;

	if ((target_count > 0) && (result_count > 0)) {
		res = foMPI_Get_accumulate(origin_addr, origin_count, origin_datatype, result_addr,
				result_count, result_datatype, target_rank, target_disp, target_count,
				target_datatype, op, win);
	}

	*request = _foMPI_ALLOC(sizeof(foMPI_Request_t));
	(*request)->type = _foMPI_REQUEST_NOT_PERSISTENT;
	foMPI_Rma_request_t * args = (foMPI_Rma_request_t *) _foMPI_ALLOC(sizeof(foMPI_Rma_request_t));
	args->win = win;
	args->elements = target_count;
	args->datatype = target_datatype;
	args->source = target_rank;
	(*request)->extra_state = (void *) args;
	(*request)->test_fun = &simple_rma_test;
	(*request)->wait_fun = &simple_rma_wait;
	(*request)->free_fun = &free_wrapper;
	(*request)->cancel_fun = NULL;

	return res;
}

int foMPI_Fetch_and_op(const void *origin_addr, void *result_addr, MPI_Datatype datatype,
		int target_rank, MPI_Aint target_disp, foMPI_Op op, foMPI_Win win) {
#ifdef PAPI
	timing_record( -1 );
#endif

	int found;
	void* buffer;
	void* target_addr;
	int type_size;
	dmapp_return_t status;
	foMPI_Win_desc_t* win_ptr = NULL; /* to get rid of the maybe-uninitialized warnings */
	fompi_seg_desc_t* win_ptr_seg = NULL; /* to get rid of the maybe-uninitialized warnings */
	fompi_seg_desc_t* seg_ptr;
	foMPI_Win_dynamic_element_t* current;
	int res = MPI_SUCCESS;

	if (target_rank == MPI_PROC_NULL) {
		return MPI_SUCCESS;
	}

	assert((target_rank >= 0) && (target_rank < win->commsize));
	dmapp_pe_t target_pe = (dmapp_pe_t) win->group_ranks[target_rank];

	MPI_Type_size(datatype, &type_size);

	if (win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC) {
		/* check already if the the requested memory region is within the boundaries */

		/* checks if the local information about the remote dynamic window are up-to-date: if not, they are fetched */
		get_dynamic_window_regions(&status, target_rank, target_pe, win);

		/* check local list for entry */
		current = win->dynamic_list[target_rank].remote_mem_regions;
		found = 0;
		if (current != NULL) {
			if ((target_disp >= current->base)
					&& (target_disp + type_size <= (current->base + current->size))) {
				found = 1;
			}
			while ((current->next != NULL) && (found == 0)) {
				current = current->next;
				if ((target_disp >= current->base)
						&& (target_disp + type_size <= (current->base + current->size))) {
					found = 1;
				}
			}
		}

		if (found == 1) {
			target_addr = (char*) MPI_BOTTOM + target_disp;
			seg_ptr = &(current->seg);
		} else {
			return foMPI_ERROR_RMA_WIN_MEM_NOT_FOUND;
		}

		win_ptr = (void*) win->dynamic_list[target_rank].win_ptr;
		win_ptr_seg = &(win->dynamic_list[target_rank].win_ptr_seg);
	} else {
		target_addr = (char*) win->win_array[target_rank].base
				+ target_disp * win->win_array[target_rank].disp_unit;
		seg_ptr = &(win->win_array[target_rank].seg);

		win_ptr = (void*) win->win_array[target_rank].win_ptr;
		win_ptr_seg = &(win->win_array[target_rank].win_ptr_seg);
	}

	/* set the mutex */
	foMPI_Set_window_mutex(&status, target_pe, win_ptr, &(win_ptr_seg->dmapp), win);

	/* get the element */
	/* TODO: maybe use the according DMAPP_{SIZE}? */
	status = dmapp_get(result_addr, target_addr, &(seg_ptr->dmapp), target_pe, type_size,
			DMAPP_BYTE);
	_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);

	/* operate on the elements */
	if (op == foMPI_REPLACE) {
		/* TODO: without mutex, put can be non-blocking with origin_addr */
		buffer = (void*) origin_addr;
	} else if (op == foMPI_NO_OP) {
	} else {
		buffer = _foMPI_ALLOC(type_size);
		res = foMPI_RMA_op(buffer, (void*) origin_addr, result_addr, op, datatype, 1);
	}

	/* put the element back */
	/* since we must free the buffer, the put operation is blocking */
	if ((op != foMPI_NO_OP) && (res == MPI_SUCCESS)) {
		status = dmapp_put(target_addr, &(seg_ptr->dmapp), target_pe, buffer, type_size,
				DMAPP_BYTE);
		_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
	}
	/* release the mutex if necessary */
	foMPI_Release_window_mutex(&status, target_pe, win_ptr, &(win_ptr_seg->dmapp), win);

	if ((op != foMPI_REPLACE) && (op != foMPI_NO_OP)) {
		_foMPI_FREE(buffer);
	}

#ifdef PAPI
	timing_record( 20 );
#endif

	return res;
}

int foMPI_Compare_and_swap(const void *origin_addr, const void *compare_addr, void *result_addr,
		MPI_Datatype datatype, int target_rank, MPI_Aint target_disp, foMPI_Win win) {
#ifdef PAPI
	timing_record( -1 );
#endif

	int size;
	void* target_addr;
	int found;
	dmapp_return_t status;
	foMPI_Win_desc_t* win_ptr = NULL; /* to get rid of the maybe-uninitialized warnings */
	fompi_seg_desc_t* win_ptr_seg = NULL; /* to get rid of the maybe-uninitialized warnings */
	fompi_seg_desc_t* seg_ptr;
	foMPI_Win_dynamic_element_t* current;

	if (target_rank == MPI_PROC_NULL) {
		return MPI_SUCCESS;
	}

	assert((target_rank >= 0) && (target_rank < win->commsize));
	dmapp_pe_t target_pe = (dmapp_pe_t) win->group_ranks[target_rank];

	MPI_Type_size(datatype, &size);

	if (win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC) {
		/* check already if the the requested memory region is within the boundaries */

		/* checks if the local information about the remote dynamic window are up-to-date: if not, they are fetched */
		get_dynamic_window_regions(&status, target_rank, target_pe, win);

		/* check local list for entry */
		current = win->dynamic_list[target_rank].remote_mem_regions;
		found = 0;
		if (current != NULL) {
			if ((target_disp >= current->base)
					&& (target_disp + size <= (current->base + current->size))) {
				found = 1;
			}
			while ((current->next != NULL) && (found == 0)) {
				current = current->next;
				if ((target_disp >= current->base)
						&& (target_disp + size <= (current->base + current->size))) {
					found = 1;
				}
			}
		}

		if (found == 1) {
			target_addr = (char*) MPI_BOTTOM + target_disp;
			seg_ptr = &(current->seg);
		} else {
			return foMPI_ERROR_RMA_WIN_MEM_NOT_FOUND;
		}
	} else {
		target_addr = (char*) win->win_array[target_rank].base
				+ target_disp * win->win_array[target_rank].disp_unit;
		seg_ptr = &(win->win_array[target_rank].seg);
	}

	switch (size) {
	case 8:
		status = dmapp_acswap_qw(result_addr, target_addr, &(seg_ptr->dmapp), target_pe,
				*(int64_t *) compare_addr, *(int64_t *) origin_addr);
		_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
		break;
	default:
		/* in the general case, we need to sync */
		if (win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC) {
			win_ptr = (void*) win->dynamic_list[target_rank].win_ptr;
			win_ptr_seg = &(win->dynamic_list[target_rank].win_ptr_seg);
		} else {
			win_ptr = (void*) win->win_array[target_rank].win_ptr;
			win_ptr_seg = &(win->win_array[target_rank].win_ptr_seg);
		}
		/* set the mutex */
		foMPI_Set_window_mutex(&status, target_pe, win_ptr, &(win_ptr_seg->dmapp), win);

		/* get the elements */
		/* TODO: maybe use the right dmapp_size */
		status = dmapp_get(result_addr, target_addr, &(seg_ptr->dmapp), target_pe, size,
				DMAPP_BYTE);
		_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);

		if (memcmp(result_addr, compare_addr, size) == 0) {
			status = dmapp_put(target_addr, &(seg_ptr->dmapp), target_pe, (void*) origin_addr, size,
					DMAPP_BYTE);
			_check_dmapp_status(status, DMAPP_RC_SUCCESS, (char*) __FILE__, __LINE__);
		}

		/* release the mutex if necessary */
		foMPI_Release_window_mutex(&status, target_pe, win_ptr, &(win_ptr_seg->dmapp), win);
		break;
	}

#ifdef PAPI
	timing_record( 18 );
#endif

	return MPI_SUCCESS;
}
#ifdef UGNI

int foMPI_Put_notify(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype,
		int target_rank, MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype,
		foMPI_Win win, int tag) {

#ifdef PAPI
	timing_record( -1 );
#endif
	if (tag == foMPI_ANY_TAG) {
		return foMPI_Put(origin_addr, origin_count, origin_datatype, target_rank, target_disp,
				target_count, target_datatype, win);
	}
	int res;

	res = _foMPI_Rma_notify(origin_addr, origin_count, origin_datatype, target_rank, target_disp,
			target_count, target_datatype, win, tag, _foMPI_PUT);

#ifdef PAPI
	timing_record( 3 );
#endif

	return res;
}

int foMPI_Get_notify(void *origin_addr, int origin_count, MPI_Datatype origin_datatype,
		int target_rank, MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype,
		foMPI_Win win, int tag) {

#ifdef PAPI
	timing_record( -1 );
#endif
	if (tag == foMPI_ANY_TAG) {
		return foMPI_Get(origin_addr, origin_count, origin_datatype, target_rank, target_disp,
				target_count, target_datatype, win);
	}
	int res;

	res = _foMPI_Rma_notify(origin_addr, origin_count, origin_datatype, target_rank, target_disp,
			target_count, target_datatype, win, tag, _foMPI_GET);

#ifdef PAPI
	timing_record( 7 );
#endif

	return res;
}

int foMPI_Rput_notify(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype,
		int target_rank, MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype,
		foMPI_Win win, foMPI_Request *request, int tag) {

	if (tag == foMPI_ANY_TAG) {
		return foMPI_Rput(origin_addr, origin_count, origin_datatype, target_rank, target_disp,
				target_count, target_datatype, win, request);
	}

	return _foMPI_Rrma_notify(origin_addr, origin_count, origin_datatype, target_rank, target_disp,
			target_count, target_datatype, win, request, tag, _foMPI_PUT);
}

int foMPI_Rget_notify(void *origin_addr, int origin_count, MPI_Datatype origin_datatype,
		int target_rank, MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype,
		foMPI_Win win, foMPI_Request *request, int tag) {

	if (tag == foMPI_ANY_TAG) {
		return foMPI_Rget(origin_addr, origin_count, origin_datatype, target_rank, target_disp,
				target_count, target_datatype, win, request);
	}

	return _foMPI_Rrma_notify(origin_addr, origin_count, origin_datatype, target_rank, target_disp,
			target_count, target_datatype, win, request, tag, _foMPI_GET);
}

static inline int _foMPI_Rrma_notify(const void *origin_addr, int origin_count,
		MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp, int target_count,
		MPI_Datatype target_datatype, foMPI_Win win, foMPI_Request *request, int tag, int operation) {

	if ((target_rank == MPI_PROC_NULL)) {
		return MPI_SUCCESS;
	}

	assert((target_rank >= 0) && (target_rank < win->commsize));

	*request = _foMPI_ALLOC(sizeof(foMPI_Request_t));
	(*request)->type = _foMPI_REQUEST_NOT_PERSISTENT;
	foMPI_Rma_request_t * args = (foMPI_Rma_request_t *) _foMPI_ALLOC(sizeof(foMPI_Rma_request_t));
	args->win = win;
	args->elements = target_count;
	args->datatype = target_datatype;
	args->source = target_rank;
	(*request)->extra_state = (void *) args;
	(*request)->test_fun = &simple_rma_test;
	(*request)->wait_fun = &simple_rma_wait;
	(*request)->free_fun = &free_wrapper;
	(*request)->cancel_fun = NULL;

	return _foMPI_Rma_notify(origin_addr, origin_count, origin_datatype, target_rank, target_disp,
			target_count, target_datatype, win, tag, operation);
}

static inline int _foMPI_Rma_notify(const void *origin_addr, int origin_count,
		MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp, int target_count,
		MPI_Datatype target_datatype, foMPI_Win win, int tag, int operation) {

	int res;

	if ((target_rank == MPI_PROC_NULL)) {
		return MPI_SUCCESS;
	}

	assert((target_rank >= 0) && (target_rank < win->commsize));
	dmapp_pe_t target_pe = (dmapp_pe_t) win->group_ranks[target_rank];

#ifdef XPMEM
	dmapp_return_t status;
	int origin_type_size;
	MPI_Aint origin_offset = (MPI_Aint) origin_addr;
	MPI_Aint target_offset;
	MPI_Type_size(origin_datatype, &origin_type_size);

	int local_rank = foMPI_onnode_rank_global_to_local(target_rank, win);
	/* determine the target_addr and some other create-flavor dependent parameter */
	if (win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC && operation == _foMPI_PUT
			&& local_rank != -1 && (origin_count * origin_type_size) <= _foMPI_XPMEM_NOTIF_INLINE_BUFF_SIZE) {
		target_offset = (MPI_Aint) win->xpmem_array[local_rank].base
				+ target_disp * win->win_array[target_rank].disp_unit;

		/* TODO: other intrinsic datatypes */
		if ((origin_datatype == target_datatype)
				&& ((origin_datatype == MPI_CHAR) || (origin_datatype == MPI_UINT64_T)
						|| (origin_datatype == MPI_DOUBLE) || (origin_datatype == MPI_INT)
						|| (origin_datatype == MPI_BYTE))) {
			/* fast path for intrinsic datatypes */
			/*on node PE, size fits notification cache line, is a put,*/
			xpmem_notif_push_and_data(win->commrank, target_offset, origin_count * origin_type_size,
					target_rank, local_rank, origin_offset, tag, win);

			return MPI_SUCCESS;
		}
	}

	/*XPMEM transfer- no fast path , we need to send the notification after*/
	if ((win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC)
			&& (local_rank != -1)) {
		res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype,
				target_rank, target_disp, target_count, target_datatype, win, target_pe,
				(operation == _foMPI_PUT) ? &xpmem_simple_put : &xpmem_simple_get, foMPI_ANY_TAG);
		/*sync*/
		/* XPMEM completion */
		__sync_synchronize();
		uint16_t myrank = win->commrank;
		uint16_t tag_to_send = tag;
		xpmem_notif_push(myrank, tag_to_send, target_rank, local_rank, win);
		//foMPI_Win_flush_all(win);
		/*zero byte put (notification only)*/
		//res = communication_and_datatype_handling_ugni_put_notify_only(origin_addr, 0,
		//		origin_datatype, target_rank, target_disp, 0, target_datatype, win, target_pe, tag);
	} else {
#endif
		/*ugni GET supports only get aligned to 4 bytes*/
		if (operation == _foMPI_GET) {
			res = communication_and_datatype_handling_ugni_get_notify_only(origin_addr,
					origin_count, origin_datatype, target_rank, target_disp, target_count,
					target_datatype, win, target_pe, tag);
		} else {
			res = communication_and_datatype_handling_ugni_put_notify_only(origin_addr,
					origin_count, origin_datatype, target_rank, target_disp, target_count,
					target_datatype, win, target_pe, tag);
		}
#ifdef XPMEM
	}
#endif
	return res;
}

//static void simple_put(dmapp_pe_t target_pe, MPI_Aint target_offset, MPI_Aint origin_offset,
//	fompi_seg_desc_t* seg_ptr, MPI_Aint size, foMPI_Win win
static inline void ugni_simple_put(dmapp_pe_t target_pe, MPI_Aint target_offset,
		MPI_Aint origin_offset, fompi_seg_desc_t* src_seg_ptr, fompi_seg_desc_t* dst_seg_ptr,
		MPI_Aint size, foMPI_Win win, int tag) {

	ugni_simple_rma(target_pe, target_offset, origin_offset, src_seg_ptr, dst_seg_ptr, size, win,
			tag, _foMPI_PUT);
}

static inline void ugni_simple_get(dmapp_pe_t target_pe, MPI_Aint target_offset,
		MPI_Aint origin_offset, fompi_seg_desc_t* src_seg_ptr, fompi_seg_desc_t* dst_seg_ptr,
		MPI_Aint size, foMPI_Win win, int tag) {

	ugni_simple_rma(target_pe, target_offset, origin_offset, src_seg_ptr, dst_seg_ptr, size, win,
			tag, _foMPI_GET);
}

static inline void ugni_simple_rma(dmapp_pe_t target_pe, MPI_Aint target_offset,
		MPI_Aint origin_offset, fompi_seg_desc_t* src_seg_ptr, fompi_seg_desc_t* dest_seg_ptr,
		MPI_Aint size, foMPI_Win win, int tag, int operation) {

	gni_return_t status_gni = GNI_RC_SUCCESS;

	gni_post_descriptor_t *data_desc = _foMPI_Comm_get_ugni_data_descriptor(win->fompi_comm);

	assert(data_desc != NULL);

	uint64_t origin_addr = (uint64_t) origin_offset;
	uint64_t target_addr = (uint64_t) target_offset;
	int seg_needs_unregister = 0;
	fompi_seg_desc_t src_reg_ptr;
	uint32_t id_encoded;
	/*encoding the rank of the Pe on the comm_world */
	_foMPI_EncodeID((uint16_t) win->commrank, (uint16_t) tag, &id_encoded);

	/*set id notification */
	uint32_t local_event_id = id_encoded;
	uint32_t remote_event_id = id_encoded;
	//TODO: OPTIMIZE
	status_gni = GNI_EpSetEventData(win->fompi_comm->endpoint_handles_array[target_pe],
			local_event_id, remote_event_id);
	_check_gni_status(status_gni, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);

	/*common parameters used by both FMA and BTE */
	/*if tag = foMPI_ANY_TAG we disable the remote notification*/
	data_desc->cq_mode =
			(tag == foMPI_ANY_TAG) ?
					GNI_CQMODE_GLOBAL_EVENT : (GNI_CQMODE_GLOBAL_EVENT | GNI_CQMODE_REMOTE_EVENT);
	data_desc->dlvr_mode = GNI_DLVMODE_PERFORMANCE;
	data_desc->local_addr = origin_addr;

	_foMPI_TRACE_LOG(3, "ugni_simple_rma -> seg_ptr = %p  \n", src_seg_ptr);
	if ( /*size > 0 &&*/size < _foMPI_RDMA_THRESHOLD && operation == _foMPI_PUT) {
		/*source segment not specified but not needed by FMA Put */
		data_desc->local_mem_hndl.qword1 = 0;
		data_desc->local_mem_hndl.qword2 = 0;
	} else if (src_seg_ptr != NULL
			&& _foMPI_is_addr_in_seg((void *) data_desc->local_addr, src_seg_ptr)) {
		/*source segment specified, and address in the segment*/
		data_desc->local_mem_hndl = src_seg_ptr->ugni;
	} else /* if (src_seg_ptr == NULL)*/{
		/*source segment not specified, but needed by fma, we have to register memory within the nic*/
		seg_needs_unregister = 1;
		_foMPI_mem_register((void *) origin_addr, (uint64_t) size, &src_reg_ptr, win);
		data_desc->local_mem_hndl = src_reg_ptr.ugni;
		local_event_id += win->fompi_comm->commsize;
	} /*else {
	 //TODO: Check if segment is registered but is not part of the window.
	 }*/

	/* The address of the remote region. This is the target for PUTs and source for GETs.
	 * Must be 4-byte aligned for GET operations and 8-byte aligned for AMOs.  */
	data_desc->remote_addr = target_addr;
	data_desc->remote_mem_hndl = dest_seg_ptr->ugni;

#ifndef NDEBUG
	/*check memory boundaries*/
	int check = _foMPI_is_addr_in_seg((void *) data_desc->remote_addr, dest_seg_ptr)
			&& _foMPI_is_addr_in_seg((void *) (data_desc->remote_addr + (uint64_t) size),
					dest_seg_ptr);

	_foMPI_TRACE_LOG(2, "ugni_simple_rma CHECK REMOTE BOUNDS -> %s \n", check ? "OK" : "ERROR");

	check = _foMPI_is_addr_in_seg((void *) data_desc->local_addr,
			(src_seg_ptr) ? src_seg_ptr : &src_reg_ptr)
			&& _foMPI_is_addr_in_seg((void *) (data_desc->local_addr + (uint64_t) size),
					(src_seg_ptr) ? src_seg_ptr : &src_reg_ptr);

	_foMPI_TRACE_LOG(2, "ugni_simple_rma CHECK LOCAL BOUNDS -> %s \n", check ? "OK" : "ERROR");

	gni_return_t mem = GNI_RC_SUCCESS;
	gni_mem_handle_t hdl;
	hdl.qword1 = 0;
	hdl.qword2 = 0;
	hdl = data_desc->local_mem_hndl;
	uint64_t address, lenght;
	int i = 0;
	while (mem == GNI_RC_SUCCESS) {
		mem = GNI_MemQueryHndls(win->fompi_comm->nic_handle, 0, &hdl, &address, &lenght);
		_foMPI_TRACE_LOG(2,
				"ugni_simple_rma CHECK LOCAL (%s) handle[%d] -> addr:%"PRIu64" len:%"PRIu64" \n",
				gni_err_str[mem], i, address, lenght);
		if (mem == GNI_RC_SUCCESS) {
			if (data_desc->local_addr >= address && data_desc->local_addr < address + lenght) {
				_foMPI_TRACE_LOG(2, "ugni_simple_rma handle[%d] -> source addr in this segment \n",
						i);
			}
			if (data_desc->local_addr + data_desc->length > address + lenght) {
				_foMPI_TRACE_LOG(2, "ugni_simple_rma handle[%d] -> ERROR lenght outside segment \n",
						i);
			}
			i++;
		}
	}

#endif

	/*
	 * Number of bytes to move.
	 * Must be a multiple of 4-bytes for GETs and multiple of 8-bytes for AMOs.
	 * */
	data_desc->length = (uint64_t) size;
	data_desc->post_id = local_event_id;
	//data_desc->cqwrite_value = id_encoded;
#ifdef UGNI_WIN_RELATED_SRC_CQ
	/*
	 *  If set, the NIC delivers the source completion events related to this transaction to
	 * the specified completion queue instead of the default one. Feature noot available on Aries
	 * */
	data_desc->src_cq_hndl = win->source_cq_handle;
	/* to avoid the overfow of the local completion queue */
	if (win->counter_ugni_nbi >= win->number_of_source_cq_entries) {
		//TODO: you can flush only ugni
		foMPI_Win_flush_all(win);
	}

#endif

	/* choosing between PostCQ, FMA or BTE */

	/*if (size == 0) {

	 data_desc->type = GNI_POST_CQWRITE;
	 data_desc->cqwrite_value = 0 ; //bigger than id_encoded
	 data_desc->cqwrite_value = id_encoded;

	 _foMPI_TRACE_LOG(3, "GNI_PostCqWrite  EP: %p  \n",
	 win->fompi_comm->endpoint_handles_array[target_pe]);

	 //  Send the transaction.


	 status_gni = GNI_PostCqWrite(win->fompi_comm->endpoint_handles_array[target_pe], data_desc);
	 _check_gni_status(status_gni, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);

	 } else */
	/*the code before was used to try the postCQ method for sending notifications only*/
	if (size < _foMPI_RDMA_THRESHOLD) {
		/*
		 * Setup the data request.
		 *    type is FMA_PUT.
		 *    cq_mode states what type of events should be sent.
		 *         GNI_CQMODE_GLOBAL_EVENT allows for the sending of an event
		 *             to the local node after the receipt of the data.
		 *         GNI_CQMODE_REMOTE_EVENT allows for the sending of an event
		 *             to the remote node after the receipt of the data.
		 *    dlvr_mode states the delivery mode.
		 *    local_addr is the address of the sending buffer.
		 *    local_mem_hndl is the memory handle of the sending buffer.
		 *    remote_addr is the the address of the receiving buffer.
		 *    remote_mem_hndl is the memory handle of the receiving buffer.
		 *    length is the amount of data to transfer.
		 */

		data_desc->type = (operation == _foMPI_PUT) ? GNI_POST_FMA_PUT : GNI_POST_FMA_GET;

		_foMPI_TRACE_LOG(3, "GNI_PostFma    local_mhd: %"PRIu64"  remote_mhd:%"PRIu64" EP: %p \n",
				data_desc->local_mem_hndl.qword1, data_desc->remote_mem_hndl.qword1,
				win->fompi_comm->endpoint_handles_array[target_pe]);

		/*
		 * Send the data.
		 */

		status_gni = GNI_PostFma(win->fompi_comm->endpoint_handles_array[target_pe], data_desc);
		_check_gni_status(status_gni, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);

	} else {

		/*
		 * The PostRdma function adds a descriptor to the tail of the RDMA queue and returns immediately.
		 *  The maximum RDMA post length is ((2^32)-1).
		 *  Note that DMAPP supports RDMA transfers of ((2^32)-1) and larger.
		 *  PUTs have no alignment restrictions.
		 *  On Gemini systems, GETs require 4-byte alignment for local address, remote address, and the length. Aries systems differ in that GETS do not require 4-byte aligned local address.
		 *
		 * Setup the data request.
		 *    type is RDMA_PUT.
		 *    cq_mode states what type of events should be sent.
		 *         GNI_CQMODE_GLOBAL_EVENT allows for the sending of an event
		 *             to the local node after the receipt of the data.
		 *         GNI_CQMODE_REMOTE_EVENT allows for the sending of an event
		 *             to the remote node after the receipt of the data.
		 *    dlvr_mode states the delivery mode.
		 *    local_addr is the address of the sending buffer.
		 *    local_mem_hndl is the memory handle of the sending buffer.
		 *    remote_addr is the the address of the receiving buffer.
		 *    remote_mem_hndl is the memory handle of the receiving buffer.
		 *    length is the amount of data to transfer.
		 *    rdma_mode states how the request will be handled.
		 *    src_cq_hndl is the source complete queue handle.
		 */

		data_desc->type = (operation == _foMPI_PUT) ? GNI_POST_RDMA_PUT : GNI_POST_RDMA_GET;
		data_desc->rdma_mode = (operation == _foMPI_PUT) ? _foMPI_PUT_RDMA_MODE : 0;

		_foMPI_TRACE_LOG(3, "GNI_PostRdma    local_mhd: %"PRIu64"  remote_mhd:%"PRIu64" EP: %p\n",
				data_desc->local_mem_hndl.qword1, data_desc->remote_mem_hndl.qword1,
				win->fompi_comm->endpoint_handles_array[target_pe]);

		/*
		 * Send the data.
		 */

		status_gni = GNI_PostRdma(win->fompi_comm->endpoint_handles_array[target_pe], data_desc);
		_check_gni_status(status_gni, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);

	}
	_foMPI_TRACE_LOG(2,
			"GNI_Post{CQWRITE|FMA|RDMA} local mem:  [0x%lx , 0x%lx] remote mem: [0x%lx , 0x%lx] data length: %"PRIu64" post_id: %lu\n",
			(char* )data_desc->local_addr, ((char* ) data_desc->local_addr) + (uint64_t )size,
			(char* )data_desc->remote_addr, ((char* )data_desc->remote_addr) + (uint64_t )size,
			(uint64_t ) size, data_desc->post_id);
	_foMPI_TRACE_LOG(2,
			"GNI_Post{CQWRITE|FMA|RDMA}  op: %s  targetPE: %d  tag: %d notification:%s successful\n",
			(operation == _foMPI_PUT ) ? "PUT" : "GET", target_pe, tag,
			(tag == foMPI_ANY_TAG) ? "no" : "yes");

	_foMPI_TRACE_LOG_FLUSH;
#ifdef _foMPI_UGNI_WIN_RELATED_SRC_CQ
	win->counter_ugni_nbi++;
#endif
	if (seg_needs_unregister) {
		/*detach memory from the nic if needed*/
		foMPI_Win_flush_all(win);
		_foMPI_mem_unregister(&src_reg_ptr, win);
	}

}

inline int _foMPI_EncodeID(uint16_t rank, uint16_t id_msg, uint32_t *id_encoded) {

	*id_encoded = (rank << 16) + id_msg;
	return MPI_SUCCESS;
}

#endif
