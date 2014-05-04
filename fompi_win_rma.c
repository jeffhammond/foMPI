// Copyright (c) 2012 The Trustees of University of Illinois. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <dmapp.h>
#include <stdlib.h>
#include <string.h>

#include "mpitypes.h"
#include "fompi.h"

static inline void sse_memcpy(char *to, const char *from, size_t len)
{
 unsigned long src = (unsigned long)from;
 unsigned long dst = (unsigned long)to;
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
int nb_counter;

dcache_blocks_t dcache_blocks[10];
int dcache_last_entry;
int dcache_origin_last_entry;
int dcache_target_last_entry;

/* some helper functions */
static int find_seg_and_size( foMPI_Win_dynamic_element_t* current, MPI_Aint size, MPI_Aint target_offset, dmapp_seg_desc_t** seg_ptr, uint64_t* real_size ) {
  
  int found = 0;

  if( current != NULL ) {
    if ( ( target_offset >= current->base ) && ( target_offset < (current->base+current->size) ) ) {
      found = 1;
    }
    while( (current->next != NULL) && (found == 0) ) {
      current = current->next;
      if ( ( target_offset >= current->base ) && ( target_offset < (current->base+current->size) ) ) {
        found = 1;
      }
    }
  }

  if (found == 0) {
    return foMPI_ERROR_RMA_WIN_MEM_NOT_FOUND;
  }
  
  *seg_ptr = &(current->seg);
  if ( (target_offset+size) > (current->base+current->size) ) {
    *real_size = current->base + current->size - target_offset;
  } else {
    *real_size = size;		
  }

  return MPI_SUCCESS;
}

static void get_dynamic_window_regions( dmapp_return_t* status, int target_rank, dmapp_pe_t target_pe, foMPI_Win win ) {

  int64_t mutex;
  uint32_t remote_dyn_id;
  foMPI_Win_dynamic_element_t* current;
  foMPI_Win_dynamic_element_t* temp;
  int times;
  foMPI_Win_ptr_seg_comb_t remote_list_head;

  /* get the mutex */
  do {
    /* TODO: busy waiting */
    *status = dmapp_acswap_qw( &mutex, (void*) &(win->dynamic_list[target_rank].win_ptr->dynamic_mutex), &(win->dynamic_list[target_rank].win_ptr_seg),
                target_pe, (int64_t) foMPI_MUTEX_NONE, (int64_t) win->commrank );
    _check_status(*status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
  } while(mutex != foMPI_MUTEX_NONE);
    
  /* get the current remote id */
  *status = dmapp_get( &remote_dyn_id, (void*) &(win->dynamic_list[target_rank].win_ptr->dynamic_id), &(win->dynamic_list[target_rank].win_ptr_seg), target_pe, 1, DMAPP_QW );
  _check_status(*status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);

  if ( remote_dyn_id != win->dynamic_list[target_rank].dynamic_id ) {

    win->dynamic_list[target_rank].dynamic_id = remote_dyn_id;
 
    /* delete the current list */
    if ( win->dynamic_list[target_rank].remote_mem_regions != NULL ) {
      current = win->dynamic_list[target_rank].remote_mem_regions;
   	  while( current->next != NULL ) {
        temp = current;
	      current = current->next;
	      free(temp);
	    }
      free( current );
	    win->dynamic_list[target_rank].remote_mem_regions = NULL;
    }

    assert( win->dynamic_list[target_rank].remote_mem_regions == NULL );

	  times = sizeof(foMPI_Win_ptr_seg_comb_t) / 8; /* TODO: what if the size is not exactly divisible by eight */
    
	  /* get new list */
	  /* get the pointer to the list */
    *status = dmapp_get( &remote_list_head, &(win->dynamic_list[target_rank].win_ptr->win_dynamic_mem_regions_seg), &(win->dynamic_list[target_rank].win_ptr_seg), target_pe, times, DMAPP_QW );
	  _check_status(*status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
	  if ( remote_list_head.ptr != NULL ) {
	    /* get the first element */
      temp = malloc( sizeof(foMPI_Win_dynamic_element_t) );
	    times = sizeof(foMPI_Win_dynamic_element_t) / 8 ; /* TODO: what if the size is not exactly divisible by eight */
      
	    *status = dmapp_get( temp, remote_list_head.ptr, &remote_list_head.seg, target_pe, times, DMAPP_QW );
	    _check_status(*status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
	    win->dynamic_list[target_rank].remote_mem_regions = temp;
	    current = temp;

      while( current->next != NULL ) { /* get remaining elements */
        temp = malloc( sizeof(foMPI_Win_dynamic_element_t) );
		    *status = dmapp_get( temp, (void*) current->next, &current->next_seg, target_pe, times, DMAPP_QW );
		    _check_status(*status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
		    current->next = temp;
		    current = current->next;
	    }
	  } /* no else needed since win->dynamic_list[target_rank].remote_mem_regions is already NULL */
  } 

  /* release the mutex */
  /* TODO: nonblocking ? */
  /* we have to do this with an atomic operation, with a simple put it was deadlocking some times */
  *status = dmapp_acswap_qw( &mutex, (void*) &(win->dynamic_list[target_rank].win_ptr->dynamic_mutex), &(win->dynamic_list[target_rank].win_ptr_seg),
              target_pe, (int64_t) win->commrank, (int64_t) foMPI_MUTEX_NONE );
  _check_status(*status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
 
}

/* pointer to the last two entries */
static int dcache_init_blocks( MPI_Datatype dtype, int count, dcache_blocks_t* blocks ) {

  int number_blocks;
  MPI_Aint lb;
  MPI_Aint extent;
  MPI_Aint true_extent;
  MPI_Aint count_extent;

  MPIT_Type_init( dtype );

  MPI_Type_get_extent( dtype, &lb, &extent );
  MPI_Type_get_true_extent( dtype, &lb, &true_extent );
  count_extent = (count-1) * extent + true_extent;
 
  MPIT_Type_blockct( count, dtype, 0, count_extent, &(blocks->number_blocks) );

  blocks->displ = malloc( blocks->number_blocks * sizeof(MPI_Aint) );
  blocks->blocklen = malloc( blocks->number_blocks * sizeof(int) );

  /* get offsets/blocklengths of the origin and the target datatypes */
  number_blocks = blocks->number_blocks;
  /* the resulting offset will be relative, this assumes that MPI_Aint can handle negative offsets */
  MPIT_Type_flatten( 0, count, dtype, 0, count_extent, &(blocks->displ[0]), &(blocks->blocklen[0]), &number_blocks );

  blocks->type = dtype;
  blocks->type_count = count;
  blocks->pos = 0;
  blocks->block_remaining_size = 0;

  return MPI_SUCCESS;
}

/* the names target_* and origin_* arbitrarily chosen, the algorithm doesn't rely on these "position" information */
static int dcache_get_next_block(MPI_Datatype origin_dtype, int origin_count, MPI_Aint* origin_offset,
  MPI_Datatype target_dtype, int target_count, MPI_Aint* target_offset, uint64_t* size, int* last_block) {

  int i;
  int origin_entry;
  int target_entry;

 /* search for the first datatype in the cache */
  i = dcache_origin_last_entry;
  origin_entry = -1;  
  do {
    if ( (dcache_blocks[i].type == origin_dtype) && (dcache_blocks[i].type_count == origin_count) ) {
      origin_entry = i;
      dcache_origin_last_entry = i;
    }
    i = (i+1) % 10;
  } while ( (i != dcache_origin_last_entry) && (origin_entry == -1) );
  
  if (origin_entry == -1) {
    origin_entry = dcache_last_entry;
    dcache_last_entry = (dcache_last_entry+1) % 10;
    /* clean up if that cache element was already used */
    if ( dcache_blocks[origin_entry].type != MPI_DATATYPE_NULL ) {
      free( dcache_blocks[origin_entry].displ );
      free( dcache_blocks[origin_entry].blocklen );
    }
    dcache_init_blocks( origin_dtype, origin_count, &dcache_blocks[origin_entry] );
  }

  /* search for the second datatype in the cache */
  i = dcache_target_last_entry;
  target_entry = -1;  
  do {
    if ( (dcache_blocks[i].type == target_dtype) && (dcache_blocks[i].type_count == target_count) ) {
      target_entry = i;
      dcache_target_last_entry = i;
    }
    i = (i+1) % 10;
  } while ( (i != dcache_target_last_entry) && (target_entry == -1) );
  
  if (target_entry == -1) {
    if (origin_entry == dcache_last_entry) {
      target_entry = (dcache_last_entry+1) % 10;
      dcache_last_entry = (dcache_last_entry+2) % 10;
    } else {
      target_entry = dcache_last_entry;
      dcache_last_entry = (dcache_last_entry+1) % 10;
    }
    /* clean up if that cache element was already used */
    if ( dcache_blocks[target_entry].type != MPI_DATATYPE_NULL ) {
      free( dcache_blocks[target_entry].displ );
      free( dcache_blocks[target_entry].blocklen );
    }
    dcache_init_blocks( target_dtype, target_count, &dcache_blocks[target_entry] );
  }

  /* find the next block */
  /* begin a new block of the origin datatype */
  if (dcache_blocks[origin_entry].block_remaining_size == 0) {
    i = dcache_blocks[origin_entry].pos++;
    dcache_blocks[origin_entry].block_remaining_size = dcache_blocks[origin_entry].blocklen[i];
    dcache_blocks[origin_entry].block_offset = dcache_blocks[origin_entry].displ[i];
  }
  /* begin a new block of the target datatype */
  if (dcache_blocks[target_entry].block_remaining_size == 0) {
    i = dcache_blocks[target_entry].pos++;
    dcache_blocks[target_entry].block_remaining_size = dcache_blocks[target_entry].blocklen[i];
    dcache_blocks[target_entry].block_offset = dcache_blocks[target_entry].displ[i];
  }
 
  if ( dcache_blocks[origin_entry].block_remaining_size < dcache_blocks[target_entry].block_remaining_size ) {
    *size = dcache_blocks[origin_entry].block_remaining_size;
  } else {
    *size = dcache_blocks[target_entry].block_remaining_size;
  }

  *origin_offset = dcache_blocks[origin_entry].block_offset;
  *target_offset = dcache_blocks[target_entry].block_offset;

  /* set the remainder 
   * it is done with arithmetic operations instead of if statements
   * one of remainder will always be zero */
  /* the if statement handles the case when the two datatypes are the same */
  if ( origin_entry != target_entry ) {
    dcache_blocks[origin_entry].block_remaining_size -= *size;
    dcache_blocks[target_entry].block_remaining_size -= *size;
    dcache_blocks[origin_entry].block_offset += *size;
    dcache_blocks[target_entry].block_offset += *size;
  } else {
    dcache_blocks[origin_entry].block_remaining_size = 0;
  }

  if ( (dcache_blocks[origin_entry].pos == dcache_blocks[origin_entry].number_blocks)
    && (dcache_blocks[target_entry].pos == dcache_blocks[target_entry].number_blocks) ) {
    *last_block = 1;
    dcache_blocks[origin_entry].pos = 0;
    dcache_blocks[target_entry].pos = 0;
    dcache_blocks[origin_entry].block_remaining_size = 0;
    dcache_blocks[target_entry].block_remaining_size = 0;
  } else {
    *last_block = 0;
  }

  return MPI_SUCCESS;
}

/* TODO: handle of MPI_COMBINER_F90_REAL, MPI_COMBINER_F90_COMPLEX, MPI_COMBINER_F90_INTEGER */
static int check_basic_type( MPI_Datatype type, int* flag, MPI_Datatype* basic_type ) {

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

  datatype_list = malloc( sizeof(dtype_list_t) );
  datatype_list->next = NULL;
  datatype_list->type = type;

  array_datatypes = malloc( sizeof(MPI_Datatype) );
  array_int = malloc( sizeof(MPI_Datatype) );
  array_addresses = malloc( sizeof(MPI_Aint) );

  while (datatype_list != NULL) {
    MPI_Type_get_envelope( datatype_list->type, &num_integers, &num_addresses, &num_datatypes, &combiner );
    if ( combiner == MPI_COMBINER_NAMED ) {
      if (init == 0) {
        init = 1;
        *basic_type = datatype_list->type;
      } else {
        if ( *basic_type != datatype_list->type ) {
          /* not the same basic datatypes */
          *flag = 0;
          /* clean up */
          while (datatype_list != NULL) {
            current = datatype_list;
            datatype_list = datatype_list->next;
            free( current );
          }
          break;
        } else {
          current = datatype_list;
          datatype_list = datatype_list->next;
          free( current );
        }
      }
    } else {
      assert( (combiner != MPI_COMBINER_F90_REAL) || (combiner != MPI_COMBINER_F90_COMPLEX) || (combiner != MPI_COMBINER_F90_INTEGER) );
      if ( num_integers > max_alloc ) {
        array_int = realloc( array_int, num_integers * sizeof(int) );
        array_addresses = realloc( array_addresses, num_integers * sizeof(MPI_Aint) );
        array_datatypes = realloc( array_datatypes, num_integers * sizeof(MPI_Datatype) );
        max_alloc = num_integers;
      }
      MPI_Type_get_contents( datatype_list->type, num_integers, num_addresses, num_datatypes, &array_int[0], &array_addresses[0], &array_datatypes[0] );
      /* free the datatype and remove it from the queue */
      if ( datatype_list->type != type ) {
        MPI_Type_free( &(datatype_list->type) );
      }

      if ( num_datatypes == 1 ) {
        datatype_list->type = array_datatypes[0];
      } else {
        /* only with a struct there is more than one sub datatypes */
        current = datatype_list;
        datatype_list = datatype_list->next;
        free( current );

        for( i=0 ; i<num_datatypes ; i++ ) {
          current = malloc( sizeof(dtype_list_t) );
          current->next = datatype_list;
          current->type = array_datatypes[i];
          datatype_list = current;
        }
      }
    }
  }

  free( array_datatypes );
  free( array_int );
  free( array_addresses );

  return MPI_SUCCESS;
}

static void accumulate_bor_8byte( dmapp_pe_t target_pe, MPI_Aint target_offset, MPI_Aint origin_offset, dmapp_seg_desc_t* seg_ptr, MPI_Aint size, foMPI_Win win ) {

  dmapp_return_t status;

  int nelements;
  int i;

  char* origin_addr = (char*) origin_offset;
  char* target_addr = (char*) target_offset;

  nelements = size / 8;

  for( i=0 ; i<nelements ; i++ ) {
    status = dmapp_aor_qw_nbi( target_addr+i*8, seg_ptr, target_pe, *(int64_t*)(origin_addr+i*8) );
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
  }
  win->nbi_counter++; /* little hack, since we actually just need a flag to indicate that there are nbi operations in progress */
}

static void accumulate_band_8byte( dmapp_pe_t target_pe, MPI_Aint target_offset, MPI_Aint origin_offset, dmapp_seg_desc_t* seg_ptr, MPI_Aint size, foMPI_Win win ) {

  dmapp_return_t status;

  int nelements;
  int i;

  char* origin_addr = (char*) origin_offset;
  char* target_addr = (char*) target_offset;

  nelements = size / 8;

  for( i=0 ; i<nelements ; i++ ) {
    status = dmapp_aand_qw_nbi( target_addr+i*8, seg_ptr, target_pe, *(int64_t*)(origin_addr+i*8) );
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
  }
  win->nbi_counter++; /* little hack, since we actually just need a flag to indicate that there are nbi operations in progress */
}


static void accumulate_bxor_8byte( dmapp_pe_t target_pe, MPI_Aint target_offset, MPI_Aint origin_offset, dmapp_seg_desc_t* seg_ptr, MPI_Aint size, foMPI_Win win ) {

  dmapp_return_t status;

  int nelements;
  int i;

  char* origin_addr = (char*) origin_offset;
  char* target_addr = (char*) target_offset;

  nelements = size / 8;

  for( i=0 ; i<nelements ; i++ ) {
    status = dmapp_axor_qw_nbi( target_addr+i*8, seg_ptr, target_pe, *(int64_t*)(origin_addr+i*8) );
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
  }
  win->nbi_counter++; /* little hack, since we actually just need a flag to indicate that there are nbi operations in progress */
}

static void accumulate_sum_8byte( dmapp_pe_t target_pe, MPI_Aint target_offset, MPI_Aint origin_offset, dmapp_seg_desc_t* seg_ptr, MPI_Aint size, foMPI_Win win ) {

  dmapp_return_t status;

  int nelements;
  int i;

  char* origin_addr = (char*) origin_offset;
  char* target_addr = (char*) target_offset;

  nelements = size / 8;

  for( i=0 ; i<nelements ; i++ ) {
    status = dmapp_aadd_qw_nbi( target_addr+i*8, seg_ptr, target_pe, *(int64_t*)(origin_addr+i*8) );
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
  }
  win->nbi_counter++; /* little hack, since we actually just need a flag to indicate that there are nbi operations in progress */
}

static void simple_put( dmapp_pe_t target_pe, MPI_Aint target_offset, MPI_Aint origin_offset, dmapp_seg_desc_t* seg_ptr, MPI_Aint size, foMPI_Win win ) {

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
  timing_record( 1 );
#endif
  if (nelements != 0) {
    status = dmapp_put_nbi( target_addr, seg_ptr, target_pe, origin_addr, nelements, DMAPP_DQW);
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
    win->nbi_counter++;
  }
  if (nelements_modulo != 0 ) {
    status = dmapp_put_nbi( target_addr+offset_modulo, seg_ptr, target_pe, origin_addr+offset_modulo, nelements_modulo, DMAPP_BYTE);
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
    win->nbi_counter++;
  }
#ifdef PAPI
  timing_record( 2 );
#endif
}

#ifdef XPMEM
static void xpmem_simple_put( dmapp_pe_t target_pe, MPI_Aint target_offset, MPI_Aint origin_offset, dmapp_seg_desc_t* seg_ptr, MPI_Aint size, foMPI_Win win ) {

#ifdef PAPI
  timing_record( 1 );
#endif

  if (size > 524288) {
    memcpy( (char*) target_offset, (char*) origin_offset, size );
  } else {
    sse_memcpy( (char*) target_offset, (char*) origin_offset, size );
  }
 
#ifdef PAPI
  timing_record( 2 );
#endif
}
#endif

static void simple_get( dmapp_pe_t target_pe, MPI_Aint target_offset, MPI_Aint origin_offset, dmapp_seg_desc_t* seg_ptr, MPI_Aint size, foMPI_Win win ) {

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
    status = dmapp_get_nbi( origin_addr, target_addr, seg_ptr, target_pe, nelements, DMAPP_DQW);
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
    win->nbi_counter++;
  }
  if (nelements_modulo != 0 ) {
    status = dmapp_get_nbi( origin_addr+offset_modulo, target_addr+offset_modulo, seg_ptr, target_pe, nelements_modulo, DMAPP_BYTE);
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
    win->nbi_counter++;
  }
#ifdef PAPI
  timing_record( 6 );
#endif
}

#ifdef XPMEM
static void xpmem_simple_get( dmapp_pe_t target_pe, MPI_Aint target_offset, MPI_Aint origin_offset, dmapp_seg_desc_t* seg_ptr, MPI_Aint size, foMPI_Win win ) {
#ifdef PAPI
  timing_record( 5 );
#endif

  if (size > 524288) {
    memcpy( (char*) origin_offset, (char*) target_offset, size );
  } else {
    sse_memcpy( (char*) origin_offset, (char*) target_offset, size );
  }

#ifdef PAPI
  timing_record( 6 );
#endif
 
}
#endif

static int communication_and_datatype_handling(const void* origin_addr, int origin_count, MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp,
  int target_count, MPI_Datatype target_datatype, foMPI_Win win, dmapp_pe_t target_pe, 
  void (*commfunc) (dmapp_pe_t, MPI_Aint, MPI_Aint, dmapp_seg_desc_t*, MPI_Aint, foMPI_Win) ) {

  dmapp_return_t status;
  dmapp_seg_desc_t* seg_ptr;
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
  if( win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC ) {

    /* checks if the local information about the remote dynamic window are up-to-date: if not, they are fetched */
    get_dynamic_window_regions( &status, target_rank, target_pe, win );

    target_offset = (MPI_Aint) MPI_BOTTOM + target_disp;
  } else {
#ifdef XPMEM
    int local_rank = foMPI_onnode_rank_global_to_local( target_rank, win );
    if ( local_rank != -1 ) {
      target_offset = (MPI_Aint) win->xpmem_array[local_rank].base + target_disp * win->win_array[target_rank].disp_unit;
    } else {
      target_offset = (MPI_Aint) win->win_array[target_rank].base + target_disp * win->win_array[target_rank].disp_unit;
    }
#else
    target_offset = (MPI_Aint) win->win_array[target_rank].base + target_disp * win->win_array[target_rank].disp_unit;
#endif
    /* TODO: other intrinsic datatypes */
    if ( (origin_datatype == target_datatype) && ( (origin_datatype == MPI_CHAR) || (origin_datatype == MPI_UINT64_T) || (origin_datatype == MPI_DOUBLE) || (origin_datatype == MPI_INT) ) ) {
      /* fast path for intrinsic datatypes */

      MPI_Type_size( origin_datatype, &origin_type_size );

      seg_ptr = &(win->win_array[target_rank].seg);

      (*commfunc) ( target_pe, target_offset, origin_offset, seg_ptr, origin_count * origin_type_size, win );

      return MPI_SUCCESS;
    }
  }

  /* process the datatypes */
  MPI_Type_size( origin_datatype, &origin_type_size );
  MPI_Type_size( target_datatype, &target_type_size );

  /* TODO: should this return a error? */
  assert( (origin_type_size > 0) && (target_type_size > 0) );
  assert( (origin_type_size * origin_count) == (target_type_size * target_count) );

  /* TODO: real test that the datatypes are identical */
  MPI_Type_get_extent( target_datatype, &lb, &target_extent );

  MPI_Type_get_true_extent( target_datatype, &lb, &target_true_extent );

  target_count_extent = (target_count-1) * target_extent + target_true_extent;

  if( win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC ) {
    /* already checks if the requested memory region is within the boundaries */

    /* check if datatype work in one memory region */
    current = win->dynamic_list[target_rank].remote_mem_regions;
    seg_ptr = NULL;
    if( current != NULL ) {
      if ( ( target_offset >= current->base ) && ( (target_offset+target_count_extent) <= (current->base+current->size) ) ) {
        seg_ptr = &(current->seg);
      }
      while (current->next != NULL) {
        current = current->next;
        if ( ( target_offset >= current->base ) && ( (target_offset+target_count_extent) <= (current->base+current->size) ) ) {
          seg_ptr = &(current->seg);
        }
      }
    }
  } else {
    seg_ptr = &(win->win_array[target_rank].seg);
  }

  if ( seg_ptr != NULL ) {
    /* only one memory region */

    do {
      dcache_get_next_block( origin_datatype, origin_count, &origin_block_offset, target_datatype, target_count, &target_block_offset, &transport_size, &last_block);

      (*commfunc) ( target_pe, target_offset+target_block_offset, origin_offset+origin_block_offset, seg_ptr, transport_size, win );
    } while ( last_block == 0 );

  } else {
    /* more than one memory region */
 
    do {
      dcache_get_next_block( origin_datatype, origin_count, &origin_block_offset, target_datatype, target_count, &target_block_offset, &remaining_size, &last_block);

      /* TODO: maybe we can save the last state and check if it is still true */
      /* check local list for entry */
      while (remaining_size > 0 ) {

#ifdef NDEBUG
        find_seg_and_size( win->dynamic_list[target_rank].remote_mem_regions, remaining_size, target_offset+target_block_offset, &seg_ptr, &transport_size );
#else      
        res = find_seg_and_size( win->dynamic_list[target_rank].remote_mem_regions, remaining_size, target_offset+target_block_offset, &seg_ptr, &transport_size );
#endif
        assert( res == MPI_SUCCESS ); /* TODO: return a real error code? */

        (*commfunc) ( target_pe, target_offset+target_block_offset, origin_offset+origin_block_offset, seg_ptr, transport_size, win );

        target_block_offset += transport_size;
        origin_block_offset += transport_size;
        remaining_size -= transport_size;
      }
      
    } while ( last_block == 0 );
  }
  
  return MPI_SUCCESS;
}

static int communication_and_datatype_handling_dmapp_only(const void* origin_addr, int origin_count, MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp,
  int target_count, MPI_Datatype target_datatype, foMPI_Win win, dmapp_pe_t target_pe, 
  void (*commfunc) (dmapp_pe_t, MPI_Aint, MPI_Aint, dmapp_seg_desc_t*, MPI_Aint, foMPI_Win) ) {

  dmapp_return_t status;
  dmapp_seg_desc_t* seg_ptr;
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
  if( win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC ) {

    /* checks if the local information about the remote dynamic window are up-to-date: if not, they are fetched */
    get_dynamic_window_regions( &status, target_rank, target_pe, win );

    target_offset = (MPI_Aint) MPI_BOTTOM + target_disp;
  } else {
    target_offset = (MPI_Aint) win->win_array[target_rank].base + target_disp * win->win_array[target_rank].disp_unit;
    /* TODO: other intrinsic datatypes */
    if ( (origin_datatype == target_datatype) && ( (origin_datatype == MPI_CHAR) || (origin_datatype == MPI_UINT64_T) || (origin_datatype == MPI_DOUBLE) || (origin_datatype == MPI_INT) ) ) {
      /* fast path for intrinsic datatypes */

      seg_ptr = &(win->win_array[target_rank].seg);
      
      MPI_Type_size( origin_datatype, &origin_type_size );
      (*commfunc) ( target_pe, target_offset, origin_offset, seg_ptr, origin_count * origin_type_size, win );
      
      return MPI_SUCCESS;
    }
  }

  /* process the datatypes */
  MPI_Type_size( origin_datatype, &origin_type_size );
  MPI_Type_size( target_datatype, &target_type_size );

  /* TODO: should this return a error? */
  assert( (origin_type_size > 0) && (target_type_size > 0) );
  assert( (origin_type_size * origin_count) == (target_type_size * target_count) );

  /* TODO: real test that the datatypes are identical */
  MPI_Type_get_extent( target_datatype, &lb, &target_extent );

  MPI_Type_get_true_extent( target_datatype, &lb, &target_true_extent );

  target_count_extent = (target_count-1) * target_extent + target_true_extent;

  if( win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC ) {
    /* already checks if the requested memory region is within the boundaries */

    /* check if datatype work in one memory region */
    current = win->dynamic_list[target_rank].remote_mem_regions;
    seg_ptr = NULL;
    if( current != NULL ) {
      if ( ( target_offset >= current->base ) && ( (target_offset+target_count_extent) <= (current->base+current->size) ) ) {
        seg_ptr = &(current->seg);
      }
      while (current->next != NULL) {
        current = current->next;
        if ( ( target_offset >= current->base ) && ( (target_offset+target_count_extent) <= (current->base+current->size) ) ) {
          seg_ptr = &(current->seg);
        }
      }
    }
  } else {
    seg_ptr = &(win->win_array[target_rank].seg);
  }

  if ( seg_ptr != NULL ) {
    /* only one memory region */

    do {
      dcache_get_next_block( origin_datatype, origin_count, &origin_block_offset, target_datatype, target_count, &target_block_offset, &transport_size, &last_block);

      (*commfunc) ( target_pe, target_offset+target_block_offset, origin_offset+origin_block_offset, seg_ptr, transport_size, win );
    } while ( last_block == 0 );

  } else {
    /* more than one memory region */
 
    do {
      dcache_get_next_block( origin_datatype, origin_count, &origin_block_offset, target_datatype, target_count, &target_block_offset, &remaining_size, &last_block);

      /* TODO: maybe we can save the last state and check if it is still true */
      /* check local list for entry */
      while (remaining_size > 0 ) {

#ifdef NDEBUG
        find_seg_and_size( win->dynamic_list[target_rank].remote_mem_regions, remaining_size, target_offset+target_block_offset, &seg_ptr, &transport_size );
#else      
        res = find_seg_and_size( win->dynamic_list[target_rank].remote_mem_regions, remaining_size, target_offset+target_block_offset, &seg_ptr, &transport_size );
#endif
        assert( res == MPI_SUCCESS ); /* TODO: return a real error code? */

        (*commfunc) ( target_pe, target_offset+target_block_offset, origin_offset+origin_block_offset, seg_ptr, transport_size, win );

        target_block_offset += transport_size;
        origin_block_offset += transport_size;
        remaining_size -= transport_size;
      }
      
    } while ( last_block == 0 );
  }
  
  return MPI_SUCCESS;
}

/* real MPI functions */
int foMPI_Put(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype, foMPI_Win win) {

#ifdef PAPI
  timing_record( -1 );
#endif

  int res;
  
  if ( (target_rank == MPI_PROC_NULL) || (origin_count == 0) || (target_count == 0) ) {
    return MPI_SUCCESS;
  }

  assert((target_rank >= 0) && (target_rank < win->commsize));
  dmapp_pe_t target_pe = (dmapp_pe_t) win->group_ranks[target_rank];

#ifdef XPMEM
  if ( (win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC) && (foMPI_onnode_rank_global_to_local( target_rank, win ) != -1) ) {
    res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &xpmem_simple_put);    
  } else {
    res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &simple_put);
  }
#else
  res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &simple_put);
#endif

#ifdef PAPI
  timing_record( 3 );
#endif

  return res;
}

int foMPI_Get(void *origin_addr, int origin_count, MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype, foMPI_Win win) {

#ifdef PAPI
  timing_record( -1 );
#endif

  int res;
  
  if ( (target_rank == MPI_PROC_NULL) || (origin_count == 0) || (target_count == 0) ) {
    return MPI_SUCCESS;
  }

  assert((target_rank >= 0) && (target_rank < win->commsize));
  dmapp_pe_t target_pe = (dmapp_pe_t) win->group_ranks[target_rank];

#ifdef XPMEM
  if ( (win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC) && ( foMPI_onnode_rank_global_to_local( target_rank, win ) != -1 ) ) {
    res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &xpmem_simple_get);    
  } else {
    res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &simple_get);
  }
#else
  res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &simple_get);
