
// for now it's an MPI-only library
#include <mpi.h>
#include <unistd.h>
#include "findcliques.h"

int TPD_Get_num_hier_levels(int* num, MPI_Comm comm) {

  // for now only node-level communicators
  *num = 1;


  return MPI_SUCCESS;
}

int TPD_Get_hier_levels(MPI_Comm *comms, int max_num, MPI_Comm comm) {

  char myname[1024];
  int err = gethostname( myname, sizeof(myname) );
  if (err) {
    perror( "gethostname: " );
    MPI_Abort( MPI_COMM_WORLD, 1 );
  }

  int cliqueNum, cliqueRanks[64], cliqueSize;
  MPE_FindCliqueFromName( myname, comm, 16, 64, &cliqueNum,
                &cliqueSize, cliqueRanks );

  int rank;
  MPI_Comm nameComm; // communicator with the same hostnames
  MPI_Comm_rank( comm, &rank );
  MPI_Comm_split( comm, cliqueNum, rank, &nameComm );

  if(max_num > 0) comms[0] = nameComm;

  return MPI_SUCCESS;
}


