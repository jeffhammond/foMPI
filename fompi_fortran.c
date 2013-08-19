#include <string.h>

#include "fompi.h"

// this should really be dome by autoconf but I don't have the time right now
#define F77_FUNC_(name,NAME) name ## _

foMPI_Win *Fortran_Windows;
int Fortran_win_sz;
fortran_free_mem_t *Fortran_free_win;

foMPI_Request *Fortran_Requests;
int Fortran_req_sz;
fortran_free_mem_t *Fortran_free_req;

/* some helper functions */

static inline void insert_free_elem( int index, fortran_free_mem_t **list_head ) {

  fortran_free_mem_t *entry;

  entry = malloc( sizeof(fortran_free_mem_t) );
  entry->index = index;
  entry->next = *list_head;
  *list_head = entry;

}

static inline int get_winidx() {

  // if we don't have a window table yet ... allocate it
  int winidx;
  int i;
  if(Fortran_Windows == NULL) {
    winidx=0;
    Fortran_Windows = (foMPI_Win*)malloc(sizeof(foMPI_Win) * Fortran_win_sz);
    for (i=0; i<Fortran_win_sz; ++i) {
      insert_free_elem( i, &Fortran_free_win ); 
    }
  } else {
    winidx = get_free_elem( &Fortran_free_win );
    if(winidx == -1) { 
      winidx = Fortran_win_sz;
      Fortran_win_sz += 100;
      Fortran_Windows = realloc( Fortran_Windows, Fortran_win_sz * sizeof(foMPI_Win) );
      for (i = winidx /* old Fortran_win_sz value */ ; i<Fortran_win_sz ; ++i) {
        insert_free_elem( i, &Fortran_free_win ); 
      }
    }
  }

  return winidx;
}

static inline int get_reqidx() {
  
  // if we don't have a request table yet ... allocate it
  int reqidx;
  int i;
  if(Fortran_Requests == NULL) {
    reqidx = 0;
    Fortran_Requests = (foMPI_Request*)malloc(sizeof(foMPI_Request)*Fortran_req_sz);
    for (i=0; i<Fortran_req_sz; ++i) {
      insert_free_elem( i, &Fortran_free_req );
    }
  } else {
    reqidx = get_free_elem( &Fortran_free_req );
    if(reqidx == -1) {
      reqidx = Fortran_req_sz;
      Fortran_req_sz += 100;
      Fortran_Requests = realloc( Fortran_Requests, Fortran_req_sz * sizeof(foMPI_Request) );
      for (i = reqidx /* old Fortran_req_sz value */; i<Fortran_req_sz; ++i) {
        insert_free_elem( i, &Fortran_free_req );
      }
    }
  } 

  return reqidx;
}

/* now for the real Fortran binding code */

/* TODO: remove after MPI bug on test machine is fixed */
void F77_FUNC_(fompi_get_address,FOMPI_GET_ADDRESS)(void *location, MPI_Aint *address, int *ierr);
void F77_FUNC_(fompi_get_address,FOMPI_GET_ADDRESS)(void *location, MPI_Aint *address, int *ierr) {
  
  *ierr = MPI_Get_address( location, address );

}

//=============================================================================
//============================ Window Creation ================================
//=============================================================================

void F77_FUNC_(fompi_win_create,FOMPI_WIN_CREATE)(void *base, MPI_Aint *size, int *disp_unit, int *info, int *comm, int *win, int *ierr);
void F77_FUNC_(fompi_win_create,FOMPI_WIN_CREATE)(void *base, MPI_Aint *size, int *disp_unit, int *info, int *icomm, int *win, int *ierr) {
	
  *win = get_winidx();

  MPI_Comm comm;
  comm = MPI_Comm_f2c(*icomm);
  MPI_Info c_info = MPI_Info_f2c( *info );

  // this is also very suboptimal
  *ierr = foMPI_Win_create(base, *size, *disp_unit, c_info, comm, &Fortran_Windows[*win]);
}