#endif

#ifdef PAPI
  timing_record( 7 );
#endif

  return res;
}

int foMPI_Rput(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp, int target_count,
  MPI_Datatype target_datatype, foMPI_Win win, foMPI_Request *request) {

  int res;

  if ( (target_rank == MPI_PROC_NULL) || (origin_count == 0) || (target_count == 0) ) {
    return MPI_SUCCESS;
  }

  assert((target_rank >= 0) && (target_rank < win->commsize));
  dmapp_pe_t target_pe = (dmapp_pe_t) win->group_ranks[target_rank];

  *request = malloc(sizeof(foMPI_Win_request_t));
  (*request)->win = win;
  (*request)->elements = target_count;
  (*request)->datatype = target_datatype;
  (*request)->source = target_rank;

#ifdef XPMEM
  if ( (win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC) && ( foMPI_onnode_rank_global_to_local( target_rank, win ) != -1 ) ) {
    res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win,
      target_pe, &xpmem_simple_put);    
  } else {
    res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win,
      target_pe, &simple_put);
  }
#else
  res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win,
    target_pe, &simple_put);
#endif

  return res; 
}

int foMPI_Rget(void *origin_addr, int origin_count, MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp, int target_count,
  MPI_Datatype target_datatype, foMPI_Win win, foMPI_Request *request) {

  int res;
  
  if ( (target_rank == MPI_PROC_NULL) || (origin_count == 0) || (target_count == 0) ) {
    return MPI_SUCCESS;
  }

  assert((target_rank >= 0) && (target_rank < win->commsize));
  dmapp_pe_t target_pe = (dmapp_pe_t) win->group_ranks[target_rank];

  *request = malloc(sizeof(foMPI_Win_request_t));
  (*request)->win = win;
  (*request)->elements = target_count;
  (*request)->datatype = target_datatype;
  (*request)->source = target_rank;

#ifdef XPMEM
  if ( (win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC) && ( foMPI_onnode_rank_global_to_local( target_rank, win ) != -1 )  ) {
    res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win,
      target_pe, &xpmem_simple_get);    
  } else {
    res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win,
      target_pe, &simple_get);
  }
