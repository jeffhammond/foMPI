
/**
 * query the number of strict hierarchy levels on this process. It may
 * be different on different processes. This function is collective on
 * comm.
 */
int TPD_Get_num_hier_levels(int* num, MPI_Comm comm);

/**
 * return all hierarchy levels as communicators on this process. This
 * function is collective on comm.
 */
int TPD_Get_hier_levels(MPI_Comm *comms, int max_num, MPI_Comm comm);


