// Copyright (c) 2012 The Trustees of University of Illinois. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
//#include <intrinsics.h> 
#include <string.h>
#include "fompi.h"

/* We have to keep track of which locks are exclusive.
 * To implement it in a scalable way (without O(p) memory overhead), we use a list.
 * We insert at the head of the list */
inline static void foMPI_insert_excl_lock( int rank, foMPI_Win win ) {

  foMPI_Win_excl_lock_elem_t* new_elem = malloc( sizeof(foMPI_Win_excl_lock_elem_t) );
  new_elem->rank = rank;

  new_elem->next = win->excl_locks;
  win->excl_locks = new_elem;

}

/* functions assumes that we always will find the rank */
inline static void foMPI_delete_excl_lock( int rank, foMPI_Win win ) {

  foMPI_Win_excl_lock_elem_t* previous;
  foMPI_Win_excl_lock_elem_t* current;

  assert( win->excl_locks != NULL );

  if( win->excl_locks->rank == rank ) { /* remove head element */
    current = win->excl_locks;
    win->excl_locks = win->excl_locks->next;
    free( current );
  } else {

    previous = win->excl_locks;
    current = win->excl_locks->next;

    while ( current->rank != rank ) {
      previous = current;
      current = current->next;
    }

    previous->next = current->next;
    free( current );
  }

}

inline static int foMPI_search_excl_lock( int rank, foMPI_Win win ) {

  if( win->excl_locks == NULL ) /* no exclusive locks */
    return foMPI_LOCK_SHARED;

  foMPI_Win_excl_lock_elem_t* current = win->excl_locks;

  while( (current->rank != rank) && (current->next != NULL) ) {
    current = current->next;
  }

  if (current->rank == rank) {
    return foMPI_LOCK_EXCLUSIVE;
  }
    
  return foMPI_LOCK_SHARED;
}

/* TODO: only does busy waiting, should be doing at least some queueing */
/* TODO: asserts unused */
int foMPI_Win_lock(int lock_type, int rank, int assert, foMPI_Win win) {
#ifdef PAPI
  timing_record( -1 );
#endif
  dmapp_return_t status;
  uint64_t result;
  uint64_t bit_mask = ~(uint64_t)0 << 32;

  foMPI_Win_desc_t* win_ptr;
  dmapp_seg_desc_t* win_ptr_seg;
  foMPI_Win_desc_t* win_ptr_master;
  dmapp_seg_desc_t* win_ptr_seg_master;
 
  assert( (lock_type == foMPI_LOCK_SHARED) || (lock_type == foMPI_LOCK_EXCLUSIVE) );

  if ( rank == MPI_PROC_NULL ) {
    return MPI_SUCCESS;
  }
 
  assert((rank >= 0) && (rank < win->commsize));
  dmapp_pe_t target_pe = (dmapp_pe_t) win->group_ranks[rank];

  if ( win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC ) {
    win_ptr = win->dynamic_list[rank].win_ptr;
    win_ptr_seg = &(win->dynamic_list[rank].win_ptr_seg);
    win_ptr_master = win->dynamic_list[MASTER].win_ptr;
    win_ptr_seg_master = &(win->dynamic_list[MASTER].win_ptr_seg);
  } else {
    win_ptr = win->win_array[rank].win_ptr;
    win_ptr_seg = &(win->win_array[rank].win_ptr_seg);
    win_ptr_master = win->win_array[MASTER].win_ptr;
    win_ptr_seg_master = &(win->win_array[MASTER].win_ptr_seg);
  }

  if (lock_type == foMPI_LOCK_EXCLUSIVE) {
    if ( win->local_exclusive_count == 0) {
      /* check if lock_all mutex is set on the master */
      /* if not, then acquire lock_all mutex */
      /* TODO: howto to count exclusive locks in the system? */
      dmapp_pe_t master_pe = (dmapp_pe_t) win->group_ranks[MASTER];

      do {
        /* check if lock_all mutex is set on the master */
        /* if not, then acquire lock_all mutex */
        /* TODO: Are we in trouble, because of int64_t operands in acswap and uint64_t variable? */
        status = dmapp_afadd_qw( &result, (void*) &(win_ptr_master->lock_all_mutex), win_ptr_seg_master, master_pe, (int64_t) 1 );
        _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);

        if (!( result & bit_mask )) {
          /* try to acquire exclusive on target */
          status = dmapp_acswap_qw( &result, (void*) &(win_ptr->lock_mutex), win_ptr_seg, target_pe, (int64_t) 0, (int64_t) 1 );
          _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
        
          /* if not acquired back off: release lock_all mutex  */
          if (result == 0) {
            break;
          } else {
            /* TODO: non-blocking? */
            status = dmapp_aadd_qw( (void*) &(win_ptr_master->lock_all_mutex), win_ptr_seg_master, master_pe, (int64_t) -1 );
            _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
          }
        } else {
          /* back off on master lock_all mutex */
          /* TODO: non-blocking? */
          status = dmapp_aadd_qw( (void*) &(win_ptr_master->lock_all_mutex), win_ptr_seg_master, master_pe, (int64_t) -1 );
          _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
        }
      } while (1);
      win->local_exclusive_count++;
    } else {
      /* we already hold the global exclusive lock */
      /* try to acquire exclusive on target */
      do {
        status = dmapp_acswap_qw( &result, (void*) &(win_ptr->lock_mutex), win_ptr_seg, target_pe, (int64_t) 0, (int64_t) 1 );
        _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
      } while ( result != 0 );
      win->local_exclusive_count++;
    }

    foMPI_insert_excl_lock( rank, win );

  } else { /* foMPI_LOCK_SHARED */
    /* propagate read intention for target process */
    /* TODO: non-blocking? */
    status = dmapp_afadd_qw( (void*) &result, (void*) &(win_ptr->lock_mutex), win_ptr_seg, target_pe, (int64_t) 2 );
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
      
    /* check if there is an exclusive lock */
    while ( result & 1) {
      status = dmapp_get( &result, (void*) &(win_ptr->lock_mutex), win_ptr_seg, target_pe, 1, DMAPP_QW );
      _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
    };
  }