#else
  res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win,
    target_pe, &simple_get);
#endif

  return res;
}

int foMPI_Accumulate(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp, int target_count,
  MPI_Datatype target_datatype, foMPI_Op op, foMPI_Win win) {

#ifdef PAPI
  timing_record( -1 );
#endif

  void* buffer;
  dmapp_return_t status;
  foMPI_Win_desc_t* win_ptr = NULL;
  dmapp_seg_desc_t* win_ptr_seg = NULL;
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

  if ( (target_rank == MPI_PROC_NULL) || (origin_count == 0) || (target_count == 0) ) {
    return MPI_SUCCESS;
  }

  assert( (target_rank >= 0) && (target_rank < win->commsize) );
  dmapp_pe_t target_pe = (dmapp_pe_t) win->group_ranks[target_rank];
 
  assert( (op >= foMPI_SUM) && (op <= foMPI_REPLACE) );

  if ( (origin_datatype == target_datatype) && (target_datatype == MPI_INT64_T) && (op == foMPI_SUM) ) {
 #ifdef NDEBUG
    communication_and_datatype_handling_dmapp_only( origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &accumulate_sum_8byte );
#else
    res = communication_and_datatype_handling_dmapp_only( origin_addr, origin_count, origin_datatype, target_rank, target_disp,
            target_count, target_datatype, win, target_pe, &accumulate_sum_8byte);
#endif
    return MPI_SUCCESS;
  }
  
  if ( (origin_datatype == target_datatype) && ( (origin_datatype == MPI_DOUBLE) || (target_datatype == MPI_INT64_T) ) && ( (op == foMPI_BOR) || (op == foMPI_BAND) || (op == foMPI_BXOR) ) ) { /* fast path */

    switch (op) {
      case foMPI_BAND:
#ifdef NDEBUG
        communication_and_datatype_handling( origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &accumulate_band_8byte );
#else
        res = communication_and_datatype_handling( origin_addr, origin_count, origin_datatype, target_rank, target_disp,
                target_count, target_datatype, win, target_pe, &accumulate_band_8byte);
#endif
        break;
      case foMPI_BOR:
#ifdef NDEBUG
        communication_and_datatype_handling( origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &accumulate_bor_8byte);
#else
        res = communication_and_datatype_handling( origin_addr, origin_count, origin_datatype, target_rank, target_disp,
                target_count, target_datatype, win, target_pe, &accumulate_bor_8byte);
#endif
        break;
      case foMPI_BXOR:
#ifdef NDEBUG
        communication_and_datatype_handling( origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &accumulate_bxor_8byte);
#else
        res = communication_and_datatype_handling( origin_addr, origin_count, origin_datatype, target_rank, target_disp,
                target_count, target_datatype, win, target_pe, &accumulate_bxor_8byte);
#endif
        break;
    }
    assert( res == MPI_SUCCESS );

  } else {

    if (op == foMPI_REPLACE) {
#ifdef XPMEM
    if ( (win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC) && ( foMPI_onnode_rank_global_to_local( target_rank, win ) != -1 ) ) {
#ifdef NDEBUG
      communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win,
        target_pe, &xpmem_simple_put);
#else
      res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win,
        target_pe, &xpmem_simple_put);
#endif
    } else {
#ifdef NDEBUG
      communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win,
        target_pe, &simple_put);
#else
      res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win,
        target_pe, &simple_put);