void F77_FUNC_(fompi_win_allocate,FOMPI_WIN_ALLOCATE)(MPI_Aint *size, int *disp_unit, int *info, int *comm, MPI_Aint *baseptr, int *win, int *ierr);
void F77_FUNC_(fompi_win_allocate,FOMPI_WIN_ALLOCATE)(MPI_Aint *size, int *disp_unit, int *info, int *comm, MPI_Aint *baseptr, int *win, int *ierr) {
 
  *win = get_winidx();

  void* ptr;
  MPI_Info c_info = MPI_Info_f2c( *info );
  MPI_Comm c_comm = MPI_Comm_f2c( *comm );

  *ierr = foMPI_Win_allocate( *size, *disp_unit, c_info, c_comm, &ptr, &Fortran_Windows[*win]);

  *baseptr = (MPI_Aint) ptr;
}

void F77_FUNC_(fompi_win_allocate_cptr,FOMPI_WIN_ALLOCATE_CPTR)(MPI_Aint *size, int *disp_unit, int *info, int *comm, void **baseptr, int *win, int *ierr);
void F77_FUNC_(fompi_win_allocate_cptr,FOMPI_WIN_ALLOCATE_CPTR)(MPI_Aint *size, int *disp_unit, int *info, int *comm, void **baseptr, int *win, int *ierr) {

  *win = get_winidx();
  
  MPI_Info c_info = MPI_Info_f2c( *info );
  MPI_Comm c_comm = MPI_Comm_f2c( *comm );

  *ierr = foMPI_Win_allocate( *size, *disp_unit, c_info, c_comm, baseptr, &Fortran_Windows[*win]);
}

void F77_FUNC_(fompi_win_create_dynamic,FOMPI_WIN_CREATE_DYNAMIC)(int *info, int *comm, int *win, int *ierr);
void F77_FUNC_(fompi_win_create_dynamic,FOMPI_WIN_CREATE_DYNAMIC)(int *info, int *icomm, int *win, int *ierr) {

  *win = get_winidx();
	
  MPI_Comm comm;
  comm = MPI_Comm_f2c(*icomm);
  MPI_Info c_info = MPI_Info_f2c( *info );

  // this is also very suboptimal
  *ierr = foMPI_Win_create_dynamic(c_info, comm, &Fortran_Windows[*win]);
}

void F77_FUNC_(fompi_win_free,FOMPI_WIN_FREE)(int *win, int *ierr);
void F77_FUNC_(fompi_win_free,FOMPI_WIN_FREE)(int *win, int *ierr) {

  *ierr = foMPI_Win_free(&Fortran_Windows[*win]);
  // Fortran_Windows[*win] = foMPI_WIN_NULL; already done by foMPI_Win_free

  *win = -1; // constant fompi_win_null in fortran
}


void F77_FUNC_(fompi_win_attach,FOMPI_WIN_ATTACH)(int *win, void *base, MPI_Aint *size, int *ierr);
void F77_FUNC_(fompi_win_attach,FOMPI_WIN_ATTACH)(int *win, void *base, MPI_Aint *size, int *ierr) {

  *ierr = foMPI_Win_attach( Fortran_Windows[*win], base, *size );

}

void F77_FUNC_(fompi_win_detach,FOMPI_WIN_DETACH)(int *win, void *base, int *ierr);
void F77_FUNC_(fompi_win_detach,FOMPI_WIN_DETACH)(int *win, void *base, int *ierr) {

  *ierr = foMPI_Win_detach( Fortran_Windows[*win], base );

}

//=============================================================================
//========================== Global Synchronization ===========================
//=============================================================================

void F77_FUNC_(fompi_win_fence,FOMPI_WIN_FENCE)(int *assert, int *win, int *ierr);
void F77_FUNC_(fompi_win_fence,FOMPI_WIN_FENCE)(int *assert, int *win, int *ierr) {
 
  *ierr = foMPI_Win_fence(*assert, Fortran_Windows[*win]);
 
}

//=============================================================================
//==================== General Active Target Synchronization ==================
//=============================================================================


