#include "fompi.h"

int foMPI_Win_get_group(foMPI_Win win, MPI_Group *group) {

  MPI_Comm_group( win->comm, group );

  return MPI_SUCCESS;
}