#endif
    }
#else
#ifdef NDEBUG
      communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win,
        target_pe, &simple_put);
#else
      res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win,
        target_pe, &simple_put);
#endif
#endif
      assert( res == MPI_SUCCESS );

    } else {
      /* determine the target_addr and some other create-flavor dependent parameter */
      if( win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC ) {
        win_ptr = win->dynamic_list[target_rank].win_ptr;
        win_ptr_seg = &(win->dynamic_list[target_rank].win_ptr_seg);
      } else {
        win_ptr = win->win_array[target_rank].win_ptr;
        win_ptr_seg = &(win->win_array[target_rank].win_ptr_seg);
      }
    
      check_basic_type( origin_datatype, &flag, &origin_basic_type );
      if (flag == 0) {
        return foMPI_DATATYPE_NOT_SUPPORTED;
      }

      check_basic_type( target_datatype, &flag, &target_basic_type );
      if (flag == 0) {
        return foMPI_DATATYPE_NOT_SUPPORTED;
      }

      if (origin_basic_type != target_basic_type) {
        return foMPI_ERROR_RMA_DATATYPE_MISMATCH;
      }
 
      /* process the datatypes */
      MPI_Type_size( origin_datatype, &origin_type_size );
      MPI_Type_size( origin_basic_type, &origin_basic_type_size);

      /* set the mutex */
      foMPI_Set_window_mutex( &status, target_pe, win_ptr, win_ptr_seg, win );

      buffer = malloc( origin_count * origin_type_size );

      /* get the elements */
      elements_in_buffer = origin_count * origin_type_size / origin_basic_type_size;

#ifdef XPMEM
      if ( (win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC) && ( foMPI_onnode_rank_global_to_local( target_rank, win ) != -1 ) ) {
#ifdef NDEBUG
        communication_and_datatype_handling(buffer, elements_in_buffer, origin_basic_type, target_rank, target_disp, target_count, target_datatype, win, target_pe, &xpmem_simple_get );
#else
        res = communication_and_datatype_handling(buffer, elements_in_buffer, origin_basic_type, target_rank, target_disp,
                target_count, target_datatype, win, target_pe, &xpmem_simple_get );
#endif
      } else {
#ifdef NDEBUG
       communication_and_datatype_handling(buffer, elements_in_buffer, origin_basic_type, target_rank, target_disp, target_count, target_datatype, win, target_pe, &simple_get );
#else
        res = communication_and_datatype_handling(buffer, elements_in_buffer, origin_basic_type, target_rank, target_disp,
                target_count, target_datatype, win, target_pe, &simple_get );
#endif
      }
#else

#ifdef NDEBUG
      communication_and_datatype_handling(buffer, elements_in_buffer, origin_basic_type, target_rank, target_disp, target_count, target_datatype, win, target_pe, &simple_get );
#else
      res = communication_and_datatype_handling(buffer, elements_in_buffer, origin_basic_type, target_rank, target_disp,
             target_count, target_datatype, win, target_pe, &simple_get );
#endif
#endif

      assert( res == MPI_SUCCESS );
      status = dmapp_gsync_wait();
      _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);

      /* operate on the elements */
      do {
        dcache_get_next_block( origin_basic_type, elements_in_buffer, &buffer_block_offset, origin_datatype, origin_count, &origin_block_offset, &transport_size, &last_block);
#ifdef NDEBUG
        foMPI_RMA_op( (char*) buffer+buffer_block_offset, (char*) origin_addr+origin_block_offset, (char*) buffer+buffer_block_offset, op, origin_basic_type, transport_size/origin_basic_type_size );
#else
        res = foMPI_RMA_op( (char*) buffer+buffer_block_offset, (char*) origin_addr+origin_block_offset, (char*) buffer+buffer_block_offset, 
              op, origin_basic_type, transport_size/origin_basic_type_size );
#endif
        assert( res == MPI_SUCCESS );

      } while (last_block == 0);
    
      /* put the elements back */
      /* since we must free the buffer, there is a global wait after the put operations */  