void F77_FUNC_(fompi_win_start,FOMPI_WIN_START)(int *group, int *assert, int *win, int *ierr);
void F77_FUNC_(fompi_win_start,FOMPI_WIN_START)(int *group, int *assert, int *win, int *ierr) {

  MPI_Group c_group = MPI_Group_f2c( *group );  

  *ierr = foMPI_Win_start( c_group, *assert, Fortran_Windows[*win] );

}

void F77_FUNC_(fompi_win_complete,FOMPI_WIN_COMPLETE)(int *win, int *ierr);
void F77_FUNC_(fompi_win_complete,FOMPI_WIN_COMPLETE)(int *win, int *ierr) {

  *ierr = foMPI_Win_complete( Fortran_Windows[*win] );

}

void F77_FUNC_(fompi_win_post,FOMPI_WIN_POST)(int* group, int *assert, int *win, int *ierr);
void F77_FUNC_(fompi_win_post,FOMPI_WIN_POST)(int* group, int *assert, int *win, int *ierr) {

  MPI_Group c_group = MPI_Group_f2c( *group );

  *ierr = foMPI_Win_post( c_group, *assert, Fortran_Windows[*win] );

}

void F77_FUNC_(fompi_win_wait,FOMPI_WIN_WAIT)(int *win, int *ierr);
void F77_FUNC_(fompi_win_wait,FOMPI_WIN_WAIT)(int *win, int *ierr) {

  *ierr = foMPI_Win_wait( Fortran_Windows[*win] );

}

void F77_FUNC_(fompi_win_test,FOMPI_WIN_TEST)(int *win, int *flag, int *ierr);
void F77_FUNC_(fompi_win_test,FOMPI_WIN_TEST)(int *win, int *flag, int *ierr) {

  *ierr = foMPI_Win_test( Fortran_Windows[*win], flag );

}

//=============================================================================
//======================= Passive Target Synchronization ======================
//=============================================================================

void F77_FUNC_(fompi_win_lock,FOMPI_WIN_LOCK)(int *type, int *rank, int *assert, int *win, int *ierr);
void F77_FUNC_(fompi_win_lock,FOMPI_WIN_LOCK)(int *type, int *rank, int *assert, int *win, int *ierr) {
 
  *ierr = foMPI_Win_lock(*type, *rank, *assert, Fortran_Windows[*win]);
 
}

void F77_FUNC_(fompi_win_unlock,FOMPI_WIN_UNLOCK)(int *rank, int *win, int *ierr);
void F77_FUNC_(fompi_win_unlock,FOMPI_WIN_UNLOCK)(int *rank, int *win, int *ierr) {
 
  *ierr = foMPI_Win_unlock(*rank, Fortran_Windows[*win]);
 
}

void F77_FUNC_(fompi_win_lock_all,FOMPI_WIN_LOCK_ALL)(int *assert, int *win, int *ierr);
void F77_FUNC_(fompi_win_lock_all,FOMPI_WIN_LOCK_ALL)(int *assert, int *win, int *ierr) {
  
  *ierr = foMPI_Win_lock_all( *assert, Fortran_Windows[*win] );

}

void F77_FUNC_(fompi_win_unlock_all,FOMPI_WIN_UNLOCK_ALL)(int *win, int *ierr);
void F77_FUNC_(fompi_win_unlock_all,FOMPI_WIN_UNLOCK_ALL)(int *win, int *ierr) {

  *ierr = foMPI_Win_unlock_all( Fortran_Windows[*win] );

}


void F77_FUNC_(fompi_win_flush,FOMPI_WIN_FLUSH)( int *rank, int *win, int *ierr );
void F77_FUNC_(fompi_win_flush,FOMPI_WIN_FLUSH)( int *rank, int *win, int *ierr ) {

  *ierr = foMPI_Win_flush( *rank, Fortran_Windows[*win] );

}

