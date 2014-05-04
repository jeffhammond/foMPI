// Copyright (c) 2012 The Trustees of University of Illinois. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "fompi.h"

int foMPI_Win_free(foMPI_Win *win) {

  dmapp_return_t status;
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
        
          status = dmapp_mem_unregister(&((*win)->win_dynamic_mem_regions->seg));
          _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);

          status = dmapp_mem_unregister(&((*win)->win_dynamic_mem_regions_seg));
          _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);

          previous = (*win)->win_dynamic_mem_regions;

          (*win)->win_dynamic_mem_regions_seg = (*win)->win_dynamic_mem_regions->next_seg; /* TODO: just use pointer? */
          (*win)->win_dynamic_mem_regions = (*win)->win_dynamic_mem_regions->next;
    
          free( previous );
        }

      } else {
        if ( (*win)->dynamic_list[i].remote_mem_regions != NULL ) {
          current = (*win)->dynamic_list[i].remote_mem_regions;
          while( current->next != NULL ) {
            previous = current;
            current = current->next;
            free( previous );
          }
          free( current );
        }
      }
    }

    status = dmapp_mem_unregister(&((*win)->dynamic_list[pe].win_ptr_seg));
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);

    status = dmapp_mem_unregister(&((*win)->dynamic_list[pe].pscw_matching_exposure_seg));
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);

    free((*win)->dynamic_list[pe].pscw_matching_exposure);
    free((*win)->dynamic_list);

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
    }
    free( (*win)->xpmem_array );
#endif

    if ( (*win)->win_array[pe].base != NULL ) {
      status = dmapp_mem_unregister(&((*win)->win_array[pe].seg));
      _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
#ifdef XPMEM
      if ((*win)->xpmem_segdesc.base.seg != -1 ) {
        //printf("%i: unexport base\n", debug_pe);
        foMPI_unexport_memory_xpmem( (*win)->xpmem_segdesc.base );
      }
#endif
    }

    if ( (*win)->create_flavor == foMPI_WIN_FLAVOR_ALLOCATE ) {
      free( (*win)->win_array[pe].base );
    }

    status = dmapp_mem_unregister(&((*win)->win_array[pe].win_ptr_seg));
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
#ifdef XPMEM
    foMPI_unexport_memory_xpmem( (*win)->xpmem_segdesc.win_ptr );
#endif

    status = dmapp_mem_unregister(&((*win)->win_array[pe].pscw_matching_exposure_seg));
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
#ifdef XPMEM
    foMPI_unexport_memory_xpmem( (*win)->xpmem_segdesc.pscw_matching_exposure );
#endif

    free((*win)->win_array[pe].pscw_matching_exposure);
    free((*win)->win_array);

#ifdef XPMEM
    if ( (*win)->onnode_lower_bound == -1 ) {
      free( (*win)->onnode_ranks );
    }
    MPI_Group_free( &((*win)->win_onnode_group) );
    MPI_Comm_free( &((*win)->win_onnode_comm) );
#endif
  }

  free((*win)->pscw_matching_access);
  /* free remainder of exclusive locks list
   * at this point, if the programm is MPI consistent, there shouldn't be a element in the list */
  if( (*win)->excl_locks != NULL ) {
    foMPI_Win_excl_lock_elem_t* previous;
    foMPI_Win_excl_lock_elem_t* current;
    previous = (*win)->excl_locks;
    current = (*win)->excl_locks->next;
    
    while( current != NULL ) {
      free( previous );
      previous = current;
      current = current->next;
    }

    free( previous ); /* free last element */

    (*win)->excl_locks = NULL;
  }
  
  free((*win)->group_ranks);
  if ( (*win)->name != NULL ) {
    free( (*win)->name );
  }
  MPI_Barrier( (*win)->comm );
  free(*win);

  *win = foMPI_WIN_NULL;

  return MPI_SUCCESS; 
}