#ifdef XPMEM
      if ( (win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC) && ( foMPI_onnode_rank_global_to_local( target_rank, win ) != -1 ) ) {
#ifdef NDEBUG
        communication_and_datatype_handling( buffer, elements_in_buffer, origin_basic_type, target_rank, target_disp, target_count, target_datatype, win, target_pe, &xpmem_simple_put );
#else
        res = communication_and_datatype_handling( buffer, elements_in_buffer, origin_basic_type, target_rank, target_disp,
                target_count, target_datatype, win, target_pe, &xpmem_simple_put );
#endif
      } else {
#ifdef NDEBUG
        communication_and_datatype_handling( buffer, elements_in_buffer, origin_basic_type, target_rank, target_disp, target_count, target_datatype, win, target_pe, &simple_put );
#else
        res = communication_and_datatype_handling( buffer, elements_in_buffer, origin_basic_type, target_rank, target_disp,
                target_count, target_datatype, win, target_pe, &simple_put );
#endif
      }
#else
#ifdef NDEBUG
      communication_and_datatype_handling( buffer, elements_in_buffer, origin_basic_type, target_rank, target_disp, target_count, target_datatype, win, target_pe, &simple_put );
#else
      res = communication_and_datatype_handling( buffer, elements_in_buffer, origin_basic_type, target_rank, target_disp,
              target_count, target_datatype, win, target_pe, &simple_put );