#ifdef PAPI
  timing_record( 10 );
#endif
  return MPI_SUCCESS;
}

int foMPI_Win_unlock(int rank, foMPI_Win win) {
#ifdef PAPI
  timing_record( -1 );
#endif
  dmapp_return_t status;
  foMPI_Win_desc_t* win_ptr;
  dmapp_seg_desc_t* win_ptr_seg;
  foMPI_Win_desc_t* win_ptr_master;
  dmapp_seg_desc_t* win_ptr_seg_master;
  int lock_type;

  if ( rank == MPI_PROC_NULL ) {
    return MPI_SUCCESS;
  }

  assert((rank >= 0) && (rank < win->commsize));
  dmapp_pe_t target_pe = (dmapp_pe_t) win->group_ranks[rank];

  if ( win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC ) {
    win_ptr = win->dynamic_list[rank].win_ptr;
    win_ptr_seg = &(win->dynamic_list[rank].win_ptr_seg);
    win_ptr_master = win->dynamic_list[MASTER].win_ptr;
    win_ptr_seg_master = &(win->dynamic_list[MASTER].win_ptr_seg);
  } else {
    win_ptr = win->win_array[rank].win_ptr;
    win_ptr_seg = &(win->win_array[rank].win_ptr_seg);
    win_ptr_master = win->win_array[MASTER].win_ptr;
    win_ptr_seg_master = &(win->win_array[MASTER].win_ptr_seg);
  }

#ifdef PAPI
  timing_record( 8 );
#endif

  lock_type = foMPI_search_excl_lock( rank, win );

  foMPI_Win_flush( rank, win );

  /* release the lock(s) */
  if ( lock_type == foMPI_LOCK_EXCLUSIVE) {
    dmapp_pe_t master_pe = (dmapp_pe_t) win->group_ranks[MASTER];

    /* release lock on target */
    /* TODO: non-blocking? */
    status = dmapp_aadd_qw( (void*) &(win_ptr->lock_mutex), win_ptr_seg, target_pe, (int64_t) -1 );
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
  
    win->local_exclusive_count--;

    if ( win->local_exclusive_count == 0 ) {
      /* release lock on master process */
     status = dmapp_aadd_qw_nbi( (void*) &(win_ptr_master->lock_all_mutex), win_ptr_seg_master, master_pe, (int64_t) -1 );
      _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
    }

    foMPI_delete_excl_lock( rank, win );
 
  } else { /* foMPI_LOCK_SHARED */
    /* release lock on target */
    status = dmapp_aadd_qw_nbi( (void*) &(win_ptr->lock_mutex), win_ptr_seg, target_pe, (int64_t) -2 );
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
  }
  
#ifdef PAPI
  timing_record( 9 );
#endif

  return MPI_SUCCESS;
}

