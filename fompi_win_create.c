// Copyright (c) 2012 The Trustees of University of Illinois. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stdlib.h>

#include "fompi.h"

int foMPI_Win_create(void *base, MPI_Aint size, int disp_unit, MPI_Info info, MPI_Comm comm, foMPI_Win *win) {

  int i;
  int* temp;
  MPI_Group group_comm_world, group;
  foMPI_Win_struct_t win_info;

  assert( size >= 0 );
  assert( disp_unit > 0 );

  /* allocate the window */
  void * memptr;
  _foMPI_ALIGNED_ALLOC(&memptr,  sizeof(foMPI_Win_desc_t) )
  *win = memptr;
  assert(*win != NULL);

  /**transition info. As soon as an foMPI Communicator is implemented update this UGNI use this*/
  (*win)->fompi_comm = glob_info.comm_world;
  /* the window communicator specific informations */
  (*win)->comm = comm;
  MPI_Comm_size( comm, &((*win)->commsize) );
  MPI_Comm_rank( comm, &((*win)->commrank) );

  /* get all ranks from the members of the group */
  (*win)->group_ranks = _foMPI_ALLOC((*win)->commsize * sizeof(int32_t));
  assert((*win)->group_ranks != NULL);
  
  temp = _foMPI_ALLOC((*win)->commsize * sizeof(int));
  assert( temp != NULL );
  for( i=0 ; i<(*win)->commsize ; i++) {
    temp[i] = i;
  }
  MPI_Comm_group(comm, &group);
  MPI_Comm_group(MPI_COMM_WORLD, &group_comm_world);
  MPI_Group_translate_ranks(group, (*win)->commsize, &temp[0], group_comm_world, &((*win)->group_ranks[0]));

  _foMPI_FREE(temp);
  MPI_Group_free(&group_comm_world);
#ifdef UGNI
  gni_return_t status_gni;

  #ifdef _foMPI_UGNI_WIN_RELATED_SRC_CQ
  /*
  	 * Create the source completion queue.
  	 *     nic_handle is the NIC handle that this completion queue will be
  	 *          associated with.
  	 *     number_of_cq_entries is the size of the completion queue.
  	 *     zero is the delay count is the number of allowed events before an
  	 *          interrupt is generated.
  	 *     GNI_CQ_NOBLOCK states that the operation mode is non-blocking.
  	 *     NULL states that no user supplied callback function is defined.
  	 *     NULL states that no user supplied pointer is passed to the callback
  	 *          function.
  	 *     cq_handle is the handle that is returned pointing to this newly
  	 *          created completion queue.
  	 */
  	(*win)->number_of_source_cq_entries = _foMPI_NUM_SRC_CQ_ENTRIES;
  	status_gni = GNI_CqCreate((*win)->fompi_comm->nic_handle, (*win)->number_of_source_cq_entries , 0,
  	_foMPI_SRC_CQ_MODE, NULL, NULL, &((*win)->source_cq_handle));
  	_check_gni_status(status_gni, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);
  	_foMPI_TRACE_LOG(3, "GNI_CqCreate      source with %i entries\n", (*win)->number_of_source_cq_entries);
#endif
  	(*win)->counter_ugni_nbi = 0;

  	  /*
  		 * Create the destination_completion queue.
  		 *     nic_handle is the NIC handle that this completion queue will be
  		 *          associated with.
  		 *     number_of_dest_cq_entries is the size of the completion queue.
  		 *     zero is the delay count is the number of allowed events before
  		 *          an interrupt is generated.
  		 *     GNI_CQ_NOBLOCK states that the operation mode is non-blocking.
  		 *     NULL states that no user supplied callback function is defined.
  		 *     NULL states that no user supplied pointer is passed to the
  		 *          callback function.
  		 *     destination_cq_handle is the handle that is returned pointing to
  		 *          this newly created completion queue.
  		 */
  		(*win)->number_of_dest_cq_entries = _foMPI_NUM_DST_CQ_ENTRIES;
  	//we try to use the handler instead of only the dispatcher trying to decrease the latency of the notification
  	#ifdef NOTIFICATION_SOFTWARE_AGENT
  	//	status_gni = GNI_CqCreate(glob_info.comm_world->nic_handle, (*win)->number_of_dest_cq_entries, 0,
  	//			foMPI_DST_CQ_MODE, &foMPI_NotificationHandler, (*win), &((*win)->destination_cq_handle));
  	#else
  		//TODO: substitute comme world with foMPI_Comm
  		status_gni = GNI_CqCreate((*win)->fompi_comm->nic_handle, (*win)->number_of_dest_cq_entries, 0,
  			_foMPI_DST_CQ_MODE, NULL, NULL, &((*win)->destination_cq_handle));
  	#endif
  		_check_gni_status(status_gni, GNI_RC_SUCCESS, (char*) __FILE__, __LINE__);
  		_foMPI_TRACE_LOG(3 , "GNI_CqCreate      destination with %i entries\n",(*win)->number_of_dest_cq_entries);

  		/*init backup_queue*/
  		(*win)->destination_cq_discarded = _fompi_notif_uq_init();
  		_foMPI_TRACE_LOG(3, "fompi_oset    Created \n");
  #endif
#ifdef XPMEM
  /* get communicator for all onnode processes that are part of the window */
  MPI_Group_intersection( glob_info.onnode_group, group /* window group */, &((*win)->win_onnode_group) );
  MPI_Comm_create( comm, (*win)->win_onnode_group, &((*win)->win_onnode_comm) );

  /* mapping of the global ranks (of the window communicator */
  MPI_Group_size( (*win)->win_onnode_group, &((*win)->onnode_size) );

  temp = _foMPI_ALLOC( (*win)->onnode_size * sizeof(int));
  assert( temp != NULL );

  (*win)->onnode_ranks = _foMPI_ALLOC( (*win)->onnode_size * sizeof(int));
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
    _foMPI_FREE( (*win)->onnode_ranks );
  } else {
    (*win)->onnode_lower_bound = -1;
    (*win)->onnode_upper_bound = -1;
  }

  //NOTIFICATION QUEUE
  /*init data structure and export*/
  int exp_size;
  xpmem_notif_init_queue(*win,(*win)->onnode_size);
  /*export memory and save into the segment descriptor to send to others on-node PEs*/
  	(*win)->xpmem_segdesc.notif_queue = foMPI_export_memory_xpmem((*win)->xpmem_notif_queue, sizeof(fompi_xpmem_notif_queue_t));
  	(*win)->xpmem_segdesc.notif_queue_state = foMPI_export_memory_xpmem((void*)((*win)->xpmem_notif_state_lock),sizeof(fompi_xpmem_notif_state_t) + (*win)->onnode_size * sizeof(lock_flags_t));

  _foMPI_FREE( temp );
