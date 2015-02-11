// Copyright (c) 2012 The Trustees of University of Illinois. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fompi.h"

int main( int argc, char *argv[] ) {

  foMPI_Win win;
  int status;

  uint64_t* base;
  uint64_t result;

  int commsize;
  int commrank;

  int ranges[1][3];
  MPI_Group group, newgroup;

  foMPI_Init( &argc, &argv );

  foMPI_Win_allocate( 2*sizeof(uint64_t), sizeof(uint64_t), MPI_INFO_NULL, MPI_COMM_WORLD, &base, &win);

  MPI_Comm_size( MPI_COMM_WORLD, &commsize );
  MPI_Comm_rank( MPI_COMM_WORLD, &commrank );
  foMPI_Request req;
  int tag =0;
  foMPI_Notify_init(win, foMPI_ANY_SOURCE, tag, commsize - 1, &req);
  
  foMPI_Win_fence( MPI_INFO_NULL, win );

  if(commrank!=0){
    foMPI_Put_notify(base,0,MPI_DOUBLE, 0, 0 ,0, MPI_DOUBLE, win, tag);
  } else {
    foMPI_Start(&req);
    foMPI_Wait(&req, MPI_STATUS_IGNORE);
  }

  if ( commrank == 0 ) {
      printf("Motification Received. Thank you for using foMPI-NA.\n");
  }

  foMPI_Win_free( &win );

  foMPI_Finalize();

  return 0;
}
