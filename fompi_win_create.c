// Copyright (c) 2012 The Trustees of University of Illinois. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stdlib.h>

#include "fompi.h"

int foMPI_Win_create(void *base, MPI_Aint size, int disp_unit, MPI_Info info, MPI_Comm comm, foMPI_Win *win) {

  dmapp_return_t status;
  int i;
  int* temp;
  MPI_Group group_comm_world, group;
  foMPI_Win_struct_t win_info;

  assert( size >= 0 );
  assert( disp_unit > 0 );

  /* allocate the window */
  *win = malloc( sizeof(foMPI_Win_desc_t) );
  assert(*win != NULL);

  /* the window communicator specific informations */
  (*win)->comm = comm;
  MPI_Comm_size( comm, &((*win)->commsize) );
  MPI_Comm_rank( comm, &((*win)->commrank) );

  /* get all ranks from the members of the group */
  (*win)->group_ranks = malloc((*win)->commsize * sizeof(int32_t)); 
  assert((*win)->group_ranks != NULL);
  
  temp = malloc((*win)->commsize * sizeof(int));
  assert( temp != NULL );
  for( i=0 ; i<(*win)->commsize ; i++) {
    temp[i] = i;
  }
  MPI_Comm_group(comm, &group);
  MPI_Comm_group(MPI_COMM_WORLD, &group_comm_world);
  MPI_Group_translate_ranks(group, (*win)->commsize, &temp[0], group_comm_world, &((*win)->group_ranks[0]));

  free(temp);
  MPI_Group_free(&group_comm_world);

#ifdef XPMEM
  /* get communicator for all onnode processes that are part of the window */
  MPI_Group_intersection( onnode_group, group /* window group */, &((*win)->win_onnode_group) );
  MPI_Comm_create( comm, (*win)->win_onnode_group, &((*win)->win_onnode_comm) );

  /* mapping of the global ranks (of the window communicator */
  MPI_Group_size( (*win)->win_onnode_group, &((*win)->onnode_size) );

  temp = malloc( (*win)->onnode_size * sizeof(int));
  assert( temp != NULL );

  (*win)->onnode_ranks = malloc( (*win)->onnode_size * sizeof(int));
  assert( (*win)->onnode_ranks != NULL );

  for( i=0 ; i<(*win)->onnode_size ; i++) {
    temp[i] = i;
  }
  MPI_Group_translate_ranks((*win)->win_onnode_group, (*win)->onnode_size, &temp[0], group, &((*win)->onnode_ranks[0]) );

  for( i=1 ; i<(*win)->onnode_size ; i++ ) {
    if( (*win)->onnode_ranks[i] != ( (*win)->onnode_ranks[i-1]+1 ) ) {
      break; 
    }
  }
  if (i == (*win)->onnode_size) {
    (*win)->onnode_lower_bound = (*win)->onnode_ranks[0];
    (*win)->onnode_upper_bound = (*win)->onnode_ranks[(*win)->onnode_size-1];
    free( (*win)->onnode_ranks );
  } else {
    (*win)->onnode_lower_bound = -1;
    (*win)->onnode_upper_bound = -1;
  }

  free( temp );
#endif
  MPI_Group_free(&group);

  /* allocate the memory for the remote window information */
  (*win)->win_array = malloc( (*win)->commsize * sizeof(foMPI_Win_struct_t) );
  assert((*win)->win_array != NULL);

  /* set the information for the remote processes */
  if ( (base != NULL) && (size > 0) ) {
    win_info.base = base;
    status = dmapp_mem_register( base, (uint64_t) size, &(win_info.seg) );
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
#ifdef XPMEM
    (*win)->xpmem_segdesc.base = foMPI_export_memory_xpmem(base, size);
#endif
  } else {
    win_info.base = NULL;
#ifdef XPMEM
    (*win)->xpmem_segdesc.base.seg = -1;
#endif
  }

  win_info.size = size;
  win_info.disp_unit = disp_unit;

  win_info.win_ptr = *win;
  status = dmapp_mem_register( *win, (uint64_t) sizeof(foMPI_Win_desc_t), &(win_info.win_ptr_seg) );
  _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
#ifdef XPMEM
  (*win)->xpmem_segdesc.win_ptr = foMPI_export_memory_xpmem(*win, sizeof(foMPI_Win_desc_t));
#endif

  /* PCSW matching */
  win_info.pscw_matching_exposure = malloc( (*win)->commsize * sizeof(uint64_t) );
  assert( win_info.pscw_matching_exposure != NULL );

  (*win)->pscw_matching_access = malloc( (*win)->commsize * sizeof(uint32_t) );
  assert( (*win)->pscw_matching_access != NULL );

  for( i=0 ; i<(*win)->commsize ; i++ ){
    win_info.pscw_matching_exposure[i] = 0;
    (*win)->pscw_matching_access[i] = 0;
  }

  status = dmapp_mem_register( win_info.pscw_matching_exposure, (uint64_t) (*win)->commsize * sizeof(uint64_t), &(win_info.pscw_matching_exposure_seg) );
  _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
#ifdef XPMEM
  (*win)->xpmem_segdesc.pscw_matching_exposure = foMPI_export_memory_xpmem( win_info.pscw_matching_exposure, (*win)->commsize * sizeof(uint64_t) );
#endif

  /* lock synchronisation */
  (*win)->mutex = foMPI_MUTEX_NONE;
  (*win)->lock_mutex = 0; /* no current access */
  if ( (*win)->commrank == MASTER ) {
    (*win)->lock_all_mutex = 0; /* no current access */
  }
  (*win)->local_exclusive_count = 0;
  (*win)->excl_locks = NULL;

  /* management of rma operations */
  (*win)->nbi_counter = 0;

  (*win)->name = NULL;
 
  (*win)->create_flavor = foMPI_WIN_FLAVOR_CREATE;

  MPI_Allgather( &win_info, sizeof(foMPI_Win_struct_t), MPI_BYTE, &((*win)->win_array[0]), sizeof(foMPI_Win_struct_t), MPI_BYTE, comm );

#ifdef XPMEM
  /* exchange the exposure infos with the onnode processes */
  (*win)->xpmem_array = malloc( (*win)->onnode_size * sizeof(fompi_xpmem_addr_t) );
  assert( (*win)->xpmem_array != NULL );
  fompi_xpmem_info_t* xpmem_temp = malloc( (*win)->onnode_size * sizeof(fompi_xpmem_info_t) );
 
  MPI_Allgather( &((*win)->xpmem_segdesc), sizeof(fompi_xpmem_info_t), MPI_BYTE, &(xpmem_temp[0]), sizeof(fompi_xpmem_info_t), MPI_BYTE, (*win)->win_onnode_comm );
  
  /* map the onnode memory */
  for( i=0 ; i<(*win)->onnode_size ; i++ ) {
    if (xpmem_temp[i].base.seg != -1) {
      (*win)->xpmem_array[i].base = foMPI_map_memory_xpmem( xpmem_temp[i].base, (*win)->win_array[foMPI_onnode_rank_local_to_global( i, (*win) )].size,
        &((*win)->xpmem_array[i].base_apid), &((*win)->xpmem_array[i].base_offset) );
    } else {
      (*win)->xpmem_array[i].base_apid = -1;
    }
    (*win)->xpmem_array[i].win_ptr = foMPI_map_memory_xpmem( xpmem_temp[i].win_ptr, sizeof(foMPI_Win_desc_t), &((*win)->xpmem_array[i].win_ptr_apid), &((*win)->xpmem_array[i].win_ptr_offset) );
    (*win)->xpmem_array[i].pscw_matching_exposure = foMPI_map_memory_xpmem( xpmem_temp[i].pscw_matching_exposure, (*win)->commsize * sizeof(uint64_t),
      &((*win)->xpmem_array[i].pscw_matching_exposure_apid), &((*win)->xpmem_array[i].pscw_matching_exposure_offset) );
  }

  free( xpmem_temp );
#endif

  return MPI_SUCCESS;
}
