// Copyright (c) 2012 The Trustees of University of Illinois. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "fompi.h"

int foMPI_Win_free(foMPI_Win *win) {

  dmapp_pe_t pe = (*win)->commrank;
  int i;
  foMPI_Win_dynamic_element_t* current;
  foMPI_Win_dynamic_element_t* previous;

  /* TODO: Could this be removed? */
  MPI_Barrier((*win)->comm);

  if ( (*win)->create_flavor ==  foMPI_WIN_FLAVOR_DYNAMIC ) {

    for( i = 0 ; i<(*win)->commsize ; i++ ) {

      if( i == pe ) {

        while( (*win)->win_dynamic_mem_regions != NULL ) {
        
        _foMPI_mem_unregister(&((*win)->win_dynamic_mem_regions->seg),*win);

		_foMPI_mem_unregister(&((*win)->win_dynamic_mem_regions_seg), *win);

          previous = (*win)->win_dynamic_mem_regions;

          (*win)->win_dynamic_mem_regions_seg = (*win)->win_dynamic_mem_regions->next_seg; /* TODO: just use pointer? */
          (*win)->win_dynamic_mem_regions = (*win)->win_dynamic_mem_regions->next;
    
          _foMPI_FREE( previous );
        }

      } else {
        if ( (*win)->dynamic_list[i].remote_mem_regions != NULL ) {
          current = (*win)->dynamic_list[i].remote_mem_regions;
          while( current->next != NULL ) {
            previous = current;
            current = current->next;
            _foMPI_FREE( previous );
          }
          _foMPI_FREE( current );
        }
      }
    }

    _foMPI_mem_unregister(&((*win)->dynamic_list[pe].win_ptr_seg), *win);

    _foMPI_mem_unregister(&((*win)->dynamic_list[pe].pscw_matching_exposure_seg), *win);

    _foMPI_FREE((*win)->dynamic_list[pe].pscw_matching_exposure);
    _foMPI_FREE((*win)->dynamic_list);

  } else {

#ifdef XPMEM
    /* unmap the remote memory */
    for( i=0 ; i<(*win)->onnode_size ; i++ ) {
      if ((*win)->xpmem_array[i].base_apid != -1) {
        //printf("%i: unmap base\n", debug_pe);
        foMPI_unmap_memory_xpmem( &((*win)->xpmem_array[i].base_apid), (*win)->xpmem_array[i].base, (*win)->xpmem_array[i].base_offset );
      }
      foMPI_unmap_memory_xpmem( &((*win)->xpmem_array[i].win_ptr_apid), (*win)->xpmem_array[i].win_ptr, (*win)->xpmem_array[i].win_ptr_offset );
      foMPI_unmap_memory_xpmem( &((*win)->xpmem_array[i].pscw_matching_exposure_apid), (*win)->xpmem_array[i].pscw_matching_exposure, (*win)->xpmem_array[i].pscw_matching_exposure_offset );
      foMPI_unmap_memory_xpmem( &((*win)->xpmem_array[i].notif_queue_apid), (*win)->xpmem_array[i].notif_queue, (*win)->xpmem_array[i].notif_queue_offset );
      foMPI_unmap_memory_xpmem( &((*win)->xpmem_array[i].notif_queue_state_apid), (*win)->xpmem_array[i].notif_queue_state, (*win)->xpmem_array[i].notif_queue_state_offset );
    }
    _foMPI_FREE( (*win)->xpmem_array );
#endif

    if ( (*win)->win_array[pe].base != NULL ) {
    	_foMPI_mem_unregister(&((*win)->win_array[pe].seg),*win);
#ifdef XPMEM
      if ((*win)->xpmem_segdesc.base.seg != -1 ) {
        //printf("%i: unexport base\n", debug_pe);
        foMPI_unexport_memory_xpmem( (*win)->xpmem_segdesc.base );
      }
#endif
    }

    if ( (*win)->create_flavor == foMPI_WIN_FLAVOR_ALLOCATE ) {
    	_foMPI_FREE( (*win)->win_array[pe].base );
    }

    _foMPI_mem_unregister(&((*win)->win_array[pe].win_ptr_seg),*win);

#ifdef XPMEM
    foMPI_unexport_memory_xpmem( (*win)->xpmem_segdesc.win_ptr );
#endif

    _foMPI_mem_unregister(&((*win)->win_array[pe].pscw_matching_exposure_seg), *win);

#ifdef XPMEM
    foMPI_unexport_memory_xpmem( (*win)->xpmem_segdesc.pscw_matching_exposure );
    foMPI_unexport_memory_xpmem( (*win)->xpmem_segdesc.notif_queue );
    foMPI_unexport_memory_xpmem( (*win)->xpmem_segdesc.notif_queue_state );
#endif
    xpmem_notif_free_queue(*win);
    _foMPI_FREE((*win)->win_array[pe].pscw_matching_exposure);
    _foMPI_FREE((*win)->win_array);

#ifdef XPMEM
    if ( (*win)->onnode_lower_bound == -1 ) {
    	_foMPI_FREE( (*win)->onnode_ranks );
    }
    MPI_Group_free( &((*win)->win_onnode_group) );
    MPI_Comm_free( &((*win)->win_onnode_comm) );
#endif
  }

  _foMPI_FREE((*win)->pscw_matching_access);
#ifdef UGNI
  _fompi_notif_uq_finalize(&((*win)->destination_cq_discarded));
  _foMPI_TRACE_LOG(3, "fompi_oset    Finalize \n");
#endif
  /* free remainder of exclusive locks list
   * at this point, if the programm is MPI consistent, there shouldn't be a element in the list */
  if( (*win)->excl_locks != NULL ) {
    foMPI_Win_excl_lock_elem_t* previous;
    foMPI_Win_excl_lock_elem_t* current;
    previous = (*win)->excl_locks;
    current = (*win)->excl_locks->next;
    
    while( current != NULL ) {
    	_foMPI_FREE( previous );
      previous = current;
      current = current->next;
    }

    _foMPI_FREE( previous ); /* free last element */

    (*win)->excl_locks = NULL;
  }
  
  _foMPI_FREE((*win)->group_ranks);
  if ( (*win)->name != NULL ) {
	  _foMPI_FREE( (*win)->name );
  }
  MPI_Barrier( (*win)->comm );
  _foMPI_FREE(*win);

  *win = foMPI_WIN_NULL;

  return MPI_SUCCESS; 
}
