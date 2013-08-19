#include <mpi.h>
#include "libtopodisc.h"
#include <stdlib.h>

int main() {

  MPI_Init(NULL, NULL);

  MPI_Comm comm=MPI_COMM_WORLD;
  int r;
  MPI_Comm_rank(comm, &r);

  int num=0;
  TPD_Get_num_hier_levels(&num, comm);

  MPI_Comm *comms=(MPI_Comm*)malloc(sizeof(MPI_Comm)*num);
  TPD_Get_hier_levels(comms, num, comm);

  printf("[%i] found %i hierarchy levels\n", r, num);
  for(int i=0; i<num; ++i) {
    int ri, pi;
    MPI_Comm_rank(comms[i], &ri);
    MPI_Comm_size(comms[i], &pi);
    printf("[%i] rank in level %i: %i of %i\n", r, i, ri, pi);
  }
  
  MPI_Finalize();

}