#endif
#endif
      assert( res == MPI_SUCCESS );
      status = dmapp_gsync_wait();
      _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
      win->nbi_counter = 0;

      /* release the mutex if necessary */
      foMPI_Release_window_mutex( &status, target_pe, win_ptr, win_ptr_seg, win );

      free(buffer);
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

int foMPI_Get_accumulate(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype, void *result_addr, int result_count, MPI_Datatype result_datatype,
  int target_rank, MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype, foMPI_Op op, foMPI_Win win) {

#ifdef PAPI
  timing_record( -1 );
#endif

  void* buffer = NULL; /* to get rid of the maybe-uninitialized warnings */
  dmapp_return_t status;
  foMPI_Win_desc_t* win_ptr = NULL; /* to get rid of the maybe-uninitialized warnings */
  dmapp_seg_desc_t* win_ptr_seg = NULL; /* to get rid of the maybe-uninitialized warnings */
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
 
  if ( (target_rank == MPI_PROC_NULL) || (target_count == 0) || (result_count == 0) ) {
    return MPI_SUCCESS;
  }

  assert( (target_rank >= 0) && (target_rank < win->commsize) );
  dmapp_pe_t target_pe = (dmapp_pe_t) win->group_ranks[target_rank];

  assert( (op >= foMPI_SUM) && (op <= foMPI_NO_OP) );

  if( win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC ) {
    win_ptr = win->dynamic_list[target_rank].win_ptr;
    win_ptr_seg = &(win->dynamic_list[target_rank].win_ptr_seg);
  } else {
    win_ptr = win->win_array[target_rank].win_ptr;
    win_ptr_seg = &(win->win_array[target_rank].win_ptr_seg);
  }

  /* fast path for INT64_T */
  if( ( origin_datatype == MPI_INT64_T) && (origin_datatype == target_datatype) && (origin_datatype == result_datatype) && ( win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC ) && (op == foMPI_SUM) ) {

    MPI_Aint i, nelements;
    int64_t* acc_target_addr = (int64_t*) ((char*) win->win_array[target_rank].base + target_disp * win->win_array[target_rank].disp_unit);
    dmapp_seg_desc_t* seg_ptr = &(win->win_array[target_rank].seg);

    nelements = target_count * win->win_array[target_rank].disp_unit / 8;

    for( i=0 ; i<nelements ; i++ ) {
      status = dmapp_afadd_qw_nbi( (int64_t*)result_addr+i, acc_target_addr+i, seg_ptr, target_pe, *((int64_t*)origin_addr+i) );
      _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
    }
    win->nbi_counter++; /* little hack, since we actually just need a flag to indicate that there are nbi operations in progress */
    return MPI_SUCCESS;
  }


  /* set the mutex if necessary */
  foMPI_Set_window_mutex( &status, target_pe, win_ptr, win_ptr_seg, win );

  /* get the elements with blocks of the combination of the result and target datatypes */
#ifdef XPMEM
  if ( (win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC) && ( foMPI_onnode_rank_global_to_local( target_rank, win ) != -1 ) ) {
#ifdef NDEBUG
    communication_and_datatype_handling(result_addr, result_count, result_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &xpmem_simple_get);
#else
    res = communication_and_datatype_handling(result_addr, result_count, result_datatype, target_rank, target_disp, target_count, target_datatype, win,
            target_pe, &xpmem_simple_get);
#endif
  } else {
#ifdef NDEBUG
    communication_and_datatype_handling(result_addr, result_count, result_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &simple_get );
#else
    res = communication_and_datatype_handling(result_addr, result_count, result_datatype, target_rank, target_disp, target_count, target_datatype, win,
            target_pe, &simple_get );
#endif
  }
#else
#ifdef NDEBUG
  communication_and_datatype_handling(result_addr, result_count, result_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &simple_get );
#else
  res = communication_and_datatype_handling(result_addr, result_count, result_datatype, target_rank, target_disp, target_count, target_datatype, win,
          target_pe, &simple_get );
#endif
#endif
  assert( res == MPI_SUCCESS );
  status = dmapp_gsync_wait();
  _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);

  /* operate on the elements */ 
  if ( (op != foMPI_NO_OP) && (op != foMPI_REPLACE) ) {

    /* process the datatypes */
    check_basic_type( origin_datatype, &flag, &origin_basic_type );
    assert( flag != 0 );

    MPI_Type_size( origin_datatype, &origin_type_size );

    MPI_Type_size( origin_basic_type, &origin_basic_type_size );

    buffer = malloc( origin_count * origin_type_size );

    buffer_ptr = (char*) buffer;
  
    do {
      dcache_get_next_block( result_datatype, result_count, &result_block_offset, origin_datatype, origin_count, &origin_block_offset, &transport_size, &last_block);

#ifdef NDEBUG
      foMPI_RMA_op( buffer_ptr, (char*) origin_addr+origin_block_offset, (char*) result_addr+result_block_offset, op, origin_basic_type, transport_size/origin_basic_type_size );
#else
      res = foMPI_RMA_op( buffer_ptr, (char*) origin_addr+origin_block_offset, (char*) result_addr+result_block_offset, 
              op, origin_basic_type, transport_size/origin_basic_type_size );
#endif
      assert( res == MPI_SUCCESS );

      buffer_ptr += transport_size;
    } while (last_block == 0);

    /* since we must free the buffer, there is a global wait after the put operations */  
#ifdef XPMEM
    if ( (win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC) && ( foMPI_onnode_rank_global_to_local( target_rank, win ) != -1 ) ) {
#ifdef NDEBUG
      communication_and_datatype_handling( buffer, origin_type_size*origin_count/origin_basic_type_size, origin_basic_type, target_rank, target_disp,
        target_count, target_datatype, win, target_pe, &xpmem_simple_put );
#else
      res = communication_and_datatype_handling( buffer, origin_type_size*origin_count/origin_basic_type_size, origin_basic_type, target_rank, target_disp,
              target_count, target_datatype, win, target_pe, &xpmem_simple_put );
#endif
    } else {
#ifdef NDEBUG
      communication_and_datatype_handling( buffer, origin_type_size*origin_count/origin_basic_type_size, origin_basic_type, target_rank, target_disp,
        target_count, target_datatype, win, target_pe, &simple_put );
#else
      res = communication_and_datatype_handling( buffer, origin_type_size*origin_count/origin_basic_type_size, origin_basic_type, target_rank, target_disp,
              target_count, target_datatype, win, target_pe, &simple_put );
#endif
    }
#else
#ifdef NDEBUG
    communication_and_datatype_handling( buffer, origin_type_size*origin_count/origin_basic_type_size, origin_basic_type, target_rank, target_disp,
      target_count, target_datatype, win, target_pe, &simple_put );
#else
    res = communication_and_datatype_handling( buffer, origin_type_size*origin_count/origin_basic_type_size, origin_basic_type, target_rank, target_disp,
       target_count, target_datatype, win, target_pe, &simple_put );
#endif
#endif
    assert( res == MPI_SUCCESS );
    status = dmapp_gsync_wait();
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
    win->nbi_counter = 0;

    free(buffer);
  } else {
    if ( op == foMPI_REPLACE ) {
      /* put the elements with blocks of the combination of the result and target datatypes */
#ifdef XPMEM
      if ( (win->create_flavor != foMPI_WIN_FLAVOR_DYNAMIC) && ( foMPI_onnode_rank_global_to_local( target_rank, win ) != -1 ) ) {
#ifdef NDEBUG
        communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &xpmem_simple_put);
#else
        res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win,
                target_pe, &xpmem_simple_put);
#endif
      } else {
#ifdef NDEBUG
        communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &simple_put );