int foMPI_Win_lock_all(int assert, foMPI_Win win) {
#ifdef PAPI
  timing_record( -1 );
#endif

  dmapp_return_t status;
  uint64_t result;
  uint64_t bit_mask = ~(~(uint64_t)0 << 32);

  foMPI_Win_desc_t* win_ptr_master;
  dmapp_seg_desc_t* win_ptr_seg_master;
  dmapp_pe_t master_pe = (dmapp_pe_t) win->group_ranks[MASTER];
 
  if ( win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC ) {
    win_ptr_master = win->dynamic_list[MASTER].win_ptr;
    win_ptr_seg_master = &(win->dynamic_list[MASTER].win_ptr_seg);
  } else {
    win_ptr_master = win->win_array[MASTER].win_ptr;
    win_ptr_seg_master = &(win->win_array[MASTER].win_ptr_seg);
  }

  status = dmapp_afadd_qw( (void*) &result, (void*) &(win_ptr_master->lock_all_mutex), win_ptr_seg_master, master_pe, (int64_t) 4294967296 ); /* 2^32 = 4294967296 */
  _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);

  while( result & bit_mask ) {
    status = dmapp_get( &result, (void*) &(win_ptr_master->lock_all_mutex), win_ptr_seg_master, master_pe, 1, DMAPP_QW );
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
  }

#ifdef PAPI
  timing_record( 16 );
#endif

  return MPI_SUCCESS;
}

int foMPI_Win_unlock_all(foMPI_Win win) {
#ifdef PAPI
  timing_record( -1 );
#endif

  dmapp_return_t status;

  foMPI_Win_desc_t* win_ptr_master;
  dmapp_seg_desc_t* win_ptr_seg_master;

  /* target and local completion */
  foMPI_Win_flush_all( win );
  
  /* release lock on master process */
  dmapp_pe_t master_pe = (dmapp_pe_t) win->group_ranks[MASTER];
 
  if ( win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC ) {
    win_ptr_master = win->dynamic_list[MASTER].win_ptr;
    win_ptr_seg_master = &(win->dynamic_list[MASTER].win_ptr_seg);
  } else {
    win_ptr_master = win->win_array[MASTER].win_ptr;
    win_ptr_seg_master = &(win->win_array[MASTER].win_ptr_seg);
  }
  status = dmapp_aadd_qw_nbi( (void*) &(win_ptr_master->lock_all_mutex), win_ptr_seg_master, master_pe, (int64_t) -4294967296 ); /* 2^32 = 4294967296 */
  _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);

#ifdef PAPI
  timing_record( 17 );
#endif

  return MPI_SUCCESS;
}

/* TODO: write a test for the flush functions */
int foMPI_Win_flush(int rank, foMPI_Win win) {

#ifdef PAPI
  timing_record( -1 );
#endif

  dmapp_return_t status;

  if ( rank == MPI_PROC_NULL ) {
    return MPI_SUCCESS;
  }

  assert((rank >= 0) && (rank < win->commsize));

#ifdef XPMEM
  /* XPMEM completion */
  __sync_synchronize();
#endif

  /* completion */
  if (win->nbi_counter > 0) {
    status = dmapp_gsync_wait();
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
    win->nbi_counter = 0;
  }

#ifdef PAPI
  timing_record( 4 );
#endif

  return MPI_SUCCESS;
}

int foMPI_Win_flush_local(int rank, foMPI_Win win) {
  return foMPI_Win_flush(rank, win);
}

int foMPI_Win_flush_all(foMPI_Win win) {
  return foMPI_Win_flush( 0 /* dummy */, win);
}

int foMPI_Win_flush_local_all(foMPI_Win win) {
  return foMPI_Win_flush( 0 /* dummy */, win);
}

int foMPI_Win_sync(foMPI_Win win) {
  return MPI_SUCCESS;
}
