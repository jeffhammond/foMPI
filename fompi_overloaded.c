#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "fompi.h"

#ifdef XPMEM
  #include "libtopodisc.h"

  MPI_Group onnode_group;
#endif

dmapp_pe_t debug_pe; /* TODO: only debug purpose */

int need_to_mpi_finalize;
int need_to_dmapp_finalize;

/* some helper functions */
static int dcache_init() {
  int i;
 
  dcache_last_entry = 0;
  for ( i=0 ; i<10 ; i++ ) {
    dcache_blocks[i].type = MPI_DATATYPE_NULL;
  }
  dcache_origin_last_entry = 0;
  dcache_target_last_entry = 0;

  return MPI_SUCCESS;
}

static int dcache_clean_up() {
  int i;

  for ( i=0 ; i<10 ; i++ ) {
    if ( dcache_blocks[i].type != MPI_DATATYPE_NULL ) {
      free( dcache_blocks[i].displ );
      free( dcache_blocks[i].blocklen );
    }
  }

  return MPI_SUCCESS;
}

/* the overloaded MPI functions */

int foMPI_Type_free( MPI_Datatype *datatype) {
  int i;

  i = 0;
  do {
    if (dcache_blocks[i].type == *datatype) {
      free( dcache_blocks[i].displ );
      free( dcache_blocks[i].blocklen );
      dcache_blocks[i].type = MPI_DATATYPE_NULL;
    }
  } while ( ++i<10 );
  
  return MPI_Type_free(datatype);
}

int foMPI_Init( int *argc, char ***argv ) {

  dmapp_return_t status;
  dmapp_rma_attrs_ext_t actual_args = {0}, rma_args = {0};
  dmapp_jobinfo_t job;
  int myrank;
  int flag;

  rma_args.put_relaxed_ordering = DMAPP_ROUTING_ADAPTIVE;
  rma_args.get_relaxed_ordering = DMAPP_ROUTING_ADAPTIVE;
  rma_args.max_outstanding_nb = DMAPP_DEF_OUTSTANDING_NB;
  rma_args.offload_threshold = DMAPP_OFFLOAD_THRESHOLD;
  rma_args.max_concurrency = 1;
  rma_args.PI_ordering = DMAPP_PI_ORDERING_RELAXED;
 
  MPI_Initialized( &flag );
  if (flag == 0) {
    MPI_Init( argc, argv );
    need_to_mpi_finalize = 1;
  } else {
    need_to_mpi_finalize = 0;
  }

   status = dmapp_initialized( &flag ); 
  _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);

  if( flag == 0 ) {
    status = dmapp_init_ext(&rma_args, &actual_args);
    assert( status == DMAPP_RC_SUCCESS);
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
    need_to_dmapp_finalize = 1;
  } else {
    status = dmapp_set_rma_attrs_ext(&rma_args, &actual_args);
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
    need_to_dmapp_finalize = 0;
  }

/* TODO: only debug purpose, debug_pe is not needed as a global variable */
  status = dmapp_get_jobinfo(&job);
  _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);

  debug_pe = job.pe; 
/* end only debug purpose */

 /* TODO: translate table if this assumption is not correct */
  MPI_Comm_rank( MPI_COMM_WORLD, &myrank );
  if (myrank != debug_pe) {
    fprintf(stderr, "Error on rank %i in %s line %i: pe rank doesn't match rank %i in MPI_COMM_WORLD.\n", debug_pe, __FILE__, __LINE__, myrank);  
    exit(EXIT_FAILURE);
  }

  /* init datatype block cache */
  dcache_init();

#ifdef XPMEM
  /* create a MPI communicator to identify onnode processes */
  int num;
  TPD_Get_num_hier_levels(&num, MPI_COMM_WORLD);
  assert( num == 1 );

  MPI_Comm onnode_comm;
  TPD_Get_hier_levels(&onnode_comm, num, MPI_COMM_WORLD);

  MPI_Comm_group( onnode_comm, &onnode_group );
#endif

  /* Fortran data init */
  Fortran_Windows = NULL;
  Fortran_Requests = NULL;
  Fortran_win_sz = 100;
  Fortran_req_sz = 100;
  Fortran_free_win = NULL;
  Fortran_free_req = NULL;

#ifdef PAPI
  timing_init();
#endif

  return MPI_SUCCESS;
}

