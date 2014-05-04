/* -*- Mode: C; c-basic-offset:4 ; -*- */
#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "findcliques.h"

/*@
  MPE_Create2dMeshComm - Create a communicator for a 2-d mesh

  Input Parameters:
+ comm - Communicator of processes
- isRowMajor - If true, mesh is row major, else column major

  Output Parameters:
+ meshComm - Communicator with ranks ordered in a mesh (see below)
. meshDims - Dimensions of mesh (number of processes in each dimension)
- meshCoords - Coordinates of this process in the meshComm

  Notes:
  This routine returns a communicator where processes on the same node
  are mapped to a local rectangular mesh; this will often be a good 
  choice for neighbor communication on a regular mesh.

  To convert from the coordinates in the mesh to a rank in this communicator,
  use
      rank = coord[0] + coord[1] * meshCoords[0]

  @*/
int MPE_Create2dMeshComm( MPI_Comm comm, int isRowMajor, 
			  MPI_Comm *meshComm, int meshDims[2], 
			  int meshCoords[2] )
{
    int err, cliqueNum, cliqueSize, cliqueRanks[64], worldSize, rank, rinc;
    int tmparr[2], nodeDims[2], gridDims[2], i0, j0, i1, j1, ig, jg;

    /* Use the nodes name to find the SMPs (cliques) */
    err = MPE_FindCliqueFromNodename( comm, 16, 64, &cliqueNum, &cliqueSize,
				      cliqueRanks );
    if (err) {
	return err;
    }
    /* Is there a consistent node size? */
    tmparr[0] = cliqueSize;
    tmparr[1] = -cliqueSize;
    MPI_Allreduce( MPI_IN_PLACE, tmparr, 2, MPI_INT, MPI_MAX, comm );
    if (tmparr[0] != cliqueSize || tmparr[1] != -cliqueSize) {
	/* Not all nodes have the same number of processes, required for
	   this routine.  Note that this will fail for all processes in
	   COMM_WORLD unless they all have the same size, since if 
	   any is different, either the min or max of the clique size
	   will be different */
	return 1;
    }
    /* Consistency check: */
    if (cliqueSize <= 0) {
	return 1;
    }
    /* Given the size of a clique, determine the layout on the node */
    nodeDims[0] = 0; nodeDims[1] = 0;
    MPI_Dims_create( cliqueSize, 2, nodeDims );

    /* Determine the layout of the grid of nodes (does not assume 
       any physical topology */
    MPI_Comm_size( comm, &worldSize );
    gridDims[0] = 0; gridDims[1] = 0;
    MPI_Dims_create( worldSize / cliqueSize, 2, gridDims );

    /* Coordinate to rank mapping: r = I + J*P, where I = (0,...,P-1) */
    /* Coordinates within the mesh: (i0,j0) */
    /* Note maximum cliqueNum is worldSize-1 */
    j0 = cliqueNum / gridDims[0];
    i0 = cliqueNum % gridDims[0];

    /* Coordinates with the mesh (i1,j1) */
    /* Needs a consistent rank in clique (rinc) - find my rank in the list of 
       processes in this clique */
    MPI_Comm_rank( comm, &rank );
    for (rinc=0; rinc<cliqueSize; rinc++) {
	if (rank == cliqueRanks[rinc]) break;
    }
    /* Consistency check */
    if (rinc == cliqueSize) {
	fprintf( stderr, "Did not find my process in the list of this clique\n" );
	return 1;
    }
    j1 = rinc / nodeDims[0];
    i1 = rinc % nodeDims[0];
    
    /* Compute coordinates in the grid */
    ig = i0 * nodeDims[0] + i1;
    jg = j0 * nodeDims[1] + j1;

    /* Compute new rank mapping based on this */
    rank = ig + jg * gridDims[0]*nodeDims[0];

    MPI_Comm_split( comm, 0, rank, meshComm );

    /* Return the grid parameters */
    meshDims[0]   = gridDims[0] * nodeDims[0];
    meshDims[1]   = gridDims[1] * nodeDims[1];
    meshCoords[0] = ig;
    meshCoords[1] = jg;

    if (0) {
	printf( "%d: (%d,%d) in [%d,%d]\n", rank, meshCoords[0], 
		meshCoords[1], meshDims[0], meshDims[1] );
    }

    /* Consistency check.  For now, print out because these should never
       occur */
    if (meshCoords[0] >= meshDims[0]) {
	fprintf( stderr, "meshcoord[0] = %d >= meshdims[0] = %d\n",
		 meshCoords[0], meshDims[0] );
	err = 2;
    }
    if (meshCoords[1] >= meshDims[1]) {
	fprintf( stderr, "meshcoord[1] = %d >= meshdims[1] = %d\n",
		 meshCoords[1], meshDims[1] );
	err = 3;
    }
    if (meshDims[0]*meshDims[1] != worldSize) {
	fprintf( stderr, "Size of mesh [%d,%d] not equal to comm world (%d)\n",
		 meshDims[0], meshDims[1], worldSize );
	err = 4;
    }

    /* We computed the row-major decomposition.  If column major desired, 
       transpose it */
    if (!isRowMajor) { 
	int t;
	t = meshDims[0]; meshDims[0] = meshDims[1]; meshDims[1] = t;
	t = meshCoords[0]; meshCoords[0] = meshCoords[1]; meshCoords[1] = t; 
    }

    return err;
}
