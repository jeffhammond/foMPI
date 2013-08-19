#ifndef FINDCLIQUES_H_INCLUDED
#define FINDCLIQUES_H_INCLUDED
#if defined(__cplusplus)
extern "C" {
#endif

int MPE_FindCliqueFromName( const char *name, MPI_Comm comm, int hintSize,
			    int maxSize, int *cliqueNum, int *cliqueSize, 
			    int cliqueRanks[] );
int MPE_FindCliqueFromNodename( MPI_Comm comm, int hintSize, int maxSize, 
				int *cliqueNum, int *cliqueSize,
				int cliqueRanks[] );
int MPE_FindCliqueFromNameStats( int *ho, double *t );

int MPE_CreateSMPComm( MPI_Comm comm, 
		       MPI_Comm *localComm, MPI_Comm *rootComm, int * );
int MPE_CreateRoundRobinComm( MPI_Comm comm, MPI_Comm *rrcomm, int *nnodes );
int MPE_CreateConseqComm( MPI_Comm comm, MPI_Comm *rrcomm, int *nnodes );
int MPE_Create2dMeshComm( MPI_Comm comm, int isRowMajor, 
			  MPI_Comm *meshComm, int meshDims[2], 
			  int meshCoords[2] );

#if defined(__cplusplus)
}
#endif

#endif