void F77_FUNC_(fompi_win_flush_all,FOMPI_WIN_FLUSH_ALL)( int *win, int *ierr );
void F77_FUNC_(fompi_win_flush_all,FOMPI_WIN_FLUSH_ALL)( int *win, int *ierr ) {

  *ierr = foMPI_Win_flush_all( Fortran_Windows[*win] );

}

void F77_FUNC_(fompi_win_flush_local,FOMPI_WIN_FLUSH_LOCAL)( int *rank, int *win, int *ierr );
void F77_FUNC_(fompi_win_flush_local,FOMPI_WIN_FLUSH_LOCAL)( int *rank, int *win, int *ierr ) {

  *ierr = foMPI_Win_flush_local( *rank, Fortran_Windows[*win] );

}

void F77_FUNC_(fompi_win_flush_local_all,FOMPI_WIN_FLUSH_LOCAL_ALL)( int *win, int *ierr );
void F77_FUNC_(fompi_win_flush_local_all,FOMPI_WIN_FLUSH_LOCAL_ALL)( int *win, int *ierr ) {

  *ierr = foMPI_Win_flush_local_all( Fortran_Windows[*win] );

}

void F77_FUNC_(fompi_win_sync,FOMPI_WIN_SYNC)( int *win, int *ierr );
void F77_FUNC_(fompi_win_sync,FOMPI_WIN_SYNC)( int *win, int *ierr ) {

  *ierr = foMPI_Win_sync( Fortran_Windows[*win] );

}

//=============================================================================
//======================= Bulk Completed Communication ========================
//=============================================================================

void F77_FUNC_(fompi_put,FOMPI_PUT)(void *origin_addr, int *origin_count, int *origin_datatype, int *target_rank, MPI_Aint *target_disp, int *target_count, int *target_datatype, int *win, int *ierr);
void F77_FUNC_(fompi_put,FOMPI_PUT)(void *origin_addr, int *origin_count, int *origin_datatype, int *target_rank, MPI_Aint *target_disp, int *target_count, int *target_datatype, int *win, int *ierr){

  MPI_Datatype otype, ttype;
  otype = MPI_Type_f2c(*origin_datatype);
  ttype = MPI_Type_f2c(*target_datatype);

  *ierr = foMPI_Put(origin_addr, *origin_count, otype, *target_rank, *target_disp, *target_count, ttype, Fortran_Windows[*win]);

}

void F77_FUNC_(fompi_get,FOMPI_GET)(void *origin_addr, int *origin_count, int *origin_datatype, int *target_rank, MPI_Aint *target_disp, int *target_count, int *target_datatype, int *win, int *ierr);
void F77_FUNC_(fompi_get,FOMPI_GET)(void *origin_addr, int *origin_count, int *origin_datatype, int *target_rank, MPI_Aint *target_disp, int *target_count, int *target_datatype, int *win, int *ierr){

  MPI_Datatype otype, ttype;
  otype = MPI_Type_f2c(*origin_datatype);
  ttype = MPI_Type_f2c(*target_datatype);

  *ierr = foMPI_Get(origin_addr, *origin_count, otype, *target_rank, *target_disp, *target_count, ttype, Fortran_Windows[*win]);

}

void F77_FUNC_(fompi_accumulate,FOMPI_ACCUMULATE)(void *origin_addr, int *origin_count, int *origin_datatype, int *target_rank,
  MPI_Aint *target_disp, int *target_count, int *target_datatype, int *op, int *win, int *ierr);
void F77_FUNC_(fompi_accumulate,FOMPI_ACCUMULATE)(void *origin_addr, int *origin_count, int *origin_datatype, int *target_rank, 
  MPI_Aint *target_disp, int *target_count, int *target_datatype, int *op, int *win, int *ierr){

  MPI_Datatype otype, ttype;
  otype = MPI_Type_f2c(*origin_datatype);
  ttype = MPI_Type_f2c(*target_datatype);
  foMPI_Op mop;
  mop = *op;//MPI_Op_f2c(*op); not for now, since we use foMPI prefix operators

  *ierr = foMPI_Accumulate(origin_addr, *origin_count, otype, *target_rank, *target_disp, *target_count, ttype, mop, Fortran_Windows[*win]);

  foMPI_Win_flush(*target_rank, Fortran_Windows[*win]); /* TODO: Why is this here? */

}

