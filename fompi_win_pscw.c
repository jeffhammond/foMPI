#include <assert.h>
//#include <intrinsics.h> 
#include <stdlib.h>

#include "fompi.h"

int foMPI_Win_start(MPI_Group group, int assert, foMPI_Win win) {
#ifdef PAPI
  timing_record( -1 );
#endif
  
  int* temp = NULL;
  MPI_Group group_win_comm;
  int size, i;

  win->pscw_access_group_ranks = NULL;

  /* get the real ranks of the processes in the group */
  MPI_Group_size( group, &size );
  win->pscw_access_group_size = size;

  win->pscw_access_group_ranks = malloc( win->pscw_access_group_size * sizeof(int) );
  assert( win->pscw_access_group_ranks != NULL );

  temp = malloc( win->pscw_access_group_size * sizeof(int) );
  assert( temp != NULL );
  for( i=0 ; i<win->pscw_access_group_size ; i++ ) {
    temp[i] = i;
  }
  MPI_Comm_group(win->comm, &group_win_comm);
  MPI_Group_translate_ranks(group, win->pscw_access_group_size, &temp[0], group_win_comm, &(win->pscw_access_group_ranks[0]));

  free(temp);
  MPI_Group_free(&group_win_comm);

  /* begin the access epoch */
  /* begin a new epoch with the ranks in group */
  for( i=0 ; i<win->pscw_access_group_size ; i++ ) {
    win->pscw_matching_access[win->pscw_access_group_ranks[i]]++;
  }

  uint64_t* pscw_matching_exposure_ptr;
   /* wait for every process in the access group to post MPI_Win_post */
  if ( win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC ) {
    for( i=0 ; i<win->pscw_access_group_size ; i++ ) {
      /* local busy waiting */
      pscw_matching_exposure_ptr = &(win->dynamic_list[win->commrank].pscw_matching_exposure[win->pscw_access_group_ranks[i]]);
      while( win->pscw_matching_access[win->pscw_access_group_ranks[i]] != *(volatile uint64_t*)pscw_matching_exposure_ptr ) {}
    }
  } else {
    for( i=0 ; i<win->pscw_access_group_size ; i++ ) {
      /* local busy waiting */
      pscw_matching_exposure_ptr = &(win->win_array[win->commrank].pscw_matching_exposure[win->pscw_access_group_ranks[i]]);
      while( win->pscw_matching_access[win->pscw_access_group_ranks[i]] != *(volatile uint64_t*)pscw_matching_exposure_ptr ) {}
    }
  } 

#ifdef PAPI
  timing_record( 11 );
#endif

  return MPI_SUCCESS;   
}

int foMPI_Win_complete(foMPI_Win win) {
#ifdef PAPI
  timing_record( -1 );
#endif

  dmapp_return_t status;
  int i;

  /* completion */
  if (win->nbi_counter > 0) {
    status = dmapp_gsync_wait();
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
    win->nbi_counter = 0;
  }

  if ( win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC ) {
    for( i=0 ; i<win->pscw_access_group_size ; i++ ) {

      status = dmapp_aadd_qw_nbi( (void*) &(win->dynamic_list[win->pscw_access_group_ranks[i]].win_ptr->global_counter), &(win->dynamic_list[win->pscw_access_group_ranks[i]].win_ptr_seg),
        win->group_ranks[win->pscw_access_group_ranks[i]] , 1 );
      _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);

    }
  } else {
#ifdef XPMEM
    /* XPMEM completion */
    __sync_synchronize();
#endif
    for( i=0 ; i<win->pscw_access_group_size ; i++ ) {
#ifdef XPMEM
      int target_rank = win->pscw_access_group_ranks[i];
      int local_rank = foMPI_onnode_rank_global_to_local( target_rank, win );
      if ( local_rank != -1 ) {
        __sync_fetch_and_add (&(win->xpmem_array[local_rank].win_ptr->xpmem_global_counter), 1);
      } else {
        status = dmapp_aadd_qw_nbi( (void*) &(win->win_array[target_rank].win_ptr->global_counter), &(win->win_array[target_rank].win_ptr_seg), win->group_ranks[target_rank] , 1 );
        _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
      }
#else 
      status = dmapp_aadd_qw_nbi( (void*) &(win->win_array[win->pscw_access_group_ranks[i]].win_ptr->global_counter), &(win->win_array[win->pscw_access_group_ranks[i]].win_ptr_seg), 
                 win->group_ranks[win->pscw_access_group_ranks[i]] , 1 );
      _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
#endif
    }
  }

  free(win->pscw_access_group_ranks);

