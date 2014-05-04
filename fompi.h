// Copyright (c) 2012 The Trustees of University of Illinois. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mpi.h"
#include "fompi_internal.h"

/* constants for win_create_flavor */
#define foMPI_WIN_FLAVOR_CREATE     0
#define foMPI_WIN_FLAVOR_ALLOCATE   1
#define foMPI_WIN_FLAVOR_DYNAMIC    2

/* constants for win_model */
#define foMPI_WIN_SEPARATE          0
#define foMPI_WIN_UNIFIED           1

/* asserts for one-sided communication */
#define foMPI_MODE_NOCHECK       1024
#define foMPI_MODE_NOSTORE       2048
#define foMPI_MODE_NOPUT         4096
#define foMPI_MODE_NOPRECEDE     8192
#define foMPI_MODE_NOSUCCEED    16384

/* error codes */
#define foMPI_ERROR_RMA_NO_SYNC               1
#define foMPI_ERROR_RMA_DATATYPE_MISMATCH     2
#define foMPI_ERROR_RMA_NO_LOCK_HOLDING       3
#define foMPI_ERROR_RMA_SYNC_CONFLICT         4
#define foMPI_ERROR_RMA_WRITE_SHARED_CONFLICT 5
#define foMPI_ERROR_RMA_WIN_MEM_NOT_FOUND     6
#define foMPI_OP_NOT_SUPPORTED                7
#define foMPI_DATATYPE_NOT_SUPPORTED          8
#define foMPI_NAME_ABBREVIATED                9

/* constants for foMPI_OP */
/* TODO: not real MPI_Op objects */
/* the numbers should be contiguous, since some sanity tests rely on that fact */
#define foMPI_SUM                   0
#define foMPI_PROD                  1
#define foMPI_MAX                   2
#define foMPI_MIN                   3
#define foMPI_LAND                  4
#define foMPI_LOR                   5
#define foMPI_LXOR                  6
#define foMPI_BAND                  7
#define foMPI_BOR                   8
#define foMPI_BXOR                  9
#define foMPI_MAXLOC               10
#define foMPI_MINLOC               11
#define foMPI_REPLACE              12
#define foMPI_NO_OP                13

/* constants for lock type */
#define foMPI_LOCK_SHARED       1
#define foMPI_LOCK_EXCLUSIVE    2

/* constants for attributes */
#define foMPI_WIN_BASE          0
#define foMPI_WIN_SIZE          1
#define foMPI_WIN_DISP_UNIT     2
#define foMPI_WIN_CREATE_FLAVOR 3
#define foMPI_WIN_MODEL         4

#define foMPI_REQUEST_NULL NULL
#define foMPI_UNDEFINED 17383
#define foMPI_WIN_NULL NULL

#ifdef __cplusplus
 extern "C" {
#endif

/* function prototypes */
int foMPI_Win_create(void *base, MPI_Aint size, int disp_unit, MPI_Info info, MPI_Comm comm, foMPI_Win *win);
int foMPI_Win_allocate(MPI_Aint size, int disp_unit, MPI_Info info, MPI_Comm comm, void *baseptr, foMPI_Win *win);
int foMPI_Win_create_dynamic(MPI_Info info, MPI_Comm comm, foMPI_Win *win);
int foMPI_Win_free(foMPI_Win *win);

int foMPI_Win_attach(foMPI_Win win, void *base, MPI_Aint size);
int foMPI_Win_detach(foMPI_Win win, const void *base);

int foMPI_Win_fence(int assert, foMPI_Win win);

int foMPI_Win_start(MPI_Group group, int assert, foMPI_Win win);
int foMPI_Win_complete(foMPI_Win win);
int foMPI_Win_post(MPI_Group group, int assert, foMPI_Win win);
int foMPI_Win_wait(foMPI_Win win);
int foMPI_Win_test(foMPI_Win win, int *flag);

int foMPI_Win_lock(int lock_type, int rank, int assert, foMPI_Win win);
int foMPI_Win_unlock(int rank, foMPI_Win win);
int foMPI_Win_lock_all(int assert, foMPI_Win win);
int foMPI_Win_unlock_all(foMPI_Win win);

int foMPI_Win_flush(int rank, foMPI_Win win);
int foMPI_Win_flush_all(foMPI_Win win);
int foMPI_Win_flush_local(int rank, foMPI_Win win);
int foMPI_Win_flush_local_all(foMPI_Win win);
int foMPI_Win_sync(foMPI_Win win);

int foMPI_Put(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype, foMPI_Win win);
int foMPI_Get(void *origin_addr, int origin_count, MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype, foMPI_Win win);
int foMPI_Accumulate(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp, int target_count,
                     MPI_Datatype target_datatype, foMPI_Op op, foMPI_Win win);
int foMPI_Get_accumulate(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype, void *result_addr, int result_count, MPI_Datatype result_datatype,
                         int target_rank, MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype, foMPI_Op op, foMPI_Win win);

int foMPI_Fetch_and_op(const void *origin_addr, void *result_addr, MPI_Datatype datatype, int target_rank, MPI_Aint target_disp, foMPI_Op op, foMPI_Win win);
int foMPI_Compare_and_swap(const void *origin_addr, const void *compare_addr, void *result_addr, MPI_Datatype datatype, int target_rank, MPI_Aint target_disp, foMPI_Win win);

int foMPI_Rput(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp, int target_count,
               MPI_Datatype target_datatype, foMPI_Win win, foMPI_Request *request);
int foMPI_Rget(void *origin_addr, int origin_count, MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp, int target_count,
               MPI_Datatype target_datatype, foMPI_Win win, foMPI_Request *request);
int foMPI_Raccumulate(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp, int target_count,
                      MPI_Datatype target_datatype, foMPI_Op op, foMPI_Win win, foMPI_Request *request);
int foMPI_Rget_accumulate(const void *origin_addr, int origin_count, MPI_Datatype origin_datatype, void *result_addr, int result_count, MPI_Datatype result_datatype,
                          int target_rank, MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype, foMPI_Op op, foMPI_Win win, foMPI_Request *request);

int foMPI_Win_get_group(foMPI_Win win, MPI_Group *group);

int foMPI_Win_set_name(foMPI_Win win, const char *win_name);
int foMPI_Win_get_name(foMPI_Win win, char *win_name, int *resultlen);

int foMPI_Win_create_keyval(MPI_Win_copy_attr_function *win_copy_attr_fn, MPI_Win_delete_attr_function *win_delete_attr_fn, int *win_keyval, void *extra_state);
int foMPI_Win_free_keyval(int *win_keyval);
int foMPI_Win_set_attr(foMPI_Win win, int win_keyval, void *attribute_val);
int foMPI_Win_get_attr(foMPI_Win win, int win_keyval, void *attribute_val, int *flag);
int foMPI_Win_delete_attr(foMPI_Win win, int win_keyval);

/* TODO: remove this */
int foMPI_Init( int *argc, char ***argv );
int foMPI_Finalize();
int foMPI_Wait(foMPI_Request *request, MPI_Status *status);
int foMPI_Test(foMPI_Request *request, int *flag, MPI_Status *status);
int foMPI_Type_free( MPI_Datatype *datatype);

#ifdef __cplusplus
  }
#endif