#endif
  MPI_Group_free(&group);

  /* allocate the memory for the remote window information */

  _foMPI_ALIGNED_ALLOC(&memptr, (*win)->commsize * sizeof(foMPI_Win_struct_t) )
  (*win)->win_array =  memptr ;
  assert((*win)->win_array != NULL);

  /* set the information for the remote processes */
  if ( (base != NULL) && (size > 0) ) {
    win_info.base = base;
    _foMPI_mem_register( base, (uint64_t) size, &(win_info.seg), *win );

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
  _foMPI_mem_register( *win, (uint64_t) sizeof(foMPI_Win_desc_t), &(win_info.win_ptr_seg), *win );

#ifdef XPMEM
  (*win)->xpmem_segdesc.win_ptr = foMPI_export_memory_xpmem(*win, sizeof(foMPI_Win_desc_t));
#endif

  /* PCSW matching */

  _foMPI_ALIGNED_ALLOC(&memptr, (*win)->commsize * sizeof(uint64_t) )
  win_info.pscw_matching_exposure = memptr;
  assert( win_info.pscw_matching_exposure != NULL );

  _foMPI_ALIGNED_ALLOC(&memptr, (*win)->commsize * sizeof(uint32_t))
  (*win)->pscw_matching_access = memptr ;
  assert( (*win)->pscw_matching_access != NULL );

  for( i=0 ; i<(*win)->commsize ; i++ ){
    win_info.pscw_matching_exposure[i] = 0;
    (*win)->pscw_matching_access[i] = 0;
  }

  _foMPI_mem_register( win_info.pscw_matching_exposure, (uint64_t) (*win)->commsize * sizeof(uint64_t), &(win_info.pscw_matching_exposure_seg), *win );

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
  (*win)->xpmem_array = _foMPI_ALLOC( (*win)->onnode_size * sizeof(fompi_xpmem_addr_t) );
  assert( (*win)->xpmem_array != NULL );
  fompi_xpmem_info_t* xpmem_temp = _foMPI_ALLOC( (*win)->onnode_size * sizeof(fompi_xpmem_info_t) );
 
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
    //notifications
    (*win)->xpmem_array[i].notif_queue = foMPI_map_memory_xpmem( xpmem_temp[i].notif_queue, sizeof(fompi_xpmem_notif_queue_t), &((*win)->xpmem_array[i].notif_queue_apid), &((*win)->xpmem_array[i].notif_queue_offset) );
    (*win)->xpmem_array[i].notif_queue_state = foMPI_map_memory_xpmem( xpmem_temp[i].notif_queue_state, (*win)->onnode_size * sizeof(lock_flags_t), &((*win)->xpmem_array[i].notif_queue_state_apid), &((*win)->xpmem_array[i].notif_queue_state_offset) );

  }

  _foMPI_FREE( xpmem_temp );
#endif

  return MPI_SUCCESS;
}