void F77_FUNC_(fompi_get_accumulate,FOMPI_GET_ACCUMULATE)(void *origin_addr, int *origin_count, int *origin_datatype, void *result_addr, int *result_count, int *result_datatype,
  int *target_rank, MPI_Aint *target_disp, int *target_count, int *target_datatype, int *op, int *win, int *ierr);
void F77_FUNC_(fompi_get_accumulate,FOMPI_GET_ACCUMULATE)(void *origin_addr, int *origin_count, int *origin_datatype, void *result_addr, int *result_count, int *result_datatype,
  int *target_rank, MPI_Aint *target_disp, int *target_count, int *target_datatype, int *op, int *win, int *ierr) {

  MPI_Datatype otype, rtype, ttype;
  otype = MPI_Type_f2c(*origin_datatype);
  rtype = MPI_Type_f2c(*result_datatype);
  ttype = MPI_Type_f2c(*target_datatype);
  foMPI_Op mop;
  mop = *op;//MPI_Op_f2c(*op); not for now, since we use foMPI prefix operators

  *ierr = foMPI_Get_accumulate(origin_addr, *origin_count, otype, result_addr, *result_count, rtype, *target_rank, *target_disp, *target_count, ttype, mop, Fortran_Windows[*win]);

}


void F77_FUNC_(fompi_compare_and_swap,FOMPI_COMPARE_AND_SWAP)(void *origin_addr, void *compare_addr, void *result_addr, int *datatype, int *target_rank, MPI_Aint *target_disp, int *win, int *ierr);
void F77_FUNC_(fompi_compare_and_swap,FOMPI_COMPARE_AND_SWAP)(void *origin_addr, void *compare_addr, void *result_addr, int *datatype, int *target_rank, MPI_Aint *target_disp, int *win, int *ierr) {

  MPI_Datatype dtype;
  dtype = MPI_Type_f2c(*datatype);

  *ierr = foMPI_Compare_and_swap(origin_addr, compare_addr, result_addr, dtype, *target_rank, *target_disp, Fortran_Windows[*win]);

}

void F77_FUNC_(fompi_fetch_and_op,FOMPI_FETCH_AND_OP)(void *origin_addr, void *result_addr, int *datatype, int *target_rank, MPI_Aint *target_disp, int *op, int *win, int *ierr);
void F77_FUNC_(fompi_fetch_and_op,FOMPI_FETCH_AND_OP)(void *origin_addr, void *result_addr, int *datatype, int *target_rank, MPI_Aint *target_disp, int *op, int *win, int *ierr) {

  MPI_Datatype dtype;
  dtype = MPI_Type_f2c(*datatype);
  foMPI_Op mop;
  mop = *op;//MPI_Op_f2c(*op); not for now, since we use foMPI prefix operators

  *ierr = foMPI_Fetch_and_op(origin_addr, result_addr, dtype, *target_rank, *target_disp, mop, Fortran_Windows[*win]);

}

//=============================================================================
//======================= Request Based Communication =========================
//=============================================================================

void F77_FUNC_(fompi_rput,FOMPI_RPUT)(void *origin_addr, int *origin_count, int *origin_datatype, int *target_rank, MPI_Aint *target_disp, int *target_count, int *target_datatype, int *win, int *request, int *ierr);
void F77_FUNC_(fompi_rput,FOMPI_RPUT)(void *origin_addr, int *origin_count, int *origin_datatype, int *target_rank, MPI_Aint *target_disp, int *target_count, int *target_datatype, int *win, int *request, int *ierr){

  MPI_Datatype otype, ttype;
  otype = MPI_Type_f2c(*origin_datatype);
  ttype = MPI_Type_f2c(*target_datatype);

  *request = get_reqidx();

  *ierr = foMPI_Rput(origin_addr, *origin_count, otype, *target_rank, *target_disp, *target_count, ttype, Fortran_Windows[*win], &Fortran_Requests[*request]);

}