int foMPI_Finalize() {

  dmapp_return_t status;

  /* TODO: Could this be removed? */
  MPI_Barrier(MPI_COMM_WORLD);

#ifdef PAPI
  timing_close();
#endif

  if( need_to_dmapp_finalize == 1) {
    status = dmapp_finalize();
    _check_status(status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
    need_to_dmapp_finalize = 0;
  }

  /* clean up the datatype block cache */
  dcache_clean_up();

  /* clean up Fortran data */
  free( Fortran_Windows );
  free( Fortran_Requests );
  Fortran_win_sz = 0;
  Fortran_req_sz = 0;
  int index;
  do {
    index = get_free_elem( &Fortran_free_win );
  } while (index != -1) ;
  do {
    index = get_free_elem( &Fortran_free_req );
  } while (index != -1) ;

  if (need_to_mpi_finalize == 1) {
    MPI_Finalize();
    need_to_mpi_finalize = 0;
  }

  return MPI_SUCCESS;
}

/* MPI_Status will habe the following properties */
/* MPI_Status.SOURCE = target_rank */
/* MPI_Status.TAG = foMPI_UNDEFINED */
/* MPI_Status.ERROR = foMPI_UNDEFINED, since there is no error checking at the moment */
/* MPI_Status: Elements = target_count, also for the datatype target_datatype is used */
/* MPI_Status: cancelled = false */

/* TODO: the output of the status elements are untested */
/* TODO: not compatible with the other request structures or MPI_Wait */
/* TODO: the error field is not set correctly */

int foMPI_Wait(foMPI_Request *request, MPI_Status *status) {

  dmapp_return_t dmapp_status;

  /* handling of MPI_REQUEST_NULL) */
  if (*request == foMPI_REQUEST_NULL) {
    if (status != MPI_STATUS_IGNORE) {
      status->MPI_SOURCE = MPI_ANY_SOURCE;
      status->MPI_TAG = MPI_ANY_TAG;
      status->MPI_ERROR = MPI_SUCCESS;
      MPI_Status_set_elements(status, MPI_DATATYPE_NULL, 0 );
      MPI_Status_set_cancelled(status, 0);
    }
    return MPI_SUCCESS;
  }

#ifdef XPMEM
  /* completion for XPMEM */
  __sync_synchronize();
#endif
   
  if ((*request)->win->nbi_counter > 0) {
    dmapp_status = dmapp_gsync_wait();
    _check_status(dmapp_status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
    (*request)->win->nbi_counter = 0;
  }

  if (status != MPI_STATUS_IGNORE) {
    status->MPI_SOURCE = (*request)->source;
    status->MPI_TAG = foMPI_UNDEFINED;
    status->MPI_ERROR = foMPI_UNDEFINED;
    MPI_Status_set_elements( status, (*request)->datatype, (*request)->elements );
    MPI_Status_set_cancelled(status, 0);
  }

  /* clean up */
  free( *request );
  *request = foMPI_REQUEST_NULL;

  return MPI_SUCCESS;
}

/* TODO: the output of the status elements are untested */
/* TODO: not compatible with the other request structures or MPI_Wait */
/* TODO: the error field is not set correctly */

/* flag = 1, if all requests are globally visible */
int foMPI_Test(foMPI_Request *request, int *flag, MPI_Status *status) {
  
  dmapp_return_t dmapp_status;
  
  /* handling of MPI_REQUEST_NULL) */
  if (*request == foMPI_REQUEST_NULL) {
    if (status != MPI_STATUS_IGNORE) {
      status->MPI_SOURCE = MPI_ANY_SOURCE;
      status->MPI_TAG = MPI_ANY_TAG;
      status->MPI_ERROR = MPI_SUCCESS;
      MPI_Status_set_elements(status, MPI_DATATYPE_NULL, 0 );
      MPI_Status_set_cancelled(status, 0);
    }
    return MPI_SUCCESS;
  }

#ifdef XPMEM
  /* completion for XPMEM, since there is no way to test */
  __sync_synchronize();
#endif
 
  if ( (*request)->win->nbi_counter == 0 ) {
    *flag = 1;
  } else {
    dmapp_status = dmapp_gsync_test( flag ); /* flag = 1, if all non-blocking implicit requests are globally visible, if not flag = 0 */
    _check_status(dmapp_status, DMAPP_RC_SUCCESS,  (char*) __FILE__, __LINE__);
  }

  if( *flag == 1 ) {
    (*request)->win->nbi_counter = 0;
  
    if (status != MPI_STATUS_IGNORE) {
      status->MPI_SOURCE = (*request)->source;
      status->MPI_TAG = foMPI_UNDEFINED;
      status->MPI_ERROR = foMPI_UNDEFINED;
      MPI_Status_set_elements( status, (*request)->datatype, (*request)->elements );
      MPI_Status_set_cancelled(status, 0);
    }

    /* clean up */
    free( *request );
    *request = foMPI_REQUEST_NULL;
  }

  return MPI_SUCCESS;
}