#else
        res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win,
                target_pe, &simple_put );
#endif
      }
#else
#ifdef NDEBUG
      communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win, target_pe, &simple_put );
#else
      res = communication_and_datatype_handling(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win,
              target_pe, &simple_put );
#endif
#endif
      assert( res == MPI_SUCCESS );
      status = dmapp_gsync_wait();
      _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
      win->nbi_counter = 0;
    }
  }
  
  /* release the mutex if necessary */
  foMPI_Release_window_mutex( &status, target_pe, win_ptr, win_ptr_seg, win );

#ifdef PAPI
  timing_record( 22 );  
#endif
      
  return MPI_SUCCESS;
}

int foMPI_Raccumulate(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp, int target_count,
  MPI_Datatype target_datatype, foMPI_Op op, foMPI_Win win, foMPI_Request *request) {

  int res = MPI_SUCCESS;
 
  if ( (origin_count > 0) && (target_count > 0) ) {
    res = foMPI_Accumulate(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, op, win );
  }

  *request = malloc(sizeof(foMPI_Win_request_t));
  (*request)->win = win;
  (*request)->elements = target_count;
  (*request)->datatype = target_datatype;
  (*request)->source = target_rank;
      
  return res;
}

int foMPI_Rget_accumulate(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype, void *result_addr, int result_count, MPI_Datatype result_datatype,
  int target_rank, MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype, foMPI_Op op, foMPI_Win win, foMPI_Request *request) {

  int res = MPI_SUCCESS;

  if ( (target_count > 0) && (result_count > 0) ) {
    res = foMPI_Get_accumulate( origin_addr, origin_count, origin_datatype, result_addr, result_count, result_datatype, target_rank, target_disp,
            target_count, target_datatype, op, win );
  }

  *request = malloc(sizeof(foMPI_Win_request_t));
  (*request)->win = win;
  (*request)->elements = target_count;
  (*request)->datatype = target_datatype;
  (*request)->source = target_rank;
      
  return res;
}