void F77_FUNC_(fompi_rget,FOMPI_RGET)(void *origin_addr, int *origin_count, int *origin_datatype, int *target_rank, MPI_Aint *target_disp, int *target_count, int *target_datatype, int *win, int *request, int *ierr);
void F77_FUNC_(fompi_rget,FOMPI_RGET)(void *origin_addr, int *origin_count, int *origin_datatype, int *target_rank, MPI_Aint *target_disp, int *target_count, int *target_datatype, int *win, int *request, int *ierr){

  MPI_Datatype otype, ttype;
  otype = MPI_Type_f2c(*origin_datatype);
  ttype = MPI_Type_f2c(*target_datatype);

  *request = get_reqidx();

  *ierr = foMPI_Rget(origin_addr, *origin_count, otype, *target_rank, *target_disp, *target_count, ttype, Fortran_Windows[*win], &Fortran_Requests[*request]);

}

void F77_FUNC_(fompi_raccumulate,FOMPI_RACCUMULATE)(void *origin_addr, int *origin_count, int *origin_datatype, int *target_rank,
  MPI_Aint *target_disp, int *target_count, int *target_datatype, int *op, int *win, int *request, int *ierr);
void F77_FUNC_(fompi_raccumulate,FOMPI_RACCUMULATE)(void *origin_addr, int *origin_count, int *origin_datatype, int *target_rank, 
  MPI_Aint *target_disp, int *target_count, int *target_datatype, int *op, int *win, int *request, int *ierr){

  MPI_Datatype otype, ttype;
  otype = MPI_Type_f2c(*origin_datatype);
  ttype = MPI_Type_f2c(*target_datatype);
  foMPI_Op mop;
  mop = *op;//MPI_Op_f2c(*op); not for now, since we use foMPI prefix operators

  *request = get_reqidx();

  *ierr = foMPI_Raccumulate(origin_addr, *origin_count, otype, *target_rank, *target_disp, *target_count, ttype, mop, Fortran_Windows[*win], &Fortran_Requests[*request]);

}

void F77_FUNC_(fompi_rget_accumulate,FOMPI_RGET_ACCUMULATE)(void *origin_addr, int *origin_count, int *origin_datatype, void *result_addr, int *result_count, int *result_datatype,
  int *target_rank, MPI_Aint *target_disp, int *target_count, int *target_datatype, int *op, int *win, int *request, int *ierr);
void F77_FUNC_(fompi_rget_accumulate,FOMPI_RGET_ACCUMULATE)(void *origin_addr, int *origin_count, int *origin_datatype, void *result_addr, int *result_count, int *result_datatype,
  int *target_rank, MPI_Aint *target_disp, int *target_count, int *target_datatype, int *op, int *win, int *request, int *ierr) {

  MPI_Datatype otype, rtype, ttype;
  otype = MPI_Type_f2c(*origin_datatype);
  rtype = MPI_Type_f2c(*result_datatype);
  ttype = MPI_Type_f2c(*target_datatype);
  foMPI_Op mop;
  mop = *op;//MPI_Op_f2c(*op); not for now, since we use foMPI prefix operators

  *request = get_reqidx();
  
  *ierr = foMPI_Rget_accumulate(origin_addr, *origin_count, otype, result_addr, *result_count, rtype, *target_rank, *target_disp, *target_count, ttype, mop, Fortran_Windows[*win], &Fortran_Requests[*request]);

}

//=============================================================================
//============================= Window Attributes =============================
//=============================================================================

void F77_FUNC_(fompi_win_get_group, FOMPI_WIN_GET_GROUP)( int *win, int *group, int *ierr );
void F77_FUNC_(fompi_win_get_group, FOMPI_WIN_GET_GROUP)( int *win, int *group, int *ierr ) {

  MPI_Group c_group;

  *ierr = foMPI_Win_get_group( Fortran_Windows[*win], &c_group );

  *group = MPI_Group_c2f( c_group );

  MPI_Group_free( &c_group );
}


