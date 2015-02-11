// Copyright (c) 2012 The Trustees of University of Illinois. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "fompi.h"

/* TODO: I assume that baseptr is a pointer to a pointer: Is this correct? */
int foMPI_Win_allocate(MPI_Aint size, int disp_unit, MPI_Info info, MPI_Comm comm, void *baseptr, foMPI_Win *win) {

  int result;

  if (baseptr != NULL) {
#ifdef SYMHEAP
    int rank;
    int pagesize;
    void* ptr;
    char test, result;

    MPI_Comm_rank( comm, &rank );
    pagesize = getpagesize();

/*
    if ( rank == 0 ) {
      *(void **)baseptr = mmap( 0, (size_t) size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0 );
      ptr = *(void **)baseptr;
    }

    MPI_Bcast( &ptr, sizeof(void*), MPI_BYTE, 0, comm );

    if ( rank != 0 ) {
      *(void **)baseptr = mmap( ptr, (size_t) size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0 );
    }

    if( ptr == *(void **)baseptr ) {
      test = 1;
    } else {
      test = 0;
    }

    MPI_Allreduce( &test, &result, 1, MPI_CHAR, MPI_MIN, comm );

    if ( result == 1 ) {
      printf("%i: We have a symmetric heap at %p.\n", rank, *(void **)baseptr );
    } else {
      printf("%i: We have no symmetric heap. My allocation begins at %p.\n", rank, *(void **)baseptr );
    }
*/

    //MPI_Abort( comm, -1 );
#else
	void * memptr;
	_foMPI_ALIGNED_ALLOC(&memptr, size)
	*((void **)baseptr) = memptr;
    assert( *(void **)baseptr != NULL );
#endif
    result = foMPI_Win_create(*(void **)baseptr, size, disp_unit, info, comm, win);
  } else {
    result = foMPI_Win_create(NULL, size, disp_unit, info, comm, win);
  }

  if ( result == MPI_SUCCESS ) {
    (*win)->create_flavor = foMPI_WIN_FLAVOR_ALLOCATE;
  }

  return result;
}