int foMPI_Fetch_and_op(const void *origin_addr, void *result_addr, MPI_Datatype datatype, int target_rank, MPI_Aint target_disp, foMPI_Op op, foMPI_Win win) {
#ifdef PAPI
  timing_record( -1 );
#endif

  int found;
  void* buffer;
  void* target_addr;
  int type_size;
  dmapp_return_t status;
  foMPI_Win_desc_t* win_ptr = NULL; /* to get rid of the maybe-uninitialized warnings */
  dmapp_seg_desc_t* win_ptr_seg = NULL; /* to get rid of the maybe-uninitialized warnings */
  dmapp_seg_desc_t* seg_ptr;
  foMPI_Win_dynamic_element_t* current;
  int res = MPI_SUCCESS;
 
  if ( target_rank == MPI_PROC_NULL ) {
    return MPI_SUCCESS;
  }

  assert( (target_rank >= 0) && (target_rank < win->commsize) );
  dmapp_pe_t target_pe = (dmapp_pe_t) win->group_ranks[target_rank];

  MPI_Type_size( datatype, &type_size );

  if( win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC ) {
    /* check already if the the requested memory region is within the boundaries */

    /* checks if the local information about the remote dynamic window are up-to-date: if not, they are fetched */
    get_dynamic_window_regions( &status, target_rank, target_pe, win );

    /* check local list for entry */
    current = win->dynamic_list[target_rank].remote_mem_regions;
    found = 0;
    if( current != NULL ) {
      if ( ( target_disp >= current->base ) && ( target_disp+type_size <= (current->base+current->size) ) ) {
        found = 1;
      }
      while( (current->next != NULL) && (found == 0) ) {
        current = current->next;
        if ( ( target_disp >= current->base ) && ( target_disp+type_size <= (current->base+current->size) ) ) {
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
    target_addr = (char*) win->win_array[target_rank].base + target_disp * win->win_array[target_rank].disp_unit;
    seg_ptr = &(win->win_array[target_rank].seg);
  
    win_ptr = (void*) win->win_array[target_rank].win_ptr;
    win_ptr_seg = &(win->win_array[target_rank].win_ptr_seg);
  }

  /* set the mutex */
  foMPI_Set_window_mutex( &status, target_pe, win_ptr, win_ptr_seg, win );
  
  /* get the element */
  /* TODO: maybe use the according DMAPP_{SIZE}? */
  status = dmapp_get( result_addr, target_addr, seg_ptr, target_pe, type_size, DMAPP_BYTE);
  _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);

  /* operate on the elements */
  if (op == foMPI_REPLACE) {
    /* TODO: without mutex, put can be non-blocking with origin_addr */
    buffer = (void*) origin_addr;
  } else if (op == foMPI_NO_OP) {
  } else {
    buffer = malloc( type_size );
    res = foMPI_RMA_op(buffer, (void*) origin_addr, result_addr, op, datatype, 1);
  }

  /* put the element back */
  /* since we must free the buffer, the put operation is blocking */  
  if ((op != foMPI_NO_OP) && (res == MPI_SUCCESS)) {
    status = dmapp_put( target_addr, seg_ptr, target_pe, buffer, type_size, DMAPP_BYTE);
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
  }
  /* release the mutex if necessary */
  foMPI_Release_window_mutex( &status, target_pe, win_ptr, win_ptr_seg, win );
  
  if ( (op != foMPI_REPLACE) && (op != foMPI_NO_OP)) {
    free(buffer);
  }

#ifdef PAPI
  timing_record( 20 );
#endif

  return res;
}

int foMPI_Compare_and_swap(const void *origin_addr, const void *compare_addr, void *result_addr, MPI_Datatype datatype, int target_rank, MPI_Aint target_disp, foMPI_Win win) {
#ifdef PAPI
  timing_record( -1 );
#endif

  int size;
  void* target_addr;
  int found;
  dmapp_return_t status;
  foMPI_Win_desc_t* win_ptr = NULL; /* to get rid of the maybe-uninitialized warnings */
  dmapp_seg_desc_t* win_ptr_seg = NULL; /* to get rid of the maybe-uninitialized warnings */
  dmapp_seg_desc_t* seg_ptr;
  foMPI_Win_dynamic_element_t* current;

  if ( target_rank == MPI_PROC_NULL ) {
    return MPI_SUCCESS;
  }

  assert( (target_rank >= 0) && (target_rank < win->commsize) );
  dmapp_pe_t target_pe = (dmapp_pe_t) win->group_ranks[target_rank];
  
  MPI_Type_size( datatype, &size );

  if( win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC ) {
    /* check already if the the requested memory region is within the boundaries */

    /* checks if the local information about the remote dynamic window are up-to-date: if not, they are fetched */
    get_dynamic_window_regions( &status, target_rank, target_pe, win );

    /* check local list for entry */
    current = win->dynamic_list[target_rank].remote_mem_regions;
    found = 0;
    if( current != NULL ) {
      if ( ( target_disp >= current->base ) && ( target_disp+size <= (current->base+current->size) ) ) {
        found = 1;
      }
      while( (current->next != NULL) && (found == 0) ) {
        current = current->next;
        if ( ( target_disp >= current->base ) && ( target_disp+size <= (current->base+current->size) ) ) {
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
    target_addr = (char*) win->win_array[target_rank].base + target_disp * win->win_array[target_rank].disp_unit;
    seg_ptr = &(win->win_array[target_rank].seg);
  }

  switch (size) {
    case 8:
      status = dmapp_acswap_qw( result_addr, target_addr, seg_ptr, target_pe, *(int64_t *)compare_addr, *(int64_t *)origin_addr );
      _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
      break;
    default:
      /* in the general case, we need to sync */
      if ( win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC ) {
        win_ptr = (void*) win->dynamic_list[target_rank].win_ptr;
        win_ptr_seg = &(win->dynamic_list[target_rank].win_ptr_seg);
      } else {
        win_ptr = (void*) win->win_array[target_rank].win_ptr;
        win_ptr_seg = &(win->win_array[target_rank].win_ptr_seg);
      }
      /* set the mutex */
      foMPI_Set_window_mutex( &status, target_pe, win_ptr, win_ptr_seg, win );
      
      /* get the elements */
      /* TODO: maybe use the right dmapp_size */
      status = dmapp_get( result_addr, target_addr, seg_ptr, target_pe, size, DMAPP_BYTE);
      _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);

      if ( memcmp( result_addr, compare_addr, size ) == 0 ) {
        status = dmapp_put(target_addr, seg_ptr, target_pe, (void*) origin_addr, size, DMAPP_BYTE );
        _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
      }
    
      /* release the mutex if necessary */
      foMPI_Release_window_mutex( &status, target_pe, win_ptr, win_ptr_seg, win );
      break;
  }

#ifdef PAPI
  timing_record( 18 );
#endif

  return MPI_SUCCESS;
}