void F77_FUNC_(fompi_win_set_name,FOMPI_WIN_SET_NAME)(int *win, char *win_name, int *ierr, int len);
void F77_FUNC_(fompi_win_set_name,FOMPI_WIN_SET_NAME)(int *win, char *iwin_name, int *ierr, int len) {

  char* c_win_name = malloc( (len+1) * sizeof(char) );
  memcpy( c_win_name, iwin_name, len );
  c_win_name[len] = '\0';

  int i;
  /* remove padding */
  for (i=len-1; i>=0; i--) {
    if (c_win_name[i] == ' ') {
      c_win_name[i] = '\0';
    } else {
      break;
    }
  }

  *ierr = foMPI_Win_set_name( Fortran_Windows[*win], c_win_name );

  free( c_win_name );

}

void F77_FUNC_(fompi_win_get_name,FOMPI_WIN_GET_NAME)(int *win, char *win_name, int *resultlen, int *ierr, int len);
void F77_FUNC_(fompi_win_get_name,FOMPI_WIN_GET_NAME)(int *win, char *iwin_name, int *resultlen, int *ierr, int len) {

  char* c_win_name = malloc( (len+1) * sizeof(char) );

  *ierr = foMPI_Win_get_name( Fortran_Windows[*win], c_win_name, resultlen );

  int minimum;
  if (*resultlen > len) {
    minimum = len;
  } else {
    minimum = *resultlen;
  }
  memcpy( iwin_name, c_win_name, minimum );

  int i;
  /* add padding */
  for( i = minimum ; i<len ; i++ ) {
    iwin_name[i] = ' ';
  }

  free( c_win_name );

}


void F77_FUNC_(fompi_win_create_keyval,FOMPI_WIN_CREATE_KEYVAL)(MPI_Win_copy_attr_function *win_copy_attr_fn, MPI_Win_delete_attr_function *win_delete_attr_fn,
  int *win_keyval, MPI_Aint *extra_state, int *ierr);
void F77_FUNC_(fompi_win_create_keyval,FOMPI_WIN_CREATE_KEYVAL)(MPI_Win_copy_attr_function *win_copy_attr_fn, MPI_Win_delete_attr_function *win_delete_attr_fn,
  int *win_keyval, MPI_Aint *extra_state, int *ierr) {

  *ierr = foMPI_Win_create_keyval( win_copy_attr_fn, win_delete_attr_fn, win_keyval, extra_state );

}

void F77_FUNC_(fompi_win_free_keyval,FOMPI_WIN_FREE_KEYVAL)(int *win_keyval, int *ierr) {

  *ierr = foMPI_Win_free_keyval( win_keyval );

}

void F77_FUNC_(fompi_win_set_attr,FOMPI_WIN_SET_ATTR)(int* win, int *win_keyval, MPI_Aint *attribute_val, int *ierr);
void F77_FUNC_(fompi_win_set_attr,FOMPI_WIN_SET_ATTR)(int* win, int *win_keyval, MPI_Aint *attribute_val, int *ierr) {

  *ierr = foMPI_Win_set_attr( Fortran_Windows[*win], *win_keyval, (void*) *attribute_val );

}

void F77_FUNC_(fompi_win_get_attr,FOMPI_WIN_GET_ATTR)(int *win, int *win_keyval, MPI_Aint *attribute_val, int *flag, int *ierr);
void F77_FUNC_(fompi_win_get_attr,FOMPI_WIN_GET_ATTR)(int *win, int *win_keyval, MPI_Aint *attribute_val, int *flag, int *ierr) {

  void *addr;
  int int_attribute;

  switch (*win_keyval) {
    case foMPI_WIN_BASE:
      *ierr = foMPI_Win_get_attr( Fortran_Windows[*win], *win_keyval, &addr, flag );
      *attribute_val = (MPI_Aint) addr;
      break;
    case foMPI_WIN_SIZE:
      *ierr = foMPI_Win_get_attr( Fortran_Windows[*win], *win_keyval, attribute_val, flag );
      break;
    default:
      *ierr = foMPI_Win_get_attr( Fortran_Windows[*win], *win_keyval, &int_attribute, flag );
      *attribute_val = int_attribute;
      break;
  }

}