#ifdef PAPI
  timing_record( 12 );
#endif
  return MPI_SUCCESS;
}

int foMPI_Win_post(MPI_Group group, int assert, foMPI_Win win) {
#ifdef PAPI
  timing_record( -1 );
#endif

  dmapp_return_t status;

  int* temp = NULL;
  int* temp_exposure_ranks = NULL;
  MPI_Group group_win_comm;
  int size, i;

  /* get the real ranks of the processes in the group */
  MPI_Group_size( group, &size );
  win->pscw_exposure_group_size = size;

  temp = malloc( win->pscw_exposure_group_size * sizeof(int) );
  assert( temp != NULL ); /* TODO: assure all of this assert kinds with a NULL intialization */
  temp_exposure_ranks = malloc( win->pscw_exposure_group_size * sizeof(int) );
  assert( temp_exposure_ranks != NULL );
  for( i=0 ; i<win->pscw_exposure_group_size ; i++ ) {
    temp[i] = i;
  }
  MPI_Comm_group(win->comm, &group_win_comm);
  MPI_Group_translate_ranks(group, win->pscw_exposure_group_size, &temp[0], group_win_comm, &temp_exposure_ranks[0]);

  free(temp);
  MPI_Group_free(&group_win_comm);

  /* set the counter for the Win_wait */
  win->global_counter = 0;
#ifdef XPMEM
  win->xpmem_global_counter = 0;
#endif

  /* begin a new exposure epoch with the ranks in group */
  if ( win->create_flavor == foMPI_WIN_FLAVOR_DYNAMIC ) {
    for( i=0 ; i<win->pscw_exposure_group_size ; i++ ) {

      status = dmapp_aadd_qw_nbi( &(win->dynamic_list[temp_exposure_ranks[i]].pscw_matching_exposure[win->commrank]), &(win->dynamic_list[temp_exposure_ranks[i]].pscw_matching_exposure_seg),
        win->group_ranks[temp_exposure_ranks[i]], (int64_t) 1 );
      _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);

    }
  } else {
    for( i=0 ; i<win->pscw_exposure_group_size ; i++ ) {
#ifdef XPMEM
      int target_rank = temp_exposure_ranks[i];
      int local_rank = foMPI_onnode_rank_global_to_local( target_rank, win );
      if ( local_rank != -1 ) {
        __sync_fetch_and_add ((volatile int64_t*) &(win->xpmem_array[local_rank].pscw_matching_exposure[win->commrank]), 1);
      } else {
        status = dmapp_aadd_qw_nbi( &(win->win_array[target_rank].pscw_matching_exposure[win->commrank]), &(win->win_array[target_rank].pscw_matching_exposure_seg),
                   win->group_ranks[target_rank], (int64_t) 1 );
        _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
      }
#else
      status = dmapp_aadd_qw_nbi( &(win->win_array[temp_exposure_ranks[i]].pscw_matching_exposure[win->commrank]), &(win->win_array[temp_exposure_ranks[i]].pscw_matching_exposure_seg),
                 win->group_ranks[temp_exposure_ranks[i]], (int64_t) 1 );
      _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
#endif
    }
  }

  free( temp_exposure_ranks );

#ifdef PAPI
  timing_record( 13 );
#endif
  return MPI_SUCCESS;
}

int foMPI_Win_wait(foMPI_Win win) {
#ifdef PAPI
  timing_record( -1 );
#endif
  
  /* local busy waiting */
#ifdef XPMEM
  while( ((volatile uint64_t) win->global_counter + (volatile char) win->xpmem_global_counter ) != win->pscw_exposure_group_size) {}
#else
  while( ((volatile uint64_t) win->global_counter ) != win->pscw_exposure_group_size) {}
#endif

#ifdef PAPI
  timing_record( 14 );
#endif
  return MPI_SUCCESS;
}

int foMPI_Win_test(foMPI_Win win, int *flag) {

#ifdef XPMEM
  if ( (win->global_counter+win->xpmem_global_counter) != win->pscw_exposure_group_size ) {
#else
  if ( win->global_counter != win->pscw_exposure_group_size ) {
#endif
    *flag = 0;
  } else {
    *flag = 1;
  }

  return MPI_SUCCESS;
}
