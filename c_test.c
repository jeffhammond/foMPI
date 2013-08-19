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

  base[0] = 1;
  base[1] = 0;

  foMPI_Win_fence( MPI_INFO_NULL, win );

  foMPI_Accumulate( &base[0], 1, MPI_UINT64_T, 0, 1, 1, MPI_UINT64_T, foMPI_SUM, win );
  
  foMPI_Win_fence( MPI_INFO_NULL, win );

  if ( commrank == 0 ) {
    if ( base[1] == commsize ) {
      printf("Reached expected sum of commsize\nThank you for using foMPI.\n");
    } else {
      printf("Accumulated %i instead of %i.\n", base[1], commsize);
    }
  }

  foMPI_Win_free( &win );

  foMPI_Finalize();

  return 0;
}