void F77_FUNC_(fompi_win_delete_attr,FOMPI_WIN_DELETE_ATTR)(int *win, int *win_keyval, int *ierr);
void F77_FUNC_(fompi_win_delete_attr,FOMPI_WIN_DELETE_ATTR)(int *win, int *win_keyval, int *ierr) {

  *ierr = foMPI_Win_delete_attr( Fortran_Windows[*win], *win_keyval );

}

//=============================================================================
//============================ Overloaded Functions ===========================
//=============================================================================

// TODO: this should also be done more carefully (see libnbc's handling) -- no time
void F77_FUNC_(fompi_init,FOMPI_INIT)(int *ierr);
void F77_FUNC_(fompi_init,FOMPI_INIT)(int *ierr) {
	
  *ierr = foMPI_Init( NULL, NULL);
}

void F77_FUNC_(fompi_finalize,FOMPI_FINALIZE)(int *ierr);
void F77_FUNC_(fompi_finalize,FOMPI_FINALIZE)(int *ierr) {
	
  *ierr = foMPI_Finalize();
}

/* we can't actual recognize MPI_STATUS_IGNORE, but since MPI_STATUS_IGNORE in Fortran is also an integer array of MPI_STATUS_SIZE, we fill it anyway */
void F77_FUNC_(fompi_wait,FOMPI_WAIT)(int *request, int *status, int *ierr);
void F77_FUNC_(fompi_wait,FOMPI_WAIT)(int *request, int *status, int *ierr) {

  MPI_Status c_status;

  *ierr = foMPI_Wait( &Fortran_Requests[*request], &c_status );

  if (*ierr == MPI_SUCCESS) {

    //according to mpif.h: PARAMETER (MPI_SOURCE=3,MPI_TAG=4,MPI_ERROR=5)
    //really ugly
    status[2] = c_status.MPI_SOURCE;
    status[3] = c_status.MPI_TAG;
    status[4] = c_status.MPI_ERROR;

    //Fortan_Requests[*request] = foMPI_REQUEST_NULL; already done by foMPI_Wait

    *request = -1; /* equivalent to fompi_request_null in fompif.h */
  }

}

/* we can't actual recognize MPI_STATUS_IGNORE, but since MPI_STATUS_IGNORE in Fortran is also an integer array of MPI_STATUS_SIZE, we fill it anyway */
void F77_FUNC_(fompi_test,FOMPI_TEST)(int *request, int *flag, int *status, int *ierr);
void F77_FUNC_(fompi_test,FOMPI_TEST)(int *request, int *flag, int *status, int *ierr) {

  MPI_Status c_status;

  *flag = 0;
  
  *ierr = foMPI_Test( &Fortran_Requests[*request], flag, &c_status );

  if( (*ierr == MPI_SUCCESS) && ( *flag != 0 ) ) {

    //according to mpif.h: PARAMETER (MPI_SOURCE=3,MPI_TAG=4,MPI_ERROR=5)
    //really ugly
    status[2] = c_status.MPI_SOURCE;
    status[3] = c_status.MPI_TAG;
    status[4] = c_status.MPI_ERROR;

    //Fortan_Requests[*request] = foMPI_REQUEST_NULL; already done by foMPI_Test
    
    *request = -1; /* equivalent to fompi_request_null in fompif.h */
  }

}

void F77_FUNC_(fompi_type_free,FOMPI_TYPE_FREE)( int *datatype, int *ierr );
void F77_FUNC_(fompi_type_free,FOMPI_TYPE_FREE)( int *datatype, int *ierr ) {

  MPI_Datatype dtype;
  dtype = MPI_Type_f2c(*datatype);

  *ierr = foMPI_Type_free( &dtype );

}
